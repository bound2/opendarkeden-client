//----------------------------------------------------------------------
// test_bug_report_message.cpp
//----------------------------------------------------------------------
//
// What a bug report actually reaches the server as.
//
// SendBugReport (Client/Packet/WireHost.cpp) formats its arguments into
// a 256-byte stack buffer, cuts the text, prefixes the chat command the
// server dispatches on, and sends the result as a CGSay. Every one of
// those steps bounds what the server is told, and the cut is the
// tightest of them: CGSay::write() refuses a message over 128 bytes,
// the prefix "*bug_report " spends 12 of those, so 116 is what is left
// for the report itself.
//
// The point of pinning it here rather than reading the constant is that
// the four SendBugReport("%s", t.toString().c_str()) sites hand over a
// Throwable::toString() - a message and then one "file:line" frame per
// __END_CATCH the exception unwound through. A report cut shorter than
// the packet allows loses frames for no reason, which is exactly the
// state fix/stringstream-nul left this in: it made the whole text reach
// the cut, and the cut was 100.
//
// Player's default constructor builds no socket and no streams, and
// sendPacket is virtual, so a subclass that keeps what it was handed
// makes the whole path observable from a test binary - the format, the
// cut, the prefix and the packet. That is the only reason any of this
// is testable: SendBugReport moved into packetwire in task 5.1, and
// WireHost.cpp is a member of tests/arch/packetwire_files.txt.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt), so the
// Packet and stream definitions are identical to the library's.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "Packet.h"
#include "Player.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "WireHost.h"
#include "Cpackets/CGSay.h"

#include <string>

namespace {

//----------------------------------------------------------------------
// The three numbers, spelled out rather than taken from the code under
// test - a test that read CGSay's cap through the same constant
// SendBugReport reads it through could not see either of them move.
//----------------------------------------------------------------------
const std::string	PREFIX		= "*bug_report ";

// CGSay::read() and CGSay::write() both throw above this.
const int		SAY_MESSAGE_MAX	= 128;

// What the prefix leaves of it: 128 - 12.
const int		TEXT_MAX	= 116;

//----------------------------------------------------------------------
// A target that keeps the packet instead of writing a socket.
//----------------------------------------------------------------------
class CapturingTarget : public Player
{
public:

	CapturingTarget ()
	: m_nSent(0), m_Color(0xFFFFFFFF)
	{
	}

	void sendPacket ( Packet * pPacket )
	{
		m_nSent++;

		if (pPacket == NULL || pPacket->getPacketID() != Packet::PACKET_CG_SAY)
			return;

		const CGSay * pSay = static_cast<const CGSay*>(pPacket);

		m_Message	= pSay->getMessage();
		m_Color		= pSay->getColor();
	}

	int		m_nSent;
	uint		m_Color;
	std::string	m_Message;
};

CapturingTarget *	s_pTarget = NULL;

Player *	HostBugReportTarget ()	{ return s_pTarget; }

// Only the one entry is filled: every other accessor answers its
// documented default with a NULL member, and nothing here asks.
const WireHost	s_Host = { NULL, NULL, NULL, HostBugReportTarget,
			NULL, NULL, NULL,
			NULL, NULL,
			NULL, NULL, NULL, NULL, NULL, NULL };

//----------------------------------------------------------------------
// Puts the library back the way every other test expects it, whichever
// way the test leaves.
//----------------------------------------------------------------------
struct NoHost
{
	~NoHost()	{ Wire::SetHost(NULL); s_pTarget = NULL; }
};

//----------------------------------------------------------------------
// A stream over a never-used socket (see packet_stream_access.h).
//----------------------------------------------------------------------
struct OutFixture
{
	Socket			m_Socket;
	SocketOutputStream	m_Stream;

	OutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
	}
};

//----------------------------------------------------------------------
// The report SendBugReport builds for one text, as the target sees it.
//----------------------------------------------------------------------
std::string	ReportFor ( const std::string & text )
{
	// The target outlives the host that names it: declared first, so
	// NoHost's destructor takes the pointer back before the object it
	// points at goes away.
	CapturingTarget	target;
	NoHost		restore;

	s_pTarget = &target;
	Wire::SetHost(&s_Host);

	SendBugReport("%s", text.c_str());

	CHECK_EQ(1, target.m_nSent);
	CHECK_EQ(0, (int)target.m_Color);

	return target.m_Message;
}

