#include "test_framework.h"
#include "MailTemplate.h"
#include "ResourceText.h"
#include "TextEncoding.h"

TEST(MailTemplate, OwnsAllFourRowsAndKeepsLongUtf8Contents)
{
	for (const std::string separator : {"\r\n", "\n", "\r"}) {
		std::string body(8192, 'x');
		body += "\xEA\xB0\x80 100%\\nnext";
		std::string source = "400 300" + separator + "sender" + separator + "title" + separator + body;
		MailTemplate::Data result;
		CHECK(MailTemplate::Parse(source, 800, 600, result));
		source.assign(16384, 'z');
		CHECK_EQ(400, result.width);
		CHECK_EQ(300, result.height);
		CHECK(result.sender == "sender");
		CHECK(result.title == "title");
		CHECK(result.contents == body);
	}
}

TEST(MailTemplate, GeometryRequiresExactlyTwoPositiveDimensionsWithinTheViewport)
{
	MailTemplate::Data result;
	CHECK(MailTemplate::Parse(" +800\t600 \nfrom\nsubject\nbody", 800, 600, result));
	for (const std::string header : {"", "400", "400 300 junk", "400 300 1", "0 300", "400 -1",
		"801 300", "400 601", "2147483648 1", "1 999999999999999999999", "4x 300", "+ 300"}) {
		CHECK(!MailTemplate::Parse(header + "\nnew\nnew\nnew", 800, 600, result));
		CHECK_EQ(800, result.width);
		CHECK(result.contents == "body");
	}
	CHECK(!MailTemplate::Parse("1 1\na\nb\nc", 0, 600, result));
	CHECK(!MailTemplate::Parse("1 1\na\nb\nc", 800, -1, result));
}

TEST(MailTemplate, IncompleteAndExtraRecordsDoNotPublishPartialTemplates)
{
	MailTemplate::Data result;
	CHECK(MailTemplate::Parse("400 300\nsender\ntitle\nbody", 800, 600, result));
	for (const std::string text : {"", "400 300", "400 300\n", "400 300\na\n", "400 300\na\nb\n",
		"400 300\na\nb\nc\nextra"}) {
		CHECK(!MailTemplate::Parse(text, 800, 600, result));
		CHECK(result.title == "title");
	}
	CHECK(!MailTemplate::Parse(std::string("400 300\na\nb\nc\0hidden", 21), 800, 600, result));
	CHECK(MailTemplate::Parse("1 1\n\n\n\n \t\r\n", 800, 600, result));
	CHECK(result.sender.empty());
	CHECK(result.title.empty());
	CHECK(result.contents.empty());
	CHECK(!MailTemplate::Parse(std::string(ResourceText::MaxFileBytes + 1, 'x'), 800, 600, result));
}

TEST(MailTemplate, DeclaredResourceDecodingPrecedesParsingAndBomOverridesThePack)
{
	const auto previous = TextEncoding::GetResourceEncoding();
	CHECK(TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Cp949));
	std::string text;
	CHECK(ResourceText::Decode("400 300\n\xC7\xD1\ntitle\n\xC7\xD1", text));
	MailTemplate::Data result;
	CHECK(MailTemplate::Parse(text, 800, 600, result));
	CHECK(result.sender == "\xED\x95\x9C");
	CHECK(result.contents == "\xED\x95\x9C");
	CHECK(ResourceText::Decode("\xEF\xBB\xBF" "400 300\n\xEA\xB0\x80\ntitle\nbody", text));
	CHECK(MailTemplate::Parse(text, 800, 600, result));
	CHECK(result.sender == "\xEA\xB0\x80");
	CHECK(TextEncoding::SetResourceEncoding(previous));
}
