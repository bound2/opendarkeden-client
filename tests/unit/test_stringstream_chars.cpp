//----------------------------------------------------------------------
// test_stringstream_chars.cpp
//----------------------------------------------------------------------
//
// The NUL StringStream appended after every streamed character, the
// three texts it corrupted, and the buffer toString() failed to clear.
//
// This is a second StringStream file beside test_stringstream.cpp, which
// pins the numeric operators' buffer sizing (docs/RESTRUCTURING.md task
// 1.3). It is kept separate because what it pins is not the formatter
// but its reach: the two character overloads, and the diagnostic text
// three consumers build out of them.
//
// The defect: operator<<(char) built a std::string(2, '\0') and wrote
// the character into the first byte only, so the second byte survived
// into the list; operator<<(uchar) did the same, and toString()
// concatenates the list verbatim. Every streamed character therefore
// carried a NUL with it.
//
// What that reached, the server-visible one first:
//
//   Throwable::toString() streams a '\n' between the message and the
//   stack trace, and getStackTrace() streams one after every frame. The
//   four SendBugReport("%s", t.toString().c_str()) sites
//   (Client/GameMain.cpp, Client/Packet/ClientCommunicationManager.cpp)
//   were so handed a *bug_report cut off before the stack trace the
//   report exists to carry. SendBugReport (Client/Packet/WireHost.cpp)
//   then cuts at 100 bytes of its own, so what the server sees is
//   still bounded; the fix makes the whole text reach that cut.
//
//   __assert__ (Client/Packet/PacketAssert.cpp) streams eos first, so
//   assertion_failed.log received a bare newline per failed Assert, and
//   the AssertionError's message was unreadable through c_str().
//
//   Every LOG_ERROR and strstr consumer of toString().c_str() saw only
//   the first line.
//
// The pins below are in two halves: the operators' own contract, and
// the text the consumers build. Each consumer pin asserts the exact
// string plus the two properties that make it usable through a C
// interface at all - no embedded NUL, and a c_str() as long as the
// std::string it came from.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "StringStream.h"
#include "Exception.h"
#include "PacketAssert.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>


