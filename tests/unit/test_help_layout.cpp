#include "test_framework.h"
#include "HelpLayout.h"

#include <limits>

namespace {
HelpLayout::Options Columns(size_t columns)
{
	HelpLayout::Options options;
	options.columns = columns;
	options.glyphWidth = 2;
	options.lineHeight = 10;
	return options;
}
HelpLayout::ImageSize Image(std::string_view filename)
{
	return filename == "map.jpg" ? HelpLayout::ImageSize{4, 20} : HelpLayout::ImageSize{};
}
}

TEST(HelpLayout, OwnsUtf8PhysicalRowsAndPreservesLiteralText)
{
	HelpLayout::Document document;
	std::string input = "abc\r\n\rx\n\xF0\x9F\x99\x82z";
	CHECK(HelpLayout::Parse(input, Columns(4), {}, document));
	input.assign(200, 'x');
	CHECK_EQ(size_t{5}, document.rowCount);
	CHECK_EQ(size_t{5}, document.text.size());
	if (document.text.size() == 5) {
		CHECK(document.text[0].text == "abc");
		CHECK(document.text[1].text.empty());
		CHECK(document.text[2].text == "x");
		CHECK(document.text[3].text == "\xF0\x9F\x99\x82");
		CHECK(document.text[4].text == "z");
	}
	CHECK(HelpLayout::Parse("10%: x < y\n  keep spaces", Columns(60), {}, document));
	CHECK_EQ(size_t{2}, document.text.size());
	if (document.text.size() == 2) {
		CHECK(document.text[0].text == "10%: x < y");
		CHECK(document.text[1].text == "  keep spaces");
	}
}

TEST(HelpLayout, LongLinesAndNarrowColumnsAlwaysProgress)
{
	HelpLayout::Document document;
	const std::string input(25600, 'x');
	CHECK(HelpLayout::Parse(input, Columns(1024), {}, document));
	CHECK_EQ(size_t{25}, document.rowCount);
	std::string joined;
	for (const auto& row : document.text) joined += row.text;
	CHECK(joined == input);
	CHECK(HelpLayout::Parse("\xEA\xB0\x80\xF0\x9F\x99\x82z", Columns(1), {}, document));
	CHECK_EQ(size_t{3}, document.rowCount);
	CHECK(HelpLayout::Parse("", Columns(60), {}, document));
	CHECK_EQ(size_t{0}, document.rowCount);
	CHECK(document.text.empty());
}

TEST(HelpLayout, FontAppendReusesOnlyAvailableSpaceOnThePreviousRow)
{
	HelpLayout::Document document;
	CHECK(HelpLayout::Parse("abc\n<#color='00ff00' a='y'>\ndefghi", Columns(8), {}, document));
	CHECK_EQ(size_t{2}, document.rowCount);
	CHECK_EQ(size_t{3}, document.text.size());
	if (document.text.size() == 3) {
		CHECK_EQ(0xffffffu, document.text[0].color);
		CHECK(document.text[1].text == "   defgh");
		CHECK_EQ(size_t{0}, document.text[1].row);
		CHECK_EQ(0x00ff00u, document.text[1].color);
		CHECK(document.text[2].text == "i");
		CHECK_EQ(size_t{1}, document.text[2].row);
	}
	CHECK(HelpLayout::Parse("abcdefgh\n<#a='y'>\nz", Columns(8), {}, document));
	CHECK_EQ(size_t{2}, document.rowCount);
	CHECK(document.text.back().text == "z");
	CHECK(HelpLayout::Parse("<#a='y'>\nz", Columns(8), {}, document));
	CHECK_EQ(size_t{1}, document.rowCount);
	CHECK(document.text.front().text == "z");
}

