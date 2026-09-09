//----------------------------------------------------------------------
// test_request_client_manager.cpp
//----------------------------------------------------------------------
//
// RequestClientPlayerManager, the last file task 5.1 took into
// packetwire: it opens the peer-to-peer connections (a thread per
// attempt) and decides what the first packet on each one says. That
// first packet is the seam - it names the character the client is
// logged in as, and the whisper variant carries the messages typed at
// the peer while the connection was still being made - and the
// character and the queue are behind WireHost now, so the manager can
// be driven here with a host that answers for both.
//
// What is reachable: the map of open connections (add, find, send,
// disconnect, drop on a dead socket) and ProcessMode, the first-packet
// policy, over a player whose socket was never opened. Nothing here
// connects anywhere: Connect() starts a thread that dials a peer, and
// the one thing tested of it is the refusal that comes before that.
//
// The player's destructor asserts the session has ended, and a failed
// Assert throws AND appends to assertion_failed.log in the working
// directory, so every player this file builds is put into
// CPS_END_SESSION before it is destroyed - by the manager's own
// disconnect path where that is what is being tested, by the fixture
// otherwise.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "packet_stream_access.h"		// EnsureSocketsInitialised, the stream seams
#include "RequestClientPlayerManager.h"
#include "RequestClientPlayer.h"
#include "Rpackets/CRConnect.h"
#include "Rpackets/CRRequest.h"
#include "Rpackets/CRWhisper.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "WireHost.h"

#include <cstring>
#include <list>
#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// The host: a logged-in character and a whisper queue, plus a record
// of which notifications the manager sent.
//----------------------------------------------------------------------
bool				s_InGame	= false;
int				s_MaxService	= 10;
std::string			s_Character	= "Alice";
WorldID_t			s_WorldID	= 2;
Race				s_Race		= RACE_VAMPIRE;

// One peer's queue. The name is the peer the messages wait for, so a
// manager asking about a different peer is told nothing waits.
std::string			s_QueuedFor;
std::list<WHISPER_MESSAGE>	s_Queued;

std::string			s_Notified;

void	Note(const char* pWhich, const std::string& name)
{
	s_Notified += pWhich;
	s_Notified += "(";
	s_Notified += name;
	s_Notified += ") ";
}

bool		HostInGame()			{ return s_InGame; }
int		HostMaxService()		{ return s_MaxService; }
std::string	HostCharacterName()		{ return s_Character; }
WorldID_t	HostCharacterWorldID()		{ return s_WorldID; }
Race		HostCharacterRace()		{ return s_Race; }

bool	HostHasWhisperMessage(const std::string& name)
{
	return name == s_QueuedFor && !s_Queued.empty();
}

const std::list<WHISPER_MESSAGE>*	HostGetWhisperMessages(const std::string& name)
{
	return name == s_QueuedFor ? &s_Queued : NULL;
}

bool	HostRemoveWhisperMessage(const std::string& name)
{
	Note("RemoveWhisper", name);
	if (name != s_QueuedFor)
		return false;
	s_Queued.clear();
	s_QueuedFor.clear();
	return true;
}

void	HostTryToSendWhisperMessage(const std::string& name)	{ Note("TryToSend", name); }
void	HostRemoveRequestUserLater(const std::string& name)	{ Note("RemoveUser", name); }
void	HostRemoveProfileRequire(const std::string& name)	{ Note("RemoveProfile", name); }

const WireHost	s_Host = { NULL, HostMaxService, NULL, NULL,
				NULL, NULL, NULL,
				NULL, HostInGame,
				NULL, NULL, NULL, NULL, NULL, NULL,
				HostCharacterName, HostCharacterWorldID, HostCharacterRace,
				HostHasWhisperMessage, HostGetWhisperMessages, HostRemoveWhisperMessage, HostTryToSendWhisperMessage,
				HostRemoveRequestUserLater, HostRemoveProfileRequire };

// Installs the host for one test and puts the library back afterwards.
struct HostScope
{
	HostScope()
	{
		s_InGame	= true;
		s_MaxService	= 10;
		s_Character	= "Alice";
		s_WorldID	= 2;
		s_Race		= RACE_VAMPIRE;
		s_QueuedFor.clear();
		s_Queued.clear();
		s_Notified.clear();
		Wire::SetHost(&s_Host);
	}

	~HostScope()	{ Wire::SetHost(NULL); }
};

void	Queue(const char* pFor, const char* pMessage, DWORD color)
{
	WHISPER_MESSAGE m;
	m.msg = pMessage;
	m.color = color;
	s_QueuedFor = pFor;
	s_Queued.push_back(m);
}