namespace {

//----------------------------------------------------------------------
// A failing assertion appends its message to assertion_failed.log in the
// working directory, exactly as it always has. The test that provokes
// one removes the file afterwards, so the ctest directory does not grow
// a line per run.
//----------------------------------------------------------------------
void	RemoveAssertionLog()
{
	std::error_code	Error;
	std::filesystem::remove("assertion_failed.log", Error);
}

//----------------------------------------------------------------------
// The two properties a diagnostic string needs to survive being handed
// to a C interface - which is what every consumer of these texts does,
// through c_str().
//----------------------------------------------------------------------
bool	IsWholeThroughCStr(const std::string& text)
{
	return text.find('\0') == std::string::npos
		&& std::strlen(text.c_str()) == text.size();
}

bool	StartsWith(const std::string& text, const std::string& prefix)
{
	return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

} // anonymous namespace


//----------------------------------------------------------------------
// The two character overloads
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// One character means one byte. The old code appended two.
//----------------------------------------------------------------------
TEST(StringStream, CharAppendsExactlyOneByte)
{
	StringStream	ss;
	ss << 'x';

	CHECK_EQ(1, ss.toString().size());
	CHECK(ss.toString() == std::string("x"));
	CHECK(IsWholeThroughCStr(ss.toString()));
}

//----------------------------------------------------------------------
// The uchar overload had the identical shape, and is what carries a
// byte outside the signed range.
//----------------------------------------------------------------------
TEST(StringStream, UCharAppendsExactlyOneByte)
{
	{
		StringStream	ss;
		ss << (uchar)'y';

		CHECK_EQ(1, ss.toString().size());
		CHECK(ss.toString() == std::string("y"));
	}
	{
		StringStream	ss;
		ss << (uchar)0xFF;

		CHECK_EQ(1, ss.toString().size());
		CHECK_EQ(0xFF, (int)(unsigned char)ss.toString()[0]);
	}
}

//----------------------------------------------------------------------
// A stream mixing the character overloads with the others must read as
// the text it spells. The old code produced eight bytes here, with a
// NUL at index 3 and another at index 7.
//----------------------------------------------------------------------
TEST(StringStream, MixedInsertionsCarryNoNul)
{
	StringStream	ss;
	ss << "hp" << ':' << (int)42 << '\n';

	const std::string	text = ss.toString();

	CHECK_EQ(6, text.size());
	CHECK(text == std::string("hp:42\n"));
	CHECK(IsWholeThroughCStr(text));
}


//----------------------------------------------------------------------
// Throwable, the consumer the server sees
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// A two-frame stack trace, and the toString() that wraps it. addStack
// pushes to the front, so the most recent frame is rendered first, at
// one leading space, and each frame ends in a streamed newline - which
// is where the NULs came from. toString() streams one more between the
// message and the trace, which is the one that truncated *bug_report.
//----------------------------------------------------------------------
TEST(Throwable, StackTraceAndToStringCarryNoNul)
{
	Throwable	t("boom");
	t.addStack("Inner.cpp", 10);
	t.addStack("Outer.cpp", 20);

	const std::string	trace = t.getStackTrace();

	CHECK(trace == std::string(" Outer.cpp:20\n  Inner.cpp:10\n"));
	CHECK(IsWholeThroughCStr(trace));

	const std::string	text = t.toString();

	CHECK(text == std::string("Throwable : boom\n Outer.cpp:20\n  Inner.cpp:10\n"));
	CHECK(IsWholeThroughCStr(text));

	// What SendBugReport("%s", t.toString().c_str()) actually transmits:
	// the whole text, stack trace included, rather than the first line.
	CHECK(std::string(text.c_str()).find("Inner.cpp:10") != std::string::npos);
}


//----------------------------------------------------------------------
// __assert__, the consumer that writes a file
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// The message opens with a streamed eos, so under the old code byte 1
// was a NUL and assertion_failed.log received a bare newline. The
// four-argument entry point is used here because it names its own file
// and line, which keeps the pinned prefix independent of this file's
// layout.
//----------------------------------------------------------------------
TEST(PacketAssert, AssertionMessageCarriesNoNul)
{
	bool	b_caught = false;

	try {
		__assert__("GameInit.cpp", 4242, "", "pZone != NULL");
	}
	catch (AssertionError& e)
	{
		b_caught = true;

		const std::string	msg = e.getMessage();

		CHECK(msg.size() > 1);
		CHECK_EQ('\n', msg[0]);
		CHECK(msg[1] != '\0');
		CHECK(StartsWith(msg, std::string("\nAssertion Failed : ")));
		CHECK(StartsWith(msg, std::string("\nAssertion Failed : GameInit.cpp : 4242 : pZone != NULL at ")));
		CHECK(IsWholeThroughCStr(msg));
	}

	CHECK(b_caught);

	RemoveAssertionLog();
}


//----------------------------------------------------------------------
// toString() after a further insertion
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// toString() caches its result and rebuilds only after something new
// was inserted. The rebuild used to append the whole list to the
// previous result rather than replace it, so the second call below
// answered "aab". Every caller in the tree calls toString() once, or
// twice with nothing between, which is why nobody saw it; the
// adversarial review of the NUL fix above read it.
//----------------------------------------------------------------------
TEST(StringStream, ToStringIsRebuiltFromScratchAfterAnInsertion)
{
	StringStream	ss;
	ss << 'a';

	CHECK(ss.toString() == std::string("a"));

	ss << 'b';

	CHECK(ss.toString() == std::string("ab"));
	CHECK(ss.toString() == std::string("ab"));

	ss << "cd" << 3;

	CHECK(ss.toString() == std::string("abcd3"));
	CHECK_EQ(5, ss.toString().size());
}
