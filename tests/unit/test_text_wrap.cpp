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