//----------------------------------------------------------------------
// A request player over a socket that was never opened. sendPacket()
// writes into the output ring and nothing here flushes it, so what the
// manager sent can be read back.
//----------------------------------------------------------------------
class ProbePlayer : public RequestClientPlayer
{
public:
	ProbePlayer(const char* pPeer, REQUEST_CLIENT_MODE mode)
	: RequestClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl())))
	{
		setRequestServerName(pPeer);
		setRequestMode(mode);
		setPlayerStatus(CPS_REQUEST_CLIENT_BEGIN_SESSION);
	}

	std::vector<unsigned char>	Sent() const
	{
		return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream);
	}
};

// Owns a probe the manager never takes, and ends its session before
// the destructor asserts on it.
struct OwnedProbe
{
	ProbePlayer*	p;

	OwnedProbe(const char* pPeer, REQUEST_CLIENT_MODE mode) : p(new ProbePlayer(pPeer, mode)) {}

	~OwnedProbe()
	{
		p->setPlayerStatus(CPS_END_SESSION);
		delete p;
	}
};

//----------------------------------------------------------------------
// Reading back what was sent: the id out of the framing header, the
// body through a fresh input stream into the packet class.
//----------------------------------------------------------------------
PacketID_t	FramedID(const std::vector<unsigned char>& frame)
{
	CHECK(frame.size() >= szPacketHeader);
	PacketID_t id = 0;
	if (frame.size() >= szPacketID)
		std::memcpy(&id, &frame[0], szPacketID);
	return id;
}

PacketSize_t	FramedSize(const std::vector<unsigned char>& frame)
{
	PacketSize_t size = 0;
	if (frame.size() >= szPacketHeader)
		std::memcpy(&size, &frame[szPacketID], szPacketSize);
	return size;
}

template <class PacketT>
void	ReadFramed(PacketT& dst, const std::vector<unsigned char>& frame)
{
	CHECK_EQ((size_t)szPacketHeader + FramedSize(frame), frame.size());
	if (frame.size() < szPacketHeader)
		return;

	EnsureSocketsInitialised();
	Socket			socket(new SocketImpl());
	SocketInputStream	in(&socket, 4096);

	SocketInputStreamTestAccess::Preload(in, &frame[szPacketHeader],
					     (unsigned int)(frame.size() - szPacketHeader));
	dst.read(in);
	CHECK(in.isEmpty());
}

} // namespace

//----------------------------------------------------------------------
// The link proof, which is also the cheapest thing the class does
//----------------------------------------------------------------------
TEST(RequestClientManager, AnEmptyManagerIsBuiltAndReleasedWithoutAHost)
{
	Wire::SetHost(NULL);

	// The constructor initialises a lock and nothing else; Release()
	// walks three empty containers. This is what proves the object is
	// in the library: unlike the request players, whose constructors
	// take a socket, this one can simply be made.
	RequestClientPlayerManager	manager;

	CHECK_EQ(0, manager.GetSize());
	CHECK_EQ(false, manager.HasConnection("nobody"));
	CHECK_EQ(false, manager.HasTryingConnection("nobody"));
	CHECK_EQ(false, manager.SendPacket("nobody", NULL));

	manager.Release();
	CHECK_EQ(0, manager.GetSize());
}

//----------------------------------------------------------------------
// The map of open connections
//----------------------------------------------------------------------
TEST(RequestClientManager, AConnectionIsRefusedOutsideTheGameAndKeptInsideIt)
{
	HostScope	host;
	RequestClientPlayerManager	manager;

	// Outside the game world the manager closes the connection the
	// thread just made rather than keeping it: the peer answered, but
	// there is no character to speak for. The player is deleted on
	// that path - through disconnect(), which ends its session first.
	s_InGame = false;
	CHECK_EQ(false, manager.AddRequestClientPlayer(new ProbePlayer("bob", REQUEST_CLIENT_MODE_NULL)));
	CHECK_EQ(0, manager.GetSize());
	CHECK_EQ(false, manager.HasConnection("bob"));

	// Inside it, the connection is kept under the peer's name and its
	// session begins - which is the state ProcessMode acts on.
	s_InGame = true;
	ProbePlayer* pBob = new ProbePlayer("bob", REQUEST_CLIENT_MODE_NULL);
	pBob->setPlayerStatus(CPS_NONE);

	CHECK_EQ(true, manager.AddRequestClientPlayer(pBob));
	CHECK_EQ(1, manager.GetSize());
	CHECK_EQ(true, manager.HasConnection("bob"));
	CHECK_EQ(false, manager.HasConnection("carol"));
	CHECK_EQ((int)CPS_REQUEST_CLIENT_BEGIN_SESSION, (int)pBob->getPlayerStatus());

	// SendPacket finds the connection by name and queues on it.
	CRRequest	request;
	request.setCode(CR_REQUEST_FILE_PROFILE);
	request.setRequestName("bob");

	CHECK_EQ(false, manager.SendPacket("carol", &request));
	CHECK_EQ(true, manager.SendPacket("bob", &request));
	CHECK_EQ((int)Packet::PACKET_CR_REQUEST, (int)FramedID(pBob->Sent()));

	// The manager owns the player now. Disconnect() erases and deletes
	// it - the session is ended here first only because the player's
	// destructor asserts on it and Disconnect() does not end it (the
	// manager catches the assertion, but it is also written to a file).
	pBob->setPlayerStatus(CPS_END_SESSION);
	manager.Disconnect("bob");
	CHECK_EQ(0, manager.GetSize());
	CHECK_EQ(false, manager.HasConnection("bob"));

	// Disconnecting a name that is not there is not an error.
	manager.Disconnect("bob");
	CHECK_EQ(0, manager.GetSize());
}

