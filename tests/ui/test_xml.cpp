#include "test_framework.h"
#include "Platform.h"
#include "SXml.h"

#include <string>
#include <cstdio>
#include <fstream>
#include <iterator>

TEST(XmlLibrary, ParsesANestedTreeWithAttributes)
{
	XMLParser parser;
	XMLTree tree;
	std::string document("<root><child name='value'>text</child></root>");
	parser.parse(document.data(), &tree);
	CHECK(tree.GetName() == "root");
	CHECK_EQ(1, tree.GetChildCount());
	const auto* child = tree.GetChild("child");
	CHECK(child != nullptr);
	if (child) {
		CHECK(child->GetParent() == &tree);
		CHECK(child->GetText() == "text");
		const auto* name = child->GetAttribute("name");
		CHECK(name != nullptr);
		if (name) CHECK(std::string(name->ToString()) == "value");
	}
}

TEST(XmlLibrary, ExistingWideConversionGuardsLinkFromTheRealLibrary)
{
	CHECK(XMLUtil::WideCharToString(L"abc") == "abc");
	CHECK(XMLUtil::WideCharToString(nullptr).empty());
	CHECK(XMLUtil::WideCharToString(L"abc", -2).empty());
	CHECK(XMLUtil::WideCharToString(L"abc", 2) == "ab");
}

TEST(XmlLibrary, WideConversionProducesUtf8WithoutAFixedBuffer)
{
	CHECK_EQ(5119, XMLUtil::WideCharToString(std::wstring(5119, L'x').c_str()).size());
	CHECK_EQ(5120, XMLUtil::WideCharToString(std::wstring(5120, L'x').c_str()).size());
	CHECK_EQ(50000, XMLUtil::WideCharToString(std::wstring(50000, L'x').c_str()).size());
	CHECK(XMLUtil::WideCharToString(L"\uD55C\U0001F600") == "\xED\x95\x9C\xF0\x9F\x98\x80");
}

TEST(XmlLibrary, ResourceTextAndAttributesHaveNoFixedStackBuffer)
{
	XMLParser parser;
	XMLTree tree;
	std::string text;
	for (int i = 0; i < 700; ++i) text += "\xED\x95\x9C";
	std::string document = "<?xml version='1.0' encoding='UTF-8'?><root><child name='" +
		text + "'>" + text + "</child></root>";
	parser.parse(document.data(), &tree);
	const auto* child = tree.GetChild("child");
	CHECK(child != nullptr);
	if (child) {
		CHECK(child->GetText() == text);
		const auto* name = child->GetAttribute("name");
		CHECK(name != nullptr);
		if (name) CHECK(std::string(name->ToString()) == text);
	}
}

TEST(XmlLibrary, TruncatedAndMalformedDocumentsPreserveTheExistingTree)
{
	XMLParser parser;
	const std::string complete("<root a='value'><child>text</child></root>");
	for (size_t size = 0; size < complete.size(); ++size) {
		XMLTree tree("kept");
		tree.AddChild("old");
		std::string truncated = complete.substr(0, size);
		parser.parse(truncated.data(), &tree);
		CHECK(tree.GetName() == "kept");
		CHECK_EQ(1, tree.GetChildCount());
		CHECK(tree.GetChild("old") != nullptr);
	}
	for (const auto* text : {"<a></b>", "<a x='one' x='two'/>", "<a>&unknown;</a>",
		"<a>&#xD800;</a>", "<a><b/></c>", "<a><!-- unfinished</a>", "<!DOCTYPE a><a/>"}) {
		XMLTree tree("kept");
		std::string document(text);
		parser.parse(document.data(), &tree);
		CHECK(tree.GetName() == "kept");
		CHECK_EQ(0, tree.GetChildCount());
	}
}