//----------------------------------------------------------------------
// Whether a CGSay will actually carry that message. This is the half
// the arithmetic above cannot assert for itself: a report one byte over
// the cap is not truncated by the packet, it is thrown out of
// Player::sendPacket and never reaches the server at all.
//----------------------------------------------------------------------
bool	ACGSayCarries ( const std::string & message )
{
	OutFixture	out;
	CGSay		say;

	say.setColor(0);
	say.setMessage(message);

	try
	{
		say.write(out.m_Stream);
	}
	catch (Throwable&)
	{
		return false;
	}

	return true;
}

//----------------------------------------------------------------------
// A text of n bytes, distinguishable by position so a cut in the wrong
// place reads as the wrong characters rather than as the right ones.
//----------------------------------------------------------------------
std::string	TextOf ( int n )
{
	std::string	text;

	for (int i = 0; i < n; i++)
		text += (char)('a' + (i % 26));

	return text;
}

} // namespace

//----------------------------------------------------------------------
// Nothing worth saying is still not sent
//----------------------------------------------------------------------
TEST(BugReport, AReportOfNoCharactersOrOneIsNotSent)
{
	CapturingTarget	target;
	NoHost		restore;

	// The arithmetic the rest of this file rests on, spelled once.
	CHECK_EQ(12, (int)PREFIX.size());
	CHECK_EQ(TEXT_MAX, SAY_MESSAGE_MAX - (int)PREFIX.size());

	s_pTarget = &target;
	Wire::SetHost(&s_Host);

	// No format at all.
	SendBugReport(NULL);
	CHECK_EQ(0, target.m_nSent);

	// A report that says nothing, and one that says one character: the
	// length test is `<= 1`, so neither survives it.
	SendBugReport("");
	SendBugReport("x");
	CHECK_EQ(0, target.m_nSent);

	// Two characters is a report.
	SendBugReport("xy");
	CHECK_EQ(1, target.m_nSent);
	CHECK(target.m_Message == PREFIX + "xy");
}

//----------------------------------------------------------------------
// A report short enough goes out whole
//----------------------------------------------------------------------
TEST(BugReport, AShortReportIsPrefixedAndNotCut)
{
	const std::string	report = ReportFor("something went wrong");

	CHECK(report == PREFIX + "something went wrong");
	CHECK_EQ((int)PREFIX.size() + 20, (int)report.size());
	CHECK(ACGSayCarries(report));

	// The arguments are formatted, not the format string sent.
	CapturingTarget	target;
	NoHost		restore;

	s_pTarget = &target;
	Wire::SetHost(&s_Host);

	SendBugReport("Exceed PacketID:%d", 4321);

	CHECK_EQ(1, target.m_nSent);
	CHECK(target.m_Message == PREFIX + "Exceed PacketID:4321");
}

//----------------------------------------------------------------------
// The cut is where the packet ends, not before it
//----------------------------------------------------------------------
TEST(BugReport, TheTextIsCutOnlyWhereACGSayMessageEnds)
{
	// One byte under the headroom, exactly on it, and one over.
	const std::string	under	= TextOf(TEXT_MAX - 1);
	const std::string	exact	= TextOf(TEXT_MAX);
	const std::string	over	= TextOf(TEXT_MAX + 1);

	CHECK(ReportFor(under) == PREFIX + under);
	CHECK(ReportFor(exact) == PREFIX + exact);
	CHECK(ReportFor(over) == PREFIX + exact);

	// Which is a full CGSay message, to the byte.
	CHECK_EQ(SAY_MESSAGE_MAX, (int)ReportFor(exact).size());
	CHECK_EQ(SAY_MESSAGE_MAX, (int)ReportFor(over).size());

	// A long report - the shape of a Throwable::toString() carrying a
	// message and several stack frames - keeps the first TEXT_MAX bytes
	// of its text and nothing after them.
	const std::string	report = ReportFor(TextOf(200));

	CHECK_EQ(SAY_MESSAGE_MAX, (int)report.size());
	CHECK(report == PREFIX + TextOf(TEXT_MAX));
	CHECK(report.compare(0, PREFIX.size(), PREFIX) == 0);
}

//----------------------------------------------------------------------
// And never further than the packet reaches
//----------------------------------------------------------------------
TEST(BugReport, NoReportOutgrowsThePacketThatCarriesIt)
{
	// Across the lengths either side of every boundary in the path:
	// the two-character floor, the headroom, the cap itself and the
	// 256-byte buffer SendBugReport formats into.
	const int	lengths[] = { 2, 3, 50, 114, 115, 116, 117, 118,
					127, 128, 129, 200, 254, 255 };

	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
	{
		const std::string	report = ReportFor(TextOf(lengths[i]));

		CHECK(report.size() <= (size_t)SAY_MESSAGE_MAX);
		CHECK(ACGSayCarries(report));
	}
}