TEST(HelpLayout, LeftAndRightImagesReserveOnlyTheirOverlappingRows)
{
	for (const auto* position : {"L", "R"}) {
		HelpLayout::Document document;
		const std::string text = std::string("<file='map' pos='") + position + "'>\nabcdefghi";
		CHECK(HelpLayout::Parse(text, Columns(6), Image, document));
		CHECK_EQ(size_t{1}, document.images.size());
		CHECK_EQ(size_t{3}, document.rowCount);
		CHECK_EQ(size_t{3}, document.text.size());
		if (document.text.size() == 3) {
			const std::string indent = position[0] == 'L' ? "  " : "";
			CHECK(document.text[0].text == indent + "abcd");
			CHECK(document.text[1].text == indent + "efgh");
			CHECK(document.text[2].text == "i");
		}
		if (!document.images.empty()) {
			CHECK_EQ(size_t{0}, document.images[0].row);
			CHECK(document.images[0].filename == "map.jpg");
			CHECK_EQ(4, document.images[0].width);
			CHECK_EQ(20, document.images[0].height);
		}
	}
}

TEST(HelpLayout, BlockAndOversizedImagesAdvanceBeforeText)
{
	for (const auto* position : {"C", "LT", "RT"}) {
		HelpLayout::Document document;
		const std::string text = std::string("<file='map' pos='") + position + "'>\nend";
		CHECK(HelpLayout::Parse(text, Columns(6), Image, document));
		CHECK_EQ(size_t{3}, document.rowCount);
		CHECK_EQ(size_t{1}, document.text.size());
		if (!document.text.empty()) CHECK_EQ(size_t{2}, document.text.front().row);
	}
	HelpLayout::Document document;
	const auto wide = [](std::string_view) { return HelpLayout::ImageSize{200, 20}; };
	CHECK(HelpLayout::Parse("<file='map' pos='L'>\nabcdef", Columns(3), wide, document));
	CHECK_EQ(size_t{4}, document.rowCount);
	CHECK_EQ(size_t{2}, document.text.size());
	if (!document.text.empty()) {
		CHECK_EQ(size_t{2}, document.text[0].row);
		CHECK(document.text[0].text == "abc");
	}
	CHECK(HelpLayout::Parse("<file='map' pos='L'>", Columns(6), Image, document));
	CHECK_EQ(size_t{2}, document.rowCount);
	CHECK(HelpLayout::Parse("<file='map' pos='L'>\n\nend", Columns(3), wide, document));
	CHECK_EQ(size_t{4}, document.rowCount);
	CHECK_EQ(size_t{2}, document.text.size());
	if (document.text.size() == 2) CHECK(document.text[0].text.empty());
}

TEST(HelpLayout, AppendAndChineseIndentUseTheActualEmittedSpaceBudget)
{
	HelpLayout::Document document;
	CHECK(HelpLayout::Parse("<file='map' pos='L'>\na\n<#a='y'>\nbcde", Columns(6), Image, document));
	CHECK_EQ(size_t{3}, document.text.size());
	if (document.text.size() == 3) {
		CHECK(document.text[0].text == "  a");
		CHECK(document.text[1].text == "   bcd");
		CHECK_EQ(size_t{0}, document.text[1].row);
		CHECK(document.text[2].text == "  e");
		CHECK_EQ(size_t{1}, document.text[2].row);
	}
	auto options = Columns(6);
	options.indentScale = 2;
	CHECK(HelpLayout::Parse("<file='map' pos='L'>\nabcde", options, Image, document));
	CHECK_EQ(size_t{3}, document.text.size());
	if (document.text.size() == 3) {
		CHECK(document.text[0].text == "    ab");
		CHECK(document.text[1].text == "    cd");
		CHECK(document.text[2].text == "e");
	}
}

TEST(HelpLayout, MissingImagesAndInvalidAlignmentsKeepTheFollowingText)
{
	HelpLayout::Document document;
	int lookups = 0;
	const auto absent = [&](std::string_view) { ++lookups; return HelpLayout::ImageSize{}; };
	CHECK(HelpLayout::Parse("<file='map' pos='bad'>\n<file='missing' pos='L'>\n<file='' pos='L'>\nbody",
		Columns(60), absent, document));
	CHECK_EQ(1, lookups);
	CHECK(document.images.empty());
	CHECK_EQ(size_t{1}, document.rowCount);
	CHECK(document.text.back().text == "body");
	CHECK(HelpLayout::Parse("<#color='invalid'>\nwhite\n<#color='123abc'>\ncolor", Columns(60), {}, document));
	CHECK_EQ(size_t{2}, document.text.size());
	if (document.text.size() == 2) {
		CHECK_EQ(0xffffffu, document.text[0].color);
		CHECK_EQ(0x123abcu, document.text[1].color);
	}
}

