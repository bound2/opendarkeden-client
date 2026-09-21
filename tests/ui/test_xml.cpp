#include "test_framework.h"
#include "Platform.h"
#include "SXml.h"

#include <string>

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

TEST(XmlLibrary, ExistingWideConversionReservesItsTerminator)
{
	CHECK_EQ(5119, XMLUtil::WideCharToString(std::wstring(5119, L'x').c_str()).size());
	CHECK(XMLUtil::WideCharToString(std::wstring(5120, L'x').c_str()).empty());
	CHECK(XMLUtil::WideCharToString(std::wstring(50000, L'x').c_str()).empty());
}