TEST(XmlLibrary, QuestDocumentsAppendDuplicateChildrenWithValidParents)
{
	XMLParser parser;
	XMLTree tree;
	std::string first("<quests><quest id='1'>first</quest></quests>");
	std::string second("<quests><quest id='2'>second</quest></quests>");
	parser.parse(first.data(), &tree, true);
	parser.parse(second.data(), &tree, true);
	CHECK_EQ(2, tree.GetChildCount());
	if (tree.GetChildCount() == 2) {
		CHECK(tree.GetChild(size_t(0))->GetText() == "first");
		CHECK(tree.GetChild(size_t(1))->GetText() == "second");
		CHECK(tree.GetChild(size_t(1))->GetParent() == &tree);
		CHECK(tree.GetChild("quest") == tree.GetChild(size_t(0)));
	}
}

TEST(XmlLibrary, XmlMarkupPreservesQuotedDelimitersCommentsAndCharacterReferences)
{
	XMLParser parser;
	XMLTree tree;
	std::string document("<?xml version='1.0' encoding='UTF-8'?><?xml-stylesheet href='unused'?><!--before--><root a='1>0 &amp; 2&lt;3'>"
		"A&amp;B&#x1F600;<![CDATA[<literal>]]><!--ignored--></root>");
	parser.parse(document.data(), &tree);
	CHECK(tree.GetName() == "root");
	CHECK(tree.GetText() == "A&B\xF0\x9F\x98\x80<literal>");
	const auto* attr = tree.GetAttribute("a");
	CHECK(attr != nullptr);
	if (attr) CHECK(std::string(attr->ToString()) == "1>0 & 2<3");
}

TEST(XmlLibrary, SavedUtf8TextAndAttributesRoundTripThroughTheRealParser)
{
	constexpr const char* path = "xml_roundtrip_test.xml";
	struct Cleanup { ~Cleanup() { std::remove("xml_roundtrip_test.xml"); } } cleanup;
	const std::string content("\xED\x95\x9C <&> \"quoted\"");
	XMLTree source("root");
	source.SetText(content);
	source.AddAttribute("value", "'\"<&>");
	source.Save(path);
	std::ifstream file(path, std::ios::binary);
	std::string document((std::istreambuf_iterator<char>(file)), {});
	CHECK(document.find("encoding=\"UTF-8\"") != std::string::npos);
	XMLTree parsed;
	XMLParser parser;
	parser.parse(document.data(), &parsed);
	CHECK(parsed.GetName() == "root");
	CHECK(parsed.GetText() == content);
	const auto* value = parsed.GetAttribute("value");
	CHECK(value != nullptr);
	if (value) CHECK(std::string(value->ToString()) == "'\"<&>");
}

TEST(XmlLibrary, DeclaredLegacyXmlConvertsOnceAndExcessiveNestingIsRejected)
{
	XMLTree tree;
	XMLParser parser;
	std::string korean("<?xml version='1.0' encoding='CP949'?><root>\xC7\xD1</root>");
	parser.parse(korean.data(), &tree);
	CHECK(tree.GetText() == "\xED\x95\x9C");
	std::string deep;
	for (int i = 0; i < 100; ++i) deep += "<a>";
	for (int i = 0; i < 100; ++i) deep += "</a>";
	parser.parse(deep.data(), &tree);
	CHECK(tree.GetName() == "root");
	CHECK(tree.GetText() == "\xED\x95\x9C");
	CHECK_EQ(0, tree.GetChildCount());
}

TEST(XmlLibrary, DepthAndNodeBudgetsAreInclusiveAndRejectedLoadsPreserveTheTree)
{
	XMLParser parser;
	for (const int depth : {64, 65}) {
		std::string document;
		for (int i = 0; i < depth; ++i) document += "<node>";
		for (int i = 0; i < depth; ++i) document += "</node>";
		XMLTree tree("kept");
		parser.parse(document.data(), &tree);
		CHECK(tree.GetName() == (depth == 64 ? "node" : "kept"));
	}
	std::string document("<root>");
	for (int i = 0; i < 99999; ++i) document += "<node/>";
	document += "</root>";
	XMLTree tree;
	parser.parse(document.data(), &tree, true);
	CHECK_EQ(99999, tree.GetChildCount());
	document.insert(document.size() - 7, "<node/>");
	parser.parse(document.data(), &tree, true);
	CHECK_EQ(99999, tree.GetChildCount());
}
