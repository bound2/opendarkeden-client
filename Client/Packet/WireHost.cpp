//----------------------------------------------------------------------
// WireHost.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "WireHost.h"
#include "Player.h"
#include "Cpackets/CGSay.h"
#include "DebugLog.h"		// DEBUG_ADD_FORMAT, for the __DEBUG_OUTPUT__ block below

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const WireHost *	Wire::s_pHost = NULL;

//----------------------------------------------------------------------
// The three tuning values, and the defaults ClientConfig's constructor
// sets for them (Client/ClientConfig.cpp).
//----------------------------------------------------------------------
int
Wire::MaxProcessPacket () throw ()
{
	if (s_pHost==NULL || s_pHost->MaxProcessPacket==NULL)
	{
		return WIRE_DEFAULT_MAX_PROCESS_PACKET;
	}

	return s_pHost->MaxProcessPacket();
}

int
Wire::MaxRequestService () throw ()
{
	if (s_pHost==NULL || s_pHost->MaxRequestService==NULL)
	{
		return WIRE_DEFAULT_MAX_REQUEST_SERVICE;
	}

	return s_pHost->MaxRequestService();
}

uint
Wire::ClientCommunicationUDPPort () throw ()
{
	if (s_pHost==NULL || s_pHost->ClientCommunicationUDPPort==NULL)
	{
		return WIRE_DEFAULT_UDP_PORT;
	}

	return s_pHost->ClientCommunicationUDPPort();
}

Player *
Wire::BugReportTarget () throw ()
{
	if (s_pHost==NULL || s_pHost->BugReportTarget==NULL)
	{
		return NULL;
	}

	return s_pHost->BugReportTarget();
}

//----------------------------------------------------------------------
// The encrypt-seed inputs.
//----------------------------------------------------------------------
// Zone 0 and server 0 with no host, which is what the seed would be
// built from before the player is in a zone. There is no better answer
// to invent: a binary with no host has no zone and no account, and the
// one caller runs only after MoveZone/LoadZone (see WireEncryptSeed).
//----------------------------------------------------------------------
ZoneID_t
Wire::EncryptZoneID () throw ()
{
	if (s_pHost==NULL || s_pHost->EncryptZoneID==NULL)
	{
		return 0;
	}

	return s_pHost->EncryptZoneID();
}

int
Wire::EncryptServerID () throw ()
{
	if (s_pHost==NULL || s_pHost->EncryptServerID==NULL)
	{
		return 0;
	}

	return s_pHost->EncryptServerID();
}

bool
Wire::EncryptUsesEnglishSeed () throw ()
{
	if (s_pHost==NULL || s_pHost->EncryptUsesEnglishSeed==NULL)
	{
		return false;
	}

	return s_pHost->EncryptUsesEnglishSeed();
}

//----------------------------------------------------------------------
// The request-service family's seams.
//----------------------------------------------------------------------
// A clock of 0 and "not in the game world" with no host. The second is
// the conservative answer rather than the convenient one:
// RequestClientPlayer throws on a request packet that arrives outside
// the game, so a binary with no host refuses them all rather than
// accepting them all.
//
// The six file-transfer calls answer false, which is what an
// unregistered transfer looks like - a caller asking whether it still
// has one cleans up instead of waiting on a manager that is not there.
//----------------------------------------------------------------------
DWORD
Wire::CurrentTime () throw ()
{
	if (s_pHost==NULL || s_pHost->CurrentTime==NULL)
	{
		return 0;
	}

	return s_pHost->CurrentTime();
}

bool
Wire::InGameMode () throw ()
{
	if (s_pHost==NULL || s_pHost->InGameMode==NULL)
	{
		return false;
	}

	return s_pHost->InGameMode();
}

bool
Wire::ReceiveMyRequest ( const std::string & name , RequestClientPlayer * pPlayer )
{
	if (s_pHost==NULL || s_pHost->ReceiveMyRequest==NULL)
	{
		return false;
	}

	return s_pHost->ReceiveMyRequest(name, pPlayer);
}

bool
Wire::HasMyRequest ( const std::string & name ) throw ()
{
	if (s_pHost==NULL || s_pHost->HasMyRequest==NULL)
	{
		return false;
	}

	return s_pHost->HasMyRequest(name);
}

bool
Wire::RemoveMyRequest ( const std::string & name ) throw ()
{
	if (s_pHost==NULL || s_pHost->RemoveMyRequest==NULL)
	{
		return false;
	}

	return s_pHost->RemoveMyRequest(name);
}

bool
Wire::SendOtherRequest ( const std::string & name , RequestServerPlayer * pPlayer )
{
	if (s_pHost==NULL || s_pHost->SendOtherRequest==NULL)
	{
		return false;
	}

	return s_pHost->SendOtherRequest(name, pPlayer);
}