TEST(RequestClientManager, UpdateDropsAConnectionWhoseSocketIsDead)
{
	HostScope	host;
	RequestClientPlayerManager	manager;

	// A socket that was never opened is what a dropped peer looks
	// like from here: Update() tests it before anything else and
	// throws, and its own catch disconnects the player - ending the
	// session, so the delete that follows is the designed one - and
	// takes it out of the map. Nothing was sent first.
	ProbePlayer* pBob = new ProbePlayer("bob", REQUEST_CLIENT_MODE_NULL);
	CHECK_EQ(true, manager.AddRequestClientPlayer(pBob));
	CHECK_EQ(1, manager.GetSize());

	manager.Update();

	CHECK_EQ(0, manager.GetSize());
	CHECK_EQ(false, manager.HasConnection("bob"));

	// And an empty manager's Update is a no-op.
	manager.Update();
	CHECK_EQ(0, manager.GetSize());
}

TEST(RequestClientManager, ConnectRefusesOnceTheServiceLimitIsReached)
{
	HostScope	host;
	RequestClientPlayerManager	manager;

	// The limit comes from the host (the config's MAX_REQUEST_SERVICE)
	// and is tested before a connection attempt is recorded or a
	// thread started. At zero, nothing can ever be tried - which is
	// the one path of Connect() that reaches no network.
	s_MaxService = 0;
	manager.Connect("127.0.0.1", "bob", REQUEST_CLIENT_MODE_WHISPER);

	CHECK_EQ(false, manager.HasTryingConnection("bob"));
	CHECK_EQ(false, manager.HasConnection("bob"));
	CHECK_EQ(0, manager.GetSize());
}

//----------------------------------------------------------------------
// ProcessMode: the first packet on a connection
//----------------------------------------------------------------------
TEST(RequestClientManager, APlainConnectionAnnouncesTheCharacter)
{
	HostScope	host;
	RequestClientPlayerManager	manager;
	OwnedProbe	bob("bob", REQUEST_CLIENT_MODE_NULL);

	manager.ProcessMode(bob.p);

	// A CRConnect naming the peer and the logged-in character, and
	// the session waits for the peer's answer.
	std::vector<unsigned char> sent = bob.p->Sent();
	CHECK_EQ((int)Packet::PACKET_CR_CONNECT, (int)FramedID(sent));

	CRConnect	connect;
	ReadFramed(connect, sent);
	CHECK(connect.getRequestServerName() == "bob");
	CHECK(connect.getRequestClientName() == "Alice");
	CHECK_EQ((int)CPS_REQUEST_CLIENT_AFTER_SENDING_CONNECT, (int)bob.p->getPlayerStatus());

	// Sent once: a second turn in that state sends nothing more.
	manager.ProcessMode(bob.p);
	CHECK_EQ(sent.size(), bob.p->Sent().size());
}

TEST(RequestClientManager, AWhisperConnectionCarriesTheQueueThenDropsIt)
{
	HostScope	host;
	RequestClientPlayerManager	manager;
	OwnedProbe	bob("bob", REQUEST_CLIENT_MODE_WHISPER);

	Queue("bob", "hello", 0x11223344);
	Queue("bob", "are you there", 0x55667788);
	s_Character	= "Alice";
	s_Race		= RACE_OUSTERS;
	s_WorldID	= 7;

	manager.ProcessMode(bob.p);

	// One CRWhisper: from the character, to the peer, with every queued
	// message in order and the character's race and world - the
	// three values this slice put behind the host.
	std::vector<unsigned char> sent = bob.p->Sent();
	CHECK_EQ((int)Packet::PACKET_CR_WHISPER, (int)FramedID(sent));

	CRWhisper	whisper;
	ReadFramed(whisper, sent);
	CHECK(whisper.getName() == "Alice");
	CHECK(whisper.getTargetName() == "bob");
	CHECK_EQ((int)RACE_OUSTERS, (int)whisper.getRace());
	CHECK_EQ(7, (int)whisper.getWorldID());
	CHECK_EQ(2, (int)whisper.getMessageSize());

	WHISPER_MESSAGE* pFirst = whisper.popMessage();
	WHISPER_MESSAGE* pSecond = whisper.popMessage();
	CHECK(pFirst->msg == "hello");
	CHECK_EQ(0x11223344, (int)pFirst->color);
	CHECK(pSecond->msg == "are you there");
	CHECK_EQ(0x55667788, (int)pSecond->color);
	delete pFirst;
	delete pSecond;

	// The session is in its normal state, and the queue was told to
	// drop what it held for the peer - after the packet was built from
	// it, since the list is the queue's own.
	CHECK_EQ((int)CPS_REQUEST_CLIENT_NORMAL, (int)bob.p->getPlayerStatus());
	CHECK(s_Notified == "RemoveWhisper(bob) ");
	CHECK(s_Queued.empty());
}

