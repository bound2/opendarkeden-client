#include "test_framework.h"
#include "TextWrap.h"
#include "TextUtf8.h"

#include <limits>
#include <string>
#include <vector>

TEST(TextWrap, PreservesCompleteScalarsAndMakesProgressAtNarrowColumns)
{
	const std::string accent = "\xC3\xA9";
	const std::string korean = "\xEA\xB0\x80";
	const std::string emoji = "\xF0\x9F\x99\x82";
	const std::string text = "a" + accent + korean + emoji + "Z";
	CHECK((TextSystem::WrapUtf8Lines(text, 1) ==
		std::vector<std::string>{"a", accent, korean, emoji, "Z"}));
	CHECK((TextSystem::WrapUtf8Lines(text, 3) ==
		std::vector<std::string>{"a" + accent, korean, emoji, "Z"}));
	CHECK((TextSystem::WrapUtf8Lines(text, 7) ==
		std::vector<std::string>{"a" + accent + korean, emoji + "Z"}));
	for (size_t budget = 1; budget <= text.size() + 1; ++budget) {
		const auto rows = TextSystem::WrapUtf8Lines(text, budget);
		std::string joined;
		for (const auto& row : rows) {
			CHECK(!row.empty());
			CHECK(TextSystem::IsValidUtf8(row.data(), row.size()));
			CHECK(row.size() <= budget || row.size() <= 4);
			joined += row;
		}
		CHECK(joined == text);
	}
}

TEST(TextWrap, SkipsOnlyOneSpaceAtAWrappedSeam)
{
	CHECK((TextSystem::WrapUtf8Lines("ab cd", 2) ==
		std::vector<std::string>{"ab", "cd"}));
	CHECK((TextSystem::WrapUtf8Lines("ab  cd", 2) ==
		std::vector<std::string>{"ab", " c", "d"}));
	CHECK((TextSystem::WrapUtf8Lines(" ab ", 4) ==
		std::vector<std::string>{" ab "}));
	CHECK((TextSystem::WrapUtf8Lines("ab ", 2) ==
		std::vector<std::string>{"ab"}));
}

TEST(TextWrap, HandlesExplicitLineBreaksWithoutPhantomRowsAtWrapBoundaries)
{
	CHECK((TextSystem::WrapUtf8Lines("ab\ncd\r\nef\rgh", 2) ==
		std::vector<std::string>{"ab", "cd", "ef", "gh"}));
	CHECK((TextSystem::WrapUtf8Lines("ab \ncd", 2) ==
		std::vector<std::string>{"ab", "cd"}));
	CHECK((TextSystem::WrapUtf8Lines("\n\nabc\n", 2) ==
		std::vector<std::string>{"", "", "ab", "c"}));
	CHECK((TextSystem::WrapUtf8Lines("\r\n", 20) ==
		std::vector<std::string>{""}));
}

TEST(TextWrap, EmptyAndDisabledColumnsHaveNoRows)
{
	CHECK(TextSystem::WrapUtf8Lines({}, 20).empty());
	CHECK(TextSystem::WrapUtf8Lines("abc", 0).empty());
	CHECK((TextSystem::WrapUtf8Lines("abc", std::numeric_limits<size_t>::max()) ==
		std::vector<std::string>{"abc"}));
}

TEST(TextWrap, ReadsOnlyTheSuppliedSpanAndOwnsItsRows)
{
	std::vector<std::string> rows;
	{
		const char exact[] = {'a', '\0', 'b', '\xE1', '\x80'};
		rows = TextSystem::WrapUtf8Lines(std::string_view(exact, sizeof(exact)), 2);
		CHECK((rows == std::vector<std::string>{std::string("a\0", 2), "b\xE1", "\x80"}));
	}
	CHECK_EQ(3, rows.size());
	CHECK(rows.back() == "\x80");
	// A literal is read-only, and the returned rows are independent of it.
	auto literalRows = TextSystem::WrapUtf8Lines("abcdef", 3);
	literalRows.front()[0] = 'Z';
	CHECK(literalRows.front() == "Zbc");
	CHECK(literalRows.back() == "def");
}

TEST(TextWrap, ChatRowsPreserveSpacesAndCanKeepEmbeddedLineBreaks)
{
	const TextSystem::Utf8WrapOptions chat{.skipSeamSpace = false, .splitNewlines = false};
	CHECK((TextSystem::WrapUtf8Lines("ab  cd", 2, chat) ==
		std::vector<std::string>{"ab", "  ", "cd"}));
	CHECK((TextSystem::WrapUtf8Lines("ab\ncd\r\nef", 4, chat) ==
		std::vector<std::string>{"ab\nc", "d\r\ne", "f"}));
	const std::string text = "\xEA\xB0\x80 \xF0\x9F\x99\x82  Z";
	for (size_t budget = 4; budget <= 255; ++budget) {
		std::string joined;
		for (const auto& row : TextSystem::WrapUtf8Lines(text, budget, chat)) {
			CHECK(row.size() <= budget);
			CHECK(TextSystem::IsValidUtf8(row.data(), row.size()));
			joined += row;
		}
		CHECK(joined == text);
	}
}

TEST(TextWrap, PersonalAndTreeRowsKeepSpacesAroundExplicitLineBreaks)
{
	const TextSystem::Utf8WrapOptions tree{.skipSeamSpace = false};
	CHECK((TextSystem::WrapUtf8Lines("ab \n cd\n", 2, tree) ==
		std::vector<std::string>{"ab", " ", " c", "d"}));
	CHECK((TextSystem::WrapUtf8Lines("\n\xF0\x9F\x99\x82\n\n", 4, tree) ==
		std::vector<std::string>{"", "\xF0\x9F\x99\x82", ""}));
}