bool
Wire::HasOtherRequest ( const std::string & name ) throw ()
{
	if (s_pHost==NULL || s_pHost->HasOtherRequest==NULL)
	{
		return false;
	}

	return s_pHost->HasOtherRequest(name);
}

bool
Wire::RemoveOtherRequest ( const std::string & name ) throw ()
{
	if (s_pHost==NULL || s_pHost->RemoveOtherRequest==NULL)
	{
		return false;
	}

	return s_pHost->RemoveOtherRequest(name);
}

//----------------------------------------------------------------------
// The stream cipher's seed.
//----------------------------------------------------------------------
// Preserved from ClientPlayer::setEncryptCode() unchanged, including
// the parentheses, so the two formulas can be compared with the
// server's without reading past a rewrite. What is gone is the
// duplication: the Netmarble, Chinese and default branches were the
// same expression written three times.
//----------------------------------------------------------------------
uchar
WireEncryptSeed ( ZoneID_t zoneID , int serverID , bool bEnglishSeed ) throw ()
{
	if (bEnglishSeed)
	{
		return (uchar)( ( ( ( zoneID ) >> 8 ) ^ ( zoneID ) ) ^ ( ( ( serverID ) + 1 ) * 51 ) );
	}

	return (uchar)( ( ( ( zoneID ) >> 8 ) ^ ( zoneID ) ) ^ ( ( ( serverID ) + 1 ) << 4 ) );
}

//----------------------------------------------------------------------
// Send Bug Report
//----------------------------------------------------------------------
// Moved here verbatim from Client/PacketFunction.cpp, with the
// executable's own connection replaced by the host's target. The
// bound on the format buffer is the one an earlier hardening pass
// left in place; the cut below it is not - that was 100 bytes, short
// of what the packet carries, and is the whole headroom now.
//
// (The global that connection lives in is deliberately not named
// here: R4 greps every line of a library source for g_p* and does not
// skip comments, so writing it down would read as a seam.)
//----------------------------------------------------------------------

namespace {

//----------------------------------------------------------------------
// The chat command the server dispatches a report on, and what a CGSay
// leaves for the report once the prefix has had its bytes.
//----------------------------------------------------------------------
// sizeof counts the NUL, which is not sent, so the prefix is 12 bytes
// of the 128 a CGSay message may hold - 116 for the text.
//
// CGSay::write() THROWS above its cap rather than truncating, so a
// message built any longer than this would not reach the server cut
// short: it would not reach the server at all.
//----------------------------------------------------------------------
const char	BUG_REPORT_PREFIX[]	= "*bug_report ";

const int	BUG_REPORT_PREFIX_LEN	= (int)sizeof(BUG_REPORT_PREFIX) - 1;

const int	BUG_REPORT_TEXT_MAX	= (int)CGSay::MAX_MESSAGE_SIZE - BUG_REPORT_PREFIX_LEN;

static_assert(BUG_REPORT_TEXT_MAX > 0, "the bug report prefix does not fit in a CGSay message");

} // namespace

void
SendBugReport ( const char * bug , ... )
{
	if( bug == NULL )
		return;


	va_list		vl;
	char Buffer[256];

	va_start(vl, bug);
	int written = vsnprintf(Buffer, sizeof(Buffer), bug, vl);
	va_end(vl);

	// vsnprintf NUL terminates within sizeof(Buffer), so a report longer than
	// the buffer is truncated instead of overrunning the stack. That also makes
	// the strlen and the cut below safe, which they were not while vsprintf
	// could already have run past the end. A negative return is an encoding
	// error: nothing usable was produced, so send nothing.
	if (written < 0)
		return;

	// And the cut has to land inside what was formatted.
	static_assert(BUG_REPORT_TEXT_MAX < (int)sizeof(Buffer), "the cut is outside the format buffer");

#ifdef __DEBUG_OUTPUT__
	DEBUG_ADD_FORMAT("[BUG_REPORT] %s",Buffer);
#endif

	int len = strlen(Buffer);

	if( len <= 1 )
		return;

	// Cut where the packet ends, not short of it. This was 100, which
	// spent 16 bytes of every long report on nothing: the four
	// SendBugReport("%s", t.toString().c_str()) sites hand over a
	// message and then one "file:line" frame per __END_CATCH the
	// exception unwound through, and an InvalidProtocolException's
	// message alone is around 55 bytes - so the 16 were most of the
	// room the first frame needed.
	if( len > BUG_REPORT_TEXT_MAX )
		Buffer[BUG_REPORT_TEXT_MAX] = '\0';

	std::string message;

	message = BUG_REPORT_PREFIX;
	message += Buffer;

	CGSay _CGSay;

	_CGSay.setMessage( message );
	_CGSay.setColor( 0 );

	Player * pTarget = Wire::BugReportTarget();

	if( pTarget != NULL )
		pTarget->sendPacket( &_CGSay );

}
