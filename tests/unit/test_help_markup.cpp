#include "test_framework.h"
#include "HelpMarkup.h"

#include <string>

TEST(HelpMarkup, ReadsExactAttributeNamesAndBothQuoteStyles)
{
	const std::string tag = "<#color = '00ffAA' a=\"y\" file='quest map' pos='L'>";
	CHECK(HelpMarkup::Attribute(tag, "color") == "00ffAA");
	CHECK(HelpMarkup::Attribute(tag, "a") == "y");
	CHECK(HelpMarkup::Attribute(tag, "file") == "quest map");
	CHECK(HelpMarkup::Attribute(tag, "pos") == "L");
	// Older fragments allowed whitespace in place of the equals sign.
	CHECK(HelpMarkup::Attribute("<file 'map' pos\t'RT'>", "file") == "map");
	CHECK(HelpMarkup::Attribute("<file 'map' pos\t'RT'>", "pos") == "RT");
}

TEST(HelpMarkup, DoesNotMatchNameSubstringsOrNamesInsideValues)
{
	const std::string tag = "<data='bad' a-long='bad' name=\"a='fake'\" a='real'>";
	CHECK(HelpMarkup::Attribute(tag, "a") == "real");
	CHECK(HelpMarkup::Attribute("<filename='wrong' file='right'>", "file") == "right");
	CHECK(HelpMarkup::Attribute("<name=\"a='fake'\">", "a").empty());
	CHECK(HelpMarkup::Attribute(tag, "missing").empty());
	CHECK(HelpMarkup::Attribute(tag, "").empty());
	CHECK(HelpMarkup::Attribute(tag, "a='").empty());
}

TEST(HelpMarkup, MissingOrUnterminatedQuotesProduceEmptyValues)
{
	for (const auto* tag : {"", "<a", "<a=", "<a y>", "<a='", "<a='y", "<a=\"y'", "<a=>"})
		CHECK(HelpMarkup::Attribute(tag, "a").empty());
	CHECK(HelpMarkup::Attribute("<a=''>", "a").empty());
	const std::string tag = "<file='quest map'>";
	for (size_t size = 0; size < tag.find_last_of('\'') + 1; ++size)
		CHECK(HelpMarkup::Attribute(std::string_view(tag).substr(0, size), "file").empty());
	CHECK(HelpMarkup::Attribute(std::string_view(tag).substr(0, tag.size() - 1), "file") == "quest map");
}

TEST(HelpMarkup, LongUnicodeValuesAreOwnedAndHaveNoScratchBufferLimit)
{
	std::string expected;
	for (int i = 0; i < 4096; ++i) expected += "\xEA\xB0\x80";
	auto value = HelpMarkup::Attribute("<file='" + expected + "'>", "file");
	CHECK(value == expected);
	const auto other = HelpMarkup::Attribute("<file='other'>", "file");
	CHECK(other == "other");
	CHECK(value == expected);
}

TEST(HelpMarkup, UsesOnlyTheSuppliedSpan)
{
	const char complete[] = {'a', '=', '\'', 'y', '\''};
	CHECK(HelpMarkup::Attribute(std::string_view(complete, sizeof complete), "a") == "y");
	const char truncated[] = {'a', '=', '\'', 'y'};
	CHECK(HelpMarkup::Attribute(std::string_view(truncated, sizeof truncated), "a").empty());
	const char embedded[] = {'a', '=', '\'', 'x', '\0', 'y', '\''};
	CHECK(HelpMarkup::Attribute(std::string_view(embedded, sizeof embedded), "a") == std::string("x\0y", 3));
}
