//----------------------------------------------------------------------
// test_packet_source_location.cpp
//----------------------------------------------------------------------
//
// The second std::source_location slice
// (docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md, modernization
// backlog priority 1): the explicit __FILE__/__LINE__ forwarders in the
// packet wire layer, converted to capture their site instead.
//
// Three call shapes changed, and the whole claim about all three is that
// nothing observable moved - the same string is logged, the same string
// is pushed onto a stack trace, the same type is thrown. So the pins
// below are exact strings, built from this translation unit's own
// __FILE__ and __LINE__, not "the line is non-zero" checks. A defaulted
// std::source_location::current() is evaluated at the CALL site, and
// here it is nested one level - it is the default argument of the
// DiagnosticSite constructor, which is itself the default argument of
// the entry point. A compiler that resolved it to the header would
// report Exception.h's own line for every __END_CATCH in the tree, and
// that is exactly what these comparisons would catch.
//
// What each site produced BEFORE the conversion, read off the old code:
//
//   __END_CATCH (Exception.h) expanded to
//       } catch ( Throwable & t ) { t.addStack(__FILE__, __LINE__); throw; }
//   and addStack pushes "<file>:<line>", which getStackTrace() renders
//   as one leading space per depth followed by that string and '\n' -
//   plus the NUL StringStream appends to every streamed char, see
//   NEWLINE_AND_NUL below.
//
//   Assert(expr) (PacketAssert.h, the __WIN32__ branch) expanded to
//       __assert__(__FILE__,__LINE__,"",#expr)
//   and __assert__ builds, with func == "" being non-NULL and therefore
//   still contributing its separator:
//       "\n\0Assertion Failed : <file> : <line> : <expr> at <ctime>"
//   (the leading eos is a char, so it carries a NUL of its own)
//   then writes that to assertion_failed.log and throws AssertionError
//   carrying it. (The tests below therefore append to that file in the
//   ctest working directory, exactly as a failing Assert always has.)
//
//   ClientPlayer.cpp's packet-skip notice was
//       DEBUG_ADD_FORMAT("[PacketSkip] So many Packets. MaxProcessPacket:%d,"
//                        " CurrentPacket:%d, File:%s, Line:%d",
//                        maxProcessPacket, processedPacket, __FILE__, __LINE__)
//   i.e. log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, ...) with the same
//   file and line repeated inside the message body.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "PacketAssert.h"
#include "Exception.h"
#include "DebugLog.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>


