#include "test_framework.h"
#include "ResourceText.h"
#include "TextEncoding.h"

namespace {
struct EncodingScope {
	TextEncoding::Encoding saved = TextEncoding::GetResourceEncoding();
	~EncodingScope() { TextEncoding::SetResourceEncoding(saved); }
};
}

TEST(ResourceText, UsesTheConfiguredPageWithoutSniffingThePayload)
{
	EncodingScope scope;
	TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Cp949);
	std::string output;
	CHECK(ResourceText::Decode("\xC2\xA1", output));
	CHECK(output == "\xEC\xA7\x95");
	CHECK(ResourceText::Decode("\xEF\xBB\xBF\xC2\xA1", output));
	CHECK(output == "\xC2\xA1");
	CHECK(ResourceText::Decode("\xC7\xD1\xFF!", output));
	CHECK(output == "\xED\x95\x9C\xEF\xBF\xBD!");
}

TEST(ResourceText, XmlDeclarationsOverrideTheResourcePackDefault)
{
	EncodingScope scope;
	TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Cp949);
	std::string output;
	CHECK(ResourceText::Decode("<?xml version='1.0' encoding = 'GBK'?><a>\xC4\xE3\xBA\xC3</a>", output, true));
	CHECK(output == "<a>\xE4\xBD\xA0\xE5\xA5\xBD</a>");
	CHECK(ResourceText::Decode("<?xml version=\"1.0\" encoding=\"UTF-8\"?><a>\xC2\xA1</a>", output, true));
	CHECK(output == "<a>\xC2\xA1</a>");
	CHECK(ResourceText::Decode("<a>\xC7\xD1</a>", output, true));
	CHECK(output == "<a>\xED\x95\x9C</a>");
}

TEST(ResourceText, InvalidDeclarationsAndOversizedFilesDoNotPublishPartialText)
{
	std::string output("kept");
	for (const auto* bytes : {"<?xml encoding='unknown'?><a/>", "<?xml encoding='GBK><a/>",
		"<?xml encoding='GBK' encoding='UTF-8'?><a/>",
		"\xEF\xBB\xBF<?xml encoding='CP949'?><a/>"}) {
		CHECK(!ResourceText::Decode(bytes, output, true));
		CHECK(output == "kept");
	}
	const std::string huge(ResourceText::MaxFileBytes + 1, 'x');
	CHECK(!ResourceText::Decode(huge, output));
	CHECK(output == "kept");
}