TEST(TextWrap, NextRowOwnsTextAndReportsExactConsumptionForChangingColumns)
{
	const std::string text = "a\xEA\xB0\x80\xF0\x9F\x99\x82Z";
	auto row = TextSystem::NextUtf8Line(text, 2);
	CHECK(row.text == "a");
	CHECK_EQ(1, row.consumed);
	row = TextSystem::NextUtf8Line(std::string_view(text).substr(1), 1);
	CHECK(row.text == "\xEA\xB0\x80");
	CHECK_EQ(3, row.consumed);
	row = TextSystem::NextUtf8Line(std::string_view(text).substr(4), 20);
	CHECK(row.text == "\xF0\x9F\x99\x82Z");
	CHECK_EQ(5, row.consumed);
	CHECK_EQ(0, TextSystem::NextUtf8Line(text, 0).consumed);
	CHECK_EQ(0, TextSystem::NextUtf8Line({}, 1).consumed);
	row = TextSystem::NextUtf8Line(std::string("temporary"), 3);
	CHECK(row.text == "tem");
	CHECK_EQ(3, row.consumed);
}

TEST(TextWrap, DialogRowsTrimLeadingSpacesWithoutConfusingBlankLines)
{
	const TextSystem::Utf8WrapOptions dialog{.skipSeamSpace = false, .trimLeadingSpaces = true};
	CHECK((TextSystem::WrapUtf8Lines("  ab   cd\n  \n ef", 2, dialog) ==
		std::vector<std::string>{"ab", "cd", "", "ef"}));
	const auto row = TextSystem::NextUtf8Line("   ab\r\nZ", 2, dialog);
	CHECK(row.text == "ab");
	CHECK_EQ(7, row.consumed);
	const auto spaces = TextSystem::NextUtf8Line("   ", 2, dialog);
	CHECK(spaces.text.empty());
	CHECK_EQ(3, spaces.consumed);
}

TEST(TextWrap, EscapedNewlinesAreAtomicEvenAcrossAColumnBoundary)
{
	const TextSystem::Utf8WrapOptions escaped{.splitNewlines = false, .splitEscapedNewlines = true};
	for (size_t budget = 1; budget <= 8; ++budget) {
		CHECK((TextSystem::WrapUtf8Lines("a\\nb\\n\\n", budget, escaped) ==
			std::vector<std::string>{"a", "b", ""}));
	}
	CHECK((TextSystem::WrapUtf8Lines("a\nb\\nC", 20, escaped) ==
		std::vector<std::string>{"a\nb", "C"}));
	CHECK((TextSystem::WrapUtf8Lines("a\\", 20, escaped) ==
		std::vector<std::string>{"a\\"}));
	const TextSystem::Utf8WrapOptions both{.splitEscapedNewlines = true};
	CHECK((TextSystem::WrapUtf8Lines("a\\nb\r\nC", 20, both) ==
		std::vector<std::string>{"a", "b", "C"}));
}

TEST(TextWrap, LongDialogTextHasNoFixedScratchLimit)
{
	const std::string text(4096, 'x');
	const auto row = TextSystem::NextUtf8Line(text, 3000);
	CHECK_EQ(3000, row.consumed);
	CHECK(row.text == std::string(3000, 'x'));
	CHECK((TextSystem::WrapUtf8Lines(text, 3000) ==
		std::vector<std::string>{std::string(3000, 'x'), std::string(1096, 'x')}));
	const char exact[] = {'x', '\\'};
	const TextSystem::Utf8WrapOptions escaped{.splitEscapedNewlines = true};
	const auto tail = TextSystem::NextUtf8Line(std::string_view(exact, 2), 2, escaped);
	CHECK_EQ(2, tail.consumed);
	CHECK(tail.text == "x\\");
}

TEST(TextWrap, LargeColumnsKeepManyShortExplicitRows)
{
	const std::string text(4096, '\n');
	const auto rows = TextSystem::WrapUtf8Lines(text, std::numeric_limits<size_t>::max());
	CHECK_EQ(text.size(), rows.size());
	for (const auto& row : rows) CHECK(row.empty());
}

TEST(TextWrap, MeasuredRowsPreserveBytesAndUseDifferentContinuationWidths)
{
	const auto rows = TextSystem::WrapUtf8MeasuredLines("a bc de", 3, 2,
		[](const std::string& text) { return static_cast<int>(text.size()); });
	CHECK_EQ(size_t(3), rows.size());
	if (rows.size() == 3) {
		CHECK(rows[0] == "a b");
		CHECK(rows[1] == "c ");
		CHECK(rows[2] == "de");
	}
}

TEST(TextWrap, MeasuredRowsAdvanceAcrossWideAndMalformedScalars)
{
	const std::string input = "\xE2\x82\xAC" "x\xFF";
	const auto rows = TextSystem::WrapUtf8MeasuredLines(input, 0, 1,
		[](const std::string&) { return 20; });
	CHECK_EQ(size_t(3), rows.size());
	if (rows.size() == 3) {
		CHECK(rows[0] == input.substr(0, 3));
		CHECK(rows[1] == "x");
		CHECK(rows[2] == input.substr(4));
	}
	CHECK(TextSystem::WrapUtf8MeasuredLines("", 1, 1,
		[](const std::string&) { return 0; }).empty());
}
