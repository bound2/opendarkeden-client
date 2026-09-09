//----------------------------------------------------------------------
// WireHost.h
//----------------------------------------------------------------------
//
// What the wire layer needs from the program around it
// (docs/RESTRUCTURING.md task 5.1).
//
// The receive loops and the connection managers under Client/Packet are
// otherwise self-contained, but they read three tuning values out of
// the executable's config and send a bug report through its own
// executable's socket. Both are seams to code the library sits below,
// so the library asks for them instead: the executable fills a WireHost
// in and installs it, exactly as MItemHost and MPriceHost work for
// gamemodel.
//
// Every accessor answers without a host. The defaults are the ones
// ClientConfig's own constructor sets, so a binary that never installs
// a host - a test binary - behaves as a client whose config file was
// missing rather than as one whose tuning is zero.
//
//----------------------------------------------------------------------

#ifndef __WIREHOST_H__
#define __WIREHOST_H__

#include "Types.h"
#include "Types/ZoneTypes.h"
#include "Types/SystemTypes.h"
#include "RaceType.h"
#include "WhisperMessage.h"

#include <list>
#include <string>

class Player;
class RequestClientPlayer;
class RequestServerPlayer;

//----------------------------------------------------------------------
// The answers with no host, which are ClientConfig's own constructor
// values. Named so that the executable's accessors can fall back to
// the same numbers rather than writing them a second time.
//----------------------------------------------------------------------
enum {
	WIRE_DEFAULT_MAX_PROCESS_PACKET		= 11,
	WIRE_DEFAULT_MAX_REQUEST_SERVICE	= 10,
	WIRE_DEFAULT_UDP_PORT			= 9858
};

//----------------------------------------------------------------------
// The host
//----------------------------------------------------------------------
struct WireHost {

	// How many packets one turn of a receive loop handles.
	int	(*MaxProcessPacket)();

	// How many request-service connections may be open at once.
	int	(*MaxRequestService)();

	// The UDP port the client communication manager binds.
	uint	(*ClientCommunicationUDPPort)();

	// Where a report about a malformed packet is sent. NULL while
	// there is no connection, which is not an error - the report is
	// dropped.
	Player*	(*BugReportTarget)();

	// The two values ClientPlayer::setEncryptCode() derives the stream
	// cipher's seed from: the zone the player is in, and the account's
	// server number.
	ZoneID_t	(*EncryptZoneID)();
	int		(*EncryptServerID)();

	// Which of the two seed formulas to use. The English client spaces
	// the server number differently; every other region shares one
	// formula. See WireEncryptSeed below.
	bool		(*EncryptUsesEnglishSeed)();

	//------------------------------------------------------------------
	// The request-service family (docs/RESTRUCTURING.md task 5.1's
	// fourth slice) - the peer-to-peer side, where two clients talk to
	// each other directly to transfer a file or a whisper.
	//------------------------------------------------------------------

	// The clock its timeouts are measured against, in milliseconds.
	DWORD		(*CurrentTime)();

	// Whether the client is in the game world. RequestClientPlayer
	// refuses a request packet that arrives outside it.
	bool		(*InGameMode)();

	// The peer file-transfer manager, which stays executable-side: it
	// draws progress, writes into the profile directory and reads the
	// UI's own state. What the wire layer needs of it is six calls -
	// the request players hand themselves to it when a transfer starts
	// and take themselves back out when the connection ends.
	//
	// Every one answers false with no host, which is what an
	// unregistered transfer looks like, so a receive loop that asks
	// about one cleans up rather than waiting.
	bool		(*ReceiveMyRequest)(const std::string& name, RequestClientPlayer* pPlayer);
	bool		(*HasMyRequest)(const std::string& name);
	bool		(*RemoveMyRequest)(const std::string& name);
	bool		(*SendOtherRequest)(const std::string& name, RequestServerPlayer* pPlayer);
	bool		(*HasOtherRequest)(const std::string& name);
	bool		(*RemoveOtherRequest)(const std::string& name);

	//------------------------------------------------------------------
	// The last holdout (task 5.1's fifth slice):
	// RequestClientPlayerManager, which opens the peer connections and
	// decides what the first packet on each one says. That first packet
	// names the character the client is logged in as, and the whisper
	// variant carries the messages typed at the peer while the
	// connection was still being made - both live in the executable.
	//------------------------------------------------------------------

	// The character the client is logged in as: what a CRConnect and a
	// CRWhisper announce to the peer. With no host there is no
	// character: an empty name, world 0 (UserInformation's own
	// constructor value) and RACE_MAX, which is "no race" rather than
	// the slayer that a zero would silently claim.
	std::string	(*CharacterName)();
	WorldID_t	(*CharacterWorldID)();
	Race		(*CharacterRace)();