namespace {

//----------------------------------------------------------------------
// A failing Assert appends its message to assertion_failed.log in the
// working directory, exactly as it always has. The tests that provoke
// one remove the file afterwards, so the ctest directory does not grow
// a line per run.
//----------------------------------------------------------------------
void	RemoveAssertionLog()
{
	std::error_code Error;
	std::filesystem::remove("assertion_failed.log", Error);
}

//----------------------------------------------------------------------
// The __END_CATCH probe.
//
// The macro has to be expanded in a real function for its capture to
// mean anything, so the line it occupies is recorded from inside, one
// statement above it, rather than counted from outside the function.
//----------------------------------------------------------------------
int	g_n_end_catch_line = 0;

void
ThrowThroughEndCatch()
{
	__BEGIN_TRY

	// __END_CATCH is three lines below this comment, two below the
	// assignment.
	g_n_end_catch_line = __LINE__ + 2;
	throw Exception("a message from the unit tests");
	__END_CATCH
}

//----------------------------------------------------------------------
// A NUL follows every character streamed into a StringStream: its
// operator<<(char) builds std::string(2, '\0') and writes the character
// into the first byte, so the second byte survives into the result.
// Both texts pinned below end up carrying one, and both carried one
// before this change too - so the expectations spell it out rather than
// trimming it away, which would make "byte for byte" a weaker claim
// than it is.
//----------------------------------------------------------------------
const std::string	NEWLINE_AND_NUL("\n\0", 2);

//----------------------------------------------------------------------
// The stack trace Throwable renders for a single frame: one space, the
// "file:line" string addStack pushed, and a streamed newline.
//----------------------------------------------------------------------
std::string
OneFrame(const std::string& file, int line)
{
	return " " + file + ":" + std::to_string(line) + NEWLINE_AND_NUL;
}

//----------------------------------------------------------------------
// Everything a failed assertion writes before the timestamp ctime()
// appends, which is the only part that legitimately differs run to run.
//----------------------------------------------------------------------
std::string
AssertionPrefix(const std::string& file, int line, const std::string& func, const std::string& expr)
{
	// The leading eos is a char, so it brings its NUL with it. func is
	// streamed only when it is non-NULL, and an empty function name is
	// non-NULL - so its separator is emitted either way, and no
	// separator ever appears between func and expr.
	return NEWLINE_AND_NUL + "Assertion Failed : " + file + " : " + std::to_string(line)
		+ " : " + func + expr + " at ";
}

bool
StartsWith(const std::string& text, const std::string& prefix)
{
	return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

} // anonymous namespace


//----------------------------------------------------------------------
// DiagnosticSite
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// The type the two converted macros go through. Its default constructor
// must report the caller, and its (file, line) constructor must report
// exactly what it is handed and claim no function name it does not have.
//----------------------------------------------------------------------
TEST(PacketSourceLocation, DiagnosticSiteCapturesTheCallersLine)
{
	const int		n_expected_line = __LINE__ + 1;
	const DiagnosticSite	site;

	CHECK_EQ(n_expected_line, site.line);
	CHECK(std::string(site.file) == __FILE__);
	CHECK(site.function != NULL);
	CHECK(std::string(site.function).find("PacketSourceLocation") != std::string::npos);

	const DiagnosticSite	explicit_site("GameMain.cpp", 1234);

	CHECK_EQ(1234, explicit_site.line);
	CHECK(std::string(explicit_site.file) == "GameMain.cpp");
	CHECK(explicit_site.function == NULL);
}


//----------------------------------------------------------------------
// Throwable::addStack and __END_CATCH
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// The no-argument overload is what __END_CATCH calls now. It must push
// the same "file:line" string the two-argument overload pushed when the
// macro forwarded __FILE__ and __LINE__ into it.
//----------------------------------------------------------------------
TEST(PacketSourceLocation, AddStackEntryPointRecordsTheCallersSite)
{
	Throwable	t("a message from the unit tests");

	const int	n_line = __LINE__ + 1;
	t.addStack();

	CHECK(t.getStackTrace() == OneFrame(__FILE__, n_line));
	CHECK(t.getMessage() == "a message from the unit tests");
}

//----------------------------------------------------------------------
// The (file, line) overload is untouched and still records exactly what
// it is given - it is what the captured overload delegates to, so the
// two must agree about the same site.
//----------------------------------------------------------------------
TEST(PacketSourceLocation, AddStackCompatibilityOverloadRecordsWhatItIsGiven)
{
	Throwable	t_explicit;
	t_explicit.addStack("GameInit.cpp", 4242);
	CHECK(t_explicit.getStackTrace() == OneFrame("GameInit.cpp", 4242));

	// Handed this translation unit's own __FILE__ and __LINE__, the old
	// spelling renders what the captured one renders for itself.
	Throwable		t_forwarded;
	const int		n_forwarded_line = __LINE__ + 1;
	t_forwarded.addStack(__FILE__, __LINE__);

	Throwable		t_captured;
	const int		n_captured_line = __LINE__ + 1;
	t_captured.addStack();

	CHECK(t_forwarded.getStackTrace() == OneFrame(__FILE__, n_forwarded_line));
	CHECK(t_captured.getStackTrace() == OneFrame(__FILE__, n_captured_line));

	// Same shape, different line: neither overload is quietly reporting
	// the header's own location.
	CHECK(t_forwarded.getStackTrace() != t_captured.getStackTrace());
}

//----------------------------------------------------------------------
// The population that mattered: every method in the wire layer is
// wrapped in __BEGIN_TRY/__END_CATCH, and the macro no longer spells
// __FILE__ and __LINE__. The frame it pushes must name the line the
// macro was written on - the header's own line would be the failure
// mode worth catching.
//----------------------------------------------------------------------
TEST(PacketSourceLocation, EndCatchRecordsTheLineOfTheMacroUse)
{
	bool	b_caught = false;

	try
	{
		ThrowThroughEndCatch();
	}
	catch (Throwable& t)
	{
		b_caught = true;

#ifdef NDEBUG
		// __BEGIN_TRY/__END_CATCH compile away entirely under NDEBUG,
		// so there is no frame to name. Debug is where this slice is
		// verified; this branch only keeps the test honest elsewhere.
		CHECK(t.getStackTrace().empty());
#else
		CHECK(g_n_end_catch_line != 0);
		CHECK(t.getStackTrace() == OneFrame(__FILE__, g_n_end_catch_line));
		CHECK(t.getMessage() == "a message from the unit tests");
#endif
	}

	CHECK(b_caught);
}


//----------------------------------------------------------------------
// __assert__ and Assert()
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// The C++20 entry point: the site arrives captured rather than
// forwarded, and the message is the one the old four-argument call
// produced, character for character up to ctime()'s timestamp.
//----------------------------------------------------------------------
TEST(PacketSourceLocation, AssertEntryPointRecordsTheCallersSite)
{
	bool	b_caught = false;

	const int	n_line = __LINE__ + 2;
	try {
		__assert__("", "n_value != 0");
	}
	catch (AssertionError& e)
	{
		b_caught = true;
		CHECK(StartsWith(e.getMessage(), AssertionPrefix(__FILE__, n_line, "", "n_value != 0")));
		CHECK(e.getName() == "AssertionError");
		RemoveAssertionLog();
	}

	CHECK(b_caught);
}

//----------------------------------------------------------------------
// Assert(expr) itself, which is the shape every live assertion in the
// wire layer uses. On this platform the macro passes an empty function
// name, and
// that empty name is still non-NULL inside __assert__ - so its " : "
// separator is still emitted and no separator appears before the
// expression. Both quirks are pinned here because keeping them is the
// point: the conversion may not move a single character.
//----------------------------------------------------------------------
TEST(PacketSourceLocation, AssertMacroRecordsTheLineOfTheMacroUse)
{
	const bool	b_false = false;
	bool		b_caught = false;

#ifdef NDEBUG
	// Assert() compiles away entirely under NDEBUG (PacketAssert.h), so
	// nothing throws. Debug is where this slice is verified; this branch
	// only keeps the test honest elsewhere.
	Assert(b_false);
	CHECK_EQ(false, b_caught);
#else
	const int	n_line = __LINE__ + 2;
	try {
		Assert(b_false);
	}
	catch (AssertionError& e)
	{
		b_caught = true;
		CHECK(StartsWith(e.getMessage(), AssertionPrefix(__FILE__, n_line, "", "b_false")));
	}

	CHECK(b_caught);
#endif

	// A satisfied assertion still says nothing at all.
	const bool	b_true = true;
	bool		b_threw = false;

	try {
		Assert(b_true);
	}
	catch (Throwable&)
	{
		b_threw = true;
	}

	CHECK_EQ(false, b_threw);

	RemoveAssertionLog();
}

//----------------------------------------------------------------------
// The four-argument entry point is unchanged and still writes exactly
// what it is handed, including the missing separator between a non-empty
// function name and the expression. Nothing in the tree calls it
// directly today; the captured overload delegates to it, so this is the
// pin on the text both spellings share.
//----------------------------------------------------------------------
TEST(PacketSourceLocation, AssertCompatibilityOverloadRecordsWhatItIsGiven)
{
	bool	b_caught = false;

	try {
		__assert__("GameInit.cpp", 4242, "InitGame", "pZone != NULL");
	}
	catch (AssertionError& e)
	{
		b_caught = true;
		CHECK(StartsWith(e.getMessage(),
			NEWLINE_AND_NUL + "Assertion Failed : GameInit.cpp : 4242 : InitGamepZone != NULL at "));
	}

	CHECK(b_caught);

	// A NULL function name drops its separator with it, which leaves the
	// line number and the expression run together. Nothing passes NULL
	// today; the case is pinned so the delegation above cannot start.
	b_caught = false;

	try {
		__assert__("GameInit.cpp", 4242, NULL, "pZone != NULL");
	}
	catch (AssertionError& e)
	{
		b_caught = true;
		CHECK(StartsWith(e.getMessage(),
			NEWLINE_AND_NUL + "Assertion Failed : GameInit.cpp : 4242pZone != NULL at "));
	}

	RemoveAssertionLog();

	CHECK(b_caught);
}


//----------------------------------------------------------------------
// ClientPlayer.cpp's packet-skip notice
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// That site is inside ClientPlayer::processCommand, behind a socket and
// a full packet queue, so the line it emits cannot be provoked from a
// test binary. What can be pinned is the shape it was converted to: the
// message repeats the file and line the log header already carries, and
// after the conversion both come from one captured site. So the old
// spelling and the new one are written here side by side, into the file
// sink, and the two lines must be the same line but for the number each
// one names for itself.
//
// The logging system is initialised for the duration of this test only
// and put back afterwards, and the timestamp - the one part of a line
// that legitimately differs between two calls - is cut off before the
// comparison.
//----------------------------------------------------------------------
TEST(PacketSourceLocation, PacketSkipLineIsWhatTheMacroProduced)
{
	static const char* const	sz_format =
		"[PacketSkip] So many Packets. MaxProcessPacket:%d, CurrentPacket:%d, File:%s, Line:%d";

	std::error_code			error;
	const std::filesystem::path	log_path =
		std::filesystem::temp_directory_path(error) / "packet_source_location.log";

	log_init();
	log_set_console_output(false);
	log_set_file_output(log_path.string().c_str());
	log_set_level(LOG_LEVEL_INFO);

	// Exactly what ClientPlayer.cpp held before the conversion.
	const int n_macro_line = __LINE__ + 1;
	DEBUG_ADD_FORMAT(sz_format, 30, 31, __FILE__, __LINE__);

	// Exactly what it holds now.
	const int n_site_line = __LINE__ + 1;
	const LogSite site;
	log_write_at(site, LOG_LEVEL_INFO, sz_format, 30, 31, site.file, site.line);

	log_set_file_output(NULL);
	log_set_console_output(true);
	log_cleanup();

	std::vector<std::string>	lines;
	{
		std::ifstream	file(log_path);
		std::string	line;

		while (std::getline(file, line))
		{
			// Everything after the timestamp: "[INFO ] [file:line] message".
			const std::string::size_type n_cut = line.find("] ");
			lines.push_back(n_cut == std::string::npos ? line : line.substr(n_cut + 2));
		}
	}
	std::filesystem::remove(log_path, error);

	CHECK_EQ(2, lines.size());

	if (lines.size() == 2)
	{
		// DebugLog prints the basename in the header and whatever it was
		// handed in the body, which is the full __FILE__ either way.
		const std::string	expected_macro =
			"[INFO ] [test_packet_source_location.cpp:" + std::to_string(n_macro_line)
			+ "] [PacketSkip] So many Packets. MaxProcessPacket:30, CurrentPacket:31, File:"
			+ __FILE__ + ", Line:" + std::to_string(n_macro_line);

		const std::string	expected_site =
			"[INFO ] [test_packet_source_location.cpp:" + std::to_string(n_site_line)
			+ "] [PacketSkip] So many Packets. MaxProcessPacket:30, CurrentPacket:31, File:"
			+ __FILE__ + ", Line:" + std::to_string(n_site_line);

		CHECK(lines[0] == expected_macro);
		CHECK(lines[1] == expected_site);

		// The header and the body of the converted line name the same
		// site, which is the property the conversion actually buys: the
		// two halves are now one value.
		CHECK(lines[1].find(":" + std::to_string(n_site_line) + "]") != std::string::npos);
		CHECK(lines[1].find("Line:" + std::to_string(n_site_line)) != std::string::npos);
	}
}
