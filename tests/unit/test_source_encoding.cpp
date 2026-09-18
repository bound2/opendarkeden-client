#include "test_framework.h"
#include "source_encoding_bom.h"

#include <string>

// Narrow literals must have the same bytes regardless of the source BOM or
// the developer's Windows code page. These are data fixtures, not UI labels.
TEST(SourceEncoding, NarrowLiteralsUseUtf8)
{
	const std::string expected = "\xED\x95\x9C";
	CHECK(expected == std::string("\uD55C"));
	CHECK(expected == std::string("한"));
	CHECK(expected == std::string(source_encoding_bom));
}
