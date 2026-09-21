#include "test_framework.h"
#include "DescriptorText.h"

#include <limits>

TEST(DescriptorText, WrapsUtf8WithoutScratchLimitsOrMissingColonArithmetic)
{
	DescriptorText::Layout layout(4);
	CHECK(layout.AppendLine("abcdefghi"));
	CHECK(layout.AppendLine("\xEA\xB0\x80\xF0\x9F\x99\x82Z"));
	std::vector<std::string> rows;
	std::string title;
	CHECK(layout.Complete(false, rows, title));
	CHECK((rows == std::vector<std::string>{"abcd", "efgh", "i", "\xEA\xB0\x80", "\xF0\x9F\x99\x82", "Z"}));
	DescriptorText::Layout longLine(3000);
	CHECK(longLine.AppendLine(std::string(4096, 'x')));
	CHECK(longLine.Complete(false, rows, title));
	CHECK((rows == std::vector<std::string>{std::string(3000, 'x'), std::string(1096, 'x')}));
}

TEST(DescriptorText, ContinuationIndentUsesOnlyAnEarlyColon)
{
	DescriptorText::Layout layout(8);
	CHECK(layout.AppendLine("A: abcdefgh"));
	CHECK(layout.AppendLine("abcde: xyz"));
	std::vector<std::string> rows;
	std::string title;
	CHECK(layout.Complete(false, rows, title));
	CHECK((rows == std::vector<std::string>{"A: abcde", "   fgh", "abcde: x", "yz"}));
}

TEST(DescriptorText, ImagesAndContinuationIndentNeverUnderflowTheColumn)
{
	DescriptorText::Layout layout(8);
	CHECK(layout.BeginImage(2, 2));
	CHECK(layout.AppendLine("A: abcdefgh"));
	std::vector<std::string> rows;
	std::string title;
	CHECK(layout.Complete(false, rows, title));
	CHECK((rows == std::vector<std::string>{"  A: abc", "     def", "   gh"}));
	DescriptorText::Layout wideImage(3);
	CHECK(wideImage.BeginImage(std::numeric_limits<size_t>::max(), 2));
	CHECK(wideImage.AppendLine("abcdef"));
	CHECK(wideImage.Complete(false, rows, title));
	CHECK((rows == std::vector<std::string>{" ", " ", "abc", "def"}));
	DescriptorText::Layout afterText(3);
	CHECK(afterText.AppendLine("top"));
	CHECK(afterText.BeginImage(3, 2));
	CHECK(afterText.AppendLine("end"));
	CHECK(afterText.Complete(false, rows, title));
	CHECK((rows == std::vector<std::string>{"top", " ", " ", "end"}));
}

TEST(DescriptorText, HeaderTabAndImageRowsKeepTheirSpacing)
{
	DescriptorText::Layout layout(20);
	CHECK(layout.AppendHeader("Header"));
	CHECK(layout.AppendLine("\tvalue"));
	CHECK(layout.AppendLine("body"));
	CHECK(layout.BeginImage(2, 2));
	CHECK(layout.AppendLine("x"));
	std::vector<std::string> rows;
	std::string title = "explicit";
	CHECK(layout.Complete(false, rows, title));
	CHECK(title == "explicit");
	CHECK((rows == std::vector<std::string>{"Header", "\tvalue", "", "body", "  x", " "}));
}

TEST(DescriptorText, EmptyTitlesAndBlankDocumentsAreSafe)
{
	std::vector<std::string> rows = {"old"};
	std::string title = "old title";
	DescriptorText::Layout empty(4);
	CHECK(empty.Complete(true, rows, title));
	CHECK(rows.empty());
	CHECK(title.empty());
	DescriptorText::Layout layout(8);
	CHECK(layout.AppendLine("Title"));
	CHECK(layout.AppendLine(""));
	CHECK(layout.AppendLine("body"));
	CHECK(layout.AppendLine(""));
	CHECK(layout.Complete(true, rows, title));
	CHECK(title == "Title");
	CHECK((rows == std::vector<std::string>{"body"}));
}

TEST(DescriptorText, LayoutLimitsRejectWithoutPublishingPartialResults)
{
	std::vector<std::string> rows = {"kept"};
	std::string title = "kept";
	DescriptorText::Layout invalid(0);
	CHECK(!invalid.AppendLine("x"));
	CHECK(!invalid.Complete(true, rows, title));
	DescriptorText::Layout rowLimit(1, 2, 20);
	CHECK(!rowLimit.AppendLine("abc"));
	CHECK(!rowLimit.Complete(true, rows, title));
	DescriptorText::Layout byteLimit(100, 10, 4);
	CHECK(!byteLimit.AppendHeader("12345"));
	CHECK(!byteLimit.Complete(true, rows, title));
	CHECK((rows == std::vector<std::string>{"kept"}));
	CHECK(title == "kept");
	DescriptorText::Layout hugeImage(1, 2, 20);
	CHECK(hugeImage.BeginImage(0, std::numeric_limits<size_t>::max()));
	CHECK(!hugeImage.Complete(false, rows, title));
}

TEST(DescriptorText, TrimmingReportsTheOffsetOfRelativeImages)
{
	DescriptorText::Layout layout(8);
	CHECK(layout.AppendLine("Title"));
	CHECK(layout.AppendLine(""));
	CHECK(layout.BeginImage(2, 2));
	CHECK_EQ(size_t{2}, layout.RowCount());
	CHECK(layout.AppendLine("body"));
	std::vector<std::string> rows;
	std::string title;
	size_t removed = 99;
	CHECK(layout.Complete(true, rows, title, &removed));
	CHECK_EQ(size_t{2}, removed);
	CHECK(title == "Title");
	CHECK((rows == std::vector<std::string>{"  body", " "}));
}

TEST(DescriptorText, ResourceIndicesAreCompleteNonnegativeIntegers)
{
	int index = -1;
	CHECK(DescriptorText::ReadIndex(" 123\r\n", index));
	CHECK_EQ(123, index);
	CHECK(DescriptorText::ReadIndex("0", index));
	CHECK_EQ(0, index);
	for (const auto* text : {"", " ", "-1", "+1", "1x", "999999999999999999999", "1 2"}) {
		index = 42;
		CHECK(!DescriptorText::ReadIndex(text, index));
		CHECK_EQ(42, index);
	}
	const char exact[] = {'4', '2'};
	CHECK(DescriptorText::ReadIndex(std::string_view(exact, sizeof exact), index));
	CHECK_EQ(42, index);
	CHECK(!DescriptorText::ReadIndex(std::string_view("1\0", 2), index));
}