TEST(RequestClientManager, AWhisperConnectionWithNothingQueuedSendsNothing)
{
	HostScope	host;
	RequestClientPlayerManager	manager;
	OwnedProbe	bob("bob", REQUEST_CLIENT_MODE_WHISPER);

	// Something waits, but for somebody else.
	Queue("carol", "not for bob", 1);

	manager.ProcessMode(bob.p);

	CHECK_EQ((size_t)0, bob.p->Sent().size());
	CHECK_EQ((int)CPS_REQUEST_CLIENT_BEGIN_SESSION, (int)bob.p->getPlayerStatus());
	CHECK(s_Notified.empty());
	CHECK_EQ((size_t)1, s_Queued.size());
}

TEST(RequestClientManager, AProfileConnectionAsksForTheProfile)
{
	HostScope	host;
	RequestClientPlayerManager	manager;
	OwnedProbe	bob("bob", REQUEST_CLIENT_MODE_PROFILE);

	manager.ProcessMode(bob.p);

	std::vector<unsigned char> sent = bob.p->Sent();
	CHECK_EQ((int)Packet::PACKET_CR_REQUEST, (int)FramedID(sent));

	CRRequest	request;
	ReadFramed(request, sent);
	CHECK_EQ((int)CR_REQUEST_FILE_PROFILE, (int)request.getCode());
	CHECK(request.getRequestName() == "bob");
	CHECK_EQ((int)CPS_REQUEST_CLIENT_NORMAL, (int)bob.p->getPlayerStatus());

	// Nothing is asked of the whisper queue for a profile.
	CHECK(s_Notified.empty());
}

//----------------------------------------------------------------------
// No host: no character
//----------------------------------------------------------------------
TEST(RequestClientManager, WithNoCharacterTheConnectPacketIsRefusedNotSentNameless)
{
	RequestClientPlayerManager	manager;
	OwnedProbe	bob("bob", REQUEST_CLIENT_MODE_NULL);

	Wire::SetHost(NULL);

	// The behaviour delta this slice records. The old code read the
	// character's name off the login unguarded; with no host the name
	// is empty, CRConnect::write refuses an empty name, and the
	// refusal comes out of ProcessMode as the InvalidProtocolException
	// the manager's Update treats like any other failure on the
	// connection. What is asserted is that nothing nameless reached
	// the wire.
	bool	refused = false;
	try {
		manager.ProcessMode(bob.p);
	} catch (Throwable&) {
		refused = true;
	}

	CHECK_EQ(true, refused);
	CHECK_EQ((size_t)0, bob.p->Sent().size());
}

//----------------------------------------------------------------------
// The packet the whisper branch builds
//----------------------------------------------------------------------
TEST(CRWhisperRace, TheThreePredicatesNameTheThreeRaces)
{
	// Found while reading the packet this file reads back: isSlayer()
	// compared the race against RACE_VAMPIRE, so a vampire's whisper
	// answered both isVampire() and isSlayer() and a slayer's answered
	// neither. Nothing calls the predicates - CRWhisperHandler reads
	// getRace() - which is why it survived; it is fixed because a
	// predicate that lies is worse than one nobody calls yet.
	CRWhisper	whisper;

	whisper.setRace(RACE_SLAYER);
	CHECK_EQ(true, whisper.isSlayer());
	CHECK_EQ(false, whisper.isVampire());
	CHECK_EQ(false, whisper.isOusters());

	whisper.setRace(RACE_VAMPIRE);
	CHECK_EQ(false, whisper.isSlayer());
	CHECK_EQ(true, whisper.isVampire());
	CHECK_EQ(false, whisper.isOusters());

	whisper.setRace(RACE_OUSTERS);
	CHECK_EQ(false, whisper.isSlayer());
	CHECK_EQ(false, whisper.isVampire());
	CHECK_EQ(true, whisper.isOusters());
}
