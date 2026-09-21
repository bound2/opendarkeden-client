#include "test_framework.h"
#include "TextUtf8.h"
#include "TextService.h"
#include <string>
#include <vector>
#include <limits>

TEST(TextUtf8, RejectsOverlongSurrogateAndOutOfRangeScalars)
{
	for (const std::string bytes : {
		"\xC0\x80", "\xC1\xBF", "\xE0\x80\x80", "\xE0\x9F\xBF",
		"\xED\xA0\x80", "\xED\xBF\xBF", "\xF0\x80\x80\x80",
		"\xF0\x8F\xBF\xBF", "\xF4\x90\x80\x80", "\xF7\xBF\xBF\xBF"}) {
		CHECK(!TextSystem::IsValidUtf8(bytes.data(), bytes.size()));
		int used = 0;
		CHECK_EQ(0xFFFD, TextSystem::Utf8Decode(bytes.data(), static_cast<int>(bytes.size()), &used));
		CHECK_EQ(1, used);
	}
}

TEST(TextUtf8, BadContinuationDoesNotSwallowFollowingAscii)
{
	for (const std::string bytes : {"\xC2" "A", "\xE1\x80" "A", "\xF1\x80\x80" "A"}) {
		CHECK(!TextSystem::IsValidUtf8(bytes.data(), bytes.size()));
		int used = 0;
		CHECK_EQ(0xFFFD, TextSystem::Utf8Decode(bytes.data(), static_cast<int>(bytes.size()), &used));
		CHECK_EQ(1, used);
	}
	// Force the normalization fallback: no legacy code page accepts 0xFF.
	// Measurement must preserve A after the invalid lead, using replacement
	// glyphs on either side rather than absorbing A into another code point.
	auto& text = TextSystem::TextService::Get();
	auto style = text.GetDefaultStyle();
	CHECK(style.font.IsValid());
	CHECK_EQ(text.MeasureText("\xEF\xBF\xBD" "A\xEF\xBF\xBD", style).width,
		text.MeasureText("\xC2" "A\xFF", style).width);
}

TEST(TextUtf8, AcceptsScalarBoundariesAndNeverReadsPastTruncations)
{
	struct Example { std::string bytes; uint32_t scalar; };
	const std::vector<Example> examples = {
		{std::string(1, '\0'), 0}, {"\x7F", 0x7F}, {"\xC2\x80", 0x80},
		{"\xDF\xBF", 0x7FF}, {"\xE0\xA0\x80", 0x800},
		{"\xED\x9F\xBF", 0xD7FF}, {"\xEE\x80\x80", 0xE000},
		{"\xEF\xBF\xBD", 0xFFFD}, {"\xEF\xBF\xBF", 0xFFFF},
		{"\xF0\x90\x80\x80", 0x10000}, {"\xF4\x8F\xBF\xBF", 0x10FFFF}
	};
	CHECK(TextSystem::IsValidUtf8(nullptr, 0));
	int used = -1;
	CHECK_EQ(0xFFFD, TextSystem::Utf8Decode(nullptr, 0, &used));
	CHECK_EQ(0, used);
	for (const auto& example : examples) {
		CHECK(TextSystem::IsValidUtf8(example.bytes.data(), example.bytes.size()));
		CHECK_EQ(example.scalar, TextSystem::Utf8Decode(example.bytes.data(), static_cast<int>(example.bytes.size()), &used));
		CHECK_EQ(example.bytes.size(), used);
		for (size_t length = 1; length < example.bytes.size(); ++length) {
			// Allocate exactly the supplied span for ASan to check the boundary.
			std::vector<char> truncated(example.bytes.begin(), example.bytes.begin() + length);
			CHECK(!TextSystem::IsValidUtf8(truncated.data(), truncated.size()));
			CHECK_EQ(0xFFFD, TextSystem::Utf8Decode(truncated.data(), static_cast<int>(length), &used));
			CHECK_EQ(1, used);
		}
	}
}

TEST(TextUtf8, PrefixBudgetKeepsTwoThreeAndFourByteScalars)
{
	const std::string text = "a\xC3\xA9\xEA\xB0\x80\xF0\x9F\x99\x82Z";
	const size_t expected[] = {0, 1, 1, 3, 3, 3, 6, 6, 6, 6, 10, 11};
	for (size_t budget = 0; budget < sizeof(expected) / sizeof(expected[0]); ++budget) {
		const size_t size = TextSystem::Utf8PrefixBytes(text, budget);
		CHECK_EQ(expected[budget], size);
		CHECK(TextSystem::IsValidUtf8(text.data(), size));
	}
	CHECK_EQ(text.size(), TextSystem::Utf8PrefixBytes(text, std::numeric_limits<size_t>::max()));
	CHECK_EQ(0, TextSystem::Utf8PrefixBytes({}, 123));
}

TEST(TextUtf8, PrefixHandlesMalformedTailsAndEmbeddedNulsWithinTheSpan)
{
	for (const std::string bytes : {"\xC0\x80", "\xE0\x80\x80", "\xED\xA0\x80",
		"\xF4\x90\x80\x80", "\xE1\x80", "\xFF" "A"}) {
		// None contains a complete valid multibyte scalar. Each damaged byte
		// remains individually consumable, including the truncated final byte.
		std::vector<char> exact(bytes.begin(), bytes.end());
		for (size_t budget = 0; budget <= exact.size(); ++budget)
			CHECK_EQ(budget, TextSystem::Utf8PrefixBytes(
				std::string_view(exact.data(), exact.size()), budget));
	}
	const std::string nul("\0\xC3\xA9", 3);
	CHECK_EQ(1, TextSystem::Utf8PrefixBytes(nul, 1));
	CHECK_EQ(1, TextSystem::Utf8PrefixBytes(nul, 2));
	CHECK_EQ(3, TextSystem::Utf8PrefixBytes(nul, 3));
}
