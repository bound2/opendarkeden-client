//----------------------------------------------------------------------
// test_stringstream_chars.cpp
//----------------------------------------------------------------------
//
// StringStream's two character overloads, and the diagnostic text
// Throwable and __assert__ build out of them: one streamed character is
// one byte, with no embedded NUL to cut the text short through c_str().
// Also toString()'s rebuild after a further insertion.
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

// A failing assertion appends to assertion_failed.log; clean it up.
void	RemoveAssertionLog()
{
	std::error_code	Error;
	std::filesystem::remove("assertion_failed.log", Error);
}

// Whether the text survives being handed to a C interface whole.
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

TEST(StringStream, CharAppendsExactlyOneByte)
{
	StringStream	ss;
	ss << 'x';

	CHECK_EQ(1, ss.toString().size());
	CHECK(ss.toString() == std::string("x"));
	CHECK(IsWholeThroughCStr(ss.toString()));
}

// The uchar overload is what carries a byte outside the signed range.
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

// addStack pushes to the front, so the most recent frame renders first.
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

	CHECK(std::string(text.c_str()).find("Inner.cpp:10") != std::string::npos);
}


//----------------------------------------------------------------------
// __assert__, the consumer that writes a file
//----------------------------------------------------------------------

// The four-argument entry point names its own file and line, which keeps
// the pinned prefix independent of this file's layout.
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

// toString() caches its result and rebuilds after a further insertion.
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