	// The whisper queue, which stays executable-side: it is fed by the
	// chat input and drained into the chat history. The manager asks
	// whether anything waits for a peer, takes the list to put on the
	// wire, and drops it once sent. The list stays owned by the queue;
	// NULL means nothing waits. When a connection could not be made,
	// TryToSendWhisperMessage counts the failed attempt against what
	// the queue holds for that peer - after the third, the queue sends
	// it through the game server instead.
	bool		(*HasWhisperMessage)(const std::string& name);
	const std::list<WHISPER_MESSAGE>*	(*GetWhisperMessages)(const std::string& name);
	bool		(*RemoveWhisperMessage)(const std::string& name);
	void		(*TryToSendWhisperMessage)(const std::string& name);

	// The two other managers a failed connection is reported to: the
	// address book of peers whose IP the client asked the server for
	// (RequestUserManager::RemoveRequestUserLater) and the profile
	// requests waiting on a peer (ProfileManager::RemoveRequire).
	void		(*RemoveRequestUserLater)(const std::string& name);
	void		(*RemoveProfileRequire)(const std::string& name);

};

//----------------------------------------------------------------------
// The stream cipher's seed.
//----------------------------------------------------------------------
// Pulled out of ClientPlayer::setEncryptCode() so that it can be
// tested. THIS IS LIVE CODE. The slice that moved it said the opposite
// - that nothing defines __USE_ENCRYPTER__, so the encrypted streams
// are never constructed - and that was wrong: Encrypter.h defines it,
// and ClientPlayer.cpp includes SocketEncryptInputStream.h two lines
// before its first #ifdef on it. tests/unit/test_packet_goldens.cpp had
// the truth written down the whole time ("0 is the plain branch, 1..5
// the __USE_ENCRYPTER__ branch"); the claim was made without checking
// against it. Both reviewers of the following slice found it.
//
// So the seed is on the live connection path: GCUpdateInfoHandler calls
// setEncryptCode() right after MoveZone/LoadZone, and the byte it
// derives has to agree with the server's exactly.
//
// The original wrote four branches - Netmarble, Chinese, English, and a
// default - of which three computed the identical expression. Only the
// English one differs, multiplying the server number by 51 where the
// others shift it left by four. That collapse is behaviour-preserving,
// which a test asserts across every server number a byte can hold
// rather than leaving to this comment.
//----------------------------------------------------------------------
uchar	WireEncryptSeed ( ZoneID_t zoneID , int serverID , bool bEnglishSeed ) noexcept;

//----------------------------------------------------------------------
// The wire layer's view of it
//----------------------------------------------------------------------
class Wire {

public :

	// Installed once at start-up; NULL puts every default back.
	static void	SetHost ( const WireHost * pHost ) noexcept { s_pHost = pHost; }

	static int	MaxProcessPacket ();
	static int	MaxRequestService ();
	static uint	ClientCommunicationUDPPort ();
	static Player *	BugReportTarget ();
	static ZoneID_t	EncryptZoneID ();
	static int	EncryptServerID ();
	static bool	EncryptUsesEnglishSeed ();

	static DWORD	CurrentTime ();
	static bool	InGameMode ();

	// These two are NOT nothrow, and the omission is deliberate. The
	// file-transfer manager behind them reads and writes the peer
	// socket, and throwing is how a transfer ends: RequestFileManager::
	// SendOtherRequest throws ConnectException("No File to Send"), and
	// both reach RequestClientPlayer::readInputStream /
	// RequestServerPlayer::send, which propagate ProtocolException and
	// Error. The call sites sit outside processCommand's try, so the
	// exception unwinds to RequestServerPlayerManager::Update's
	// catch (Throwable&), which disconnects that peer - the designed
	// teardown. A throw() here would make that path undefined under
	// MSVC and std::terminate under C++17 or on clang/gcc.
	static bool	ReceiveMyRequest ( const std::string & name , RequestClientPlayer * pPlayer );
	static bool	SendOtherRequest ( const std::string & name , RequestServerPlayer * pPlayer );

	static bool	HasMyRequest ( const std::string & name );
	static bool	RemoveMyRequest ( const std::string & name );
	static bool	HasOtherRequest ( const std::string & name );
	static bool	RemoveOtherRequest ( const std::string & name );

	// The logged-in character, and the whisper queue and the two
	// managers a failed peer connection is reported to (the last
	// holdout's seams). Nothrow like the six above them is NOT claimed:
	// the queue and the managers lock and allocate.
	static std::string	CharacterName ();
	static WorldID_t	CharacterWorldID ();
	static Race		CharacterRace ();

	static bool	HasWhisperMessage ( const std::string & name );
	static const std::list<WHISPER_MESSAGE> *	GetWhisperMessages ( const std::string & name );
	static bool	RemoveWhisperMessage ( const std::string & name );
	static void	TryToSendWhisperMessage ( const std::string & name );

	static void	RemoveRequestUserLater ( const std::string & name );
	static void	RemoveProfileRequire ( const std::string & name );

private :

	static const WireHost *	s_pHost;

};

//----------------------------------------------------------------------
// Report a malformed packet to the server, as a chat message.
//----------------------------------------------------------------------
// The executable defined this and the wire layer called it, which is a
// seam no include rule and no ratchet over globals can see - only a
// failed link found it. It lives here now, and takes its target from
// the host.
//----------------------------------------------------------------------
void	SendBugReport ( const char * bug , ... );

#endif	// __WIREHOST_H__