TEST(HelpLayout, FollowingImagesStartAfterThePreviousImage)
{
	HelpLayout::Document document;
	CHECK(HelpLayout::Parse("<file='map' pos='L'>\nx\n<file='map' pos='R'>\ny", Columns(8), Image, document));
	CHECK_EQ(size_t{2}, document.images.size());
	CHECK_EQ(size_t{4}, document.rowCount);
	if (document.images.size() == 2) CHECK_EQ(size_t{2}, document.images[1].row);
	CHECK_EQ(size_t{2}, document.text.size());
	if (document.text.size() == 2) CHECK_EQ(size_t{2}, document.text[1].row);
}

TEST(HelpLayout, ScalarsThatCannotFitBesideAnImageOrAppendMoveToTheNextSpace)
{
	HelpLayout::Document document;
	auto options = Columns(4);
	options.glyphWidth = 1;
	const auto image = [](std::string_view) { return HelpLayout::ImageSize{3, 20}; };
	CHECK(HelpLayout::Parse("<file='map' pos='R'>\n\xEA\xB0\x80", options, image, document));
	CHECK_EQ(size_t{3}, document.rowCount);
	CHECK_EQ(size_t{1}, document.text.size());
	if (!document.text.empty()) CHECK_EQ(size_t{2}, document.text[0].row);
	CHECK(HelpLayout::Parse("abc\n<#a='y'>\n\xEA\xB0\x80", options, {}, document));
	CHECK_EQ(size_t{2}, document.rowCount);
	CHECK_EQ(size_t{2}, document.text.size());
	if (document.text.size() == 2) {
		CHECK_EQ(size_t{1}, document.text[1].row);
		CHECK(document.text[1].text == "\xEA\xB0\x80");
	}
}

TEST(HelpLayout, InvalidGeometryAndLimitsDoNotPublishPartialDocuments)
{
	HelpLayout::Document document;
	CHECK(HelpLayout::Parse("kept", Columns(60), {}, document));
	for (int field = 0; field < 3; ++field) {
		auto invalid = Columns(60);
		if (field == 0) invalid.columns = 0;
		if (field == 1) invalid.glyphWidth = 0;
		if (field == 2) invalid.lineHeight = 0;
		CHECK(!HelpLayout::Parse("new", invalid, {}, document));
	}
	auto limited = Columns(1);
	limited.maxRows = 2;
	CHECK(!HelpLayout::Parse("abc", limited, {}, document));
	limited = Columns(60);
	limited.maxBytes = 2;
	CHECK(!HelpLayout::Parse("abc", limited, {}, document));
	limited = Columns(60);
	limited.maxRuns = 2;
	CHECK(!HelpLayout::Parse("a\n<#a='y'>\nb\n<#a='y'>\nc", limited, {}, document));
	limited = Columns(60);
	limited.maxImages = 1;
	CHECK(!HelpLayout::Parse("<file='map' pos='L'>\n<file='map' pos='L'>", limited, Image, document));
	const auto huge = [](std::string_view) { return HelpLayout::ImageSize{1, (std::numeric_limits<int>::max)()}; };
	CHECK(!HelpLayout::Parse("<file='map' pos='L'>", Columns(60), huge, document));
	CHECK(!HelpLayout::Parse(std::string_view("x\0y", 3), Columns(60), {}, document));
	CHECK_EQ(size_t{1}, document.rowCount);
	CHECK_EQ(size_t{1}, document.text.size());
	CHECK(document.text.front().text == "kept");
}

TEST(HelpLayout, UnknownMarkupIsTextRatherThanAnImageNameSubstring)
{
	HelpLayout::Document document;
	CHECK(HelpLayout::Parse("<filename='keep'>\n<file-name='also keep'>\n<file='unfinished", Columns(60), Image, document));
	CHECK_EQ(size_t{3}, document.rowCount);
	CHECK_EQ(size_t{3}, document.text.size());
	if (document.text.size() == 3) {
		CHECK(document.text[0].text == "<filename='keep'>");
		CHECK(document.text[1].text == "<file-name='also keep'>");
		CHECK(document.text[2].text == "<file='unfinished");
	}
	CHECK(document.images.empty());
}
