// The English overrides of the packed UI text (tools/i18n/ui-text, written
// by tools/i18n/uitext_apply.pl) are loose files the client prefers over
// the Korean members of the Data/Ui/txt archives. They are shipped as is,
// so this checks that every one of them is what the loaders expect: ASCII
// throughout (the client decodes loose text as CP949, which ASCII is a
// subset of), the help tips parse with the production parser, every mail
// template parses, and the quest lists are well-formed XML.
#include "test_framework.h"
#include "Platform.h"
#include "MHelpMessageManager.h"
#include "MailTemplate.h"
#include "SXml.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifndef UITEXT_DIR
#error "UITEXT_DIR must name tools/i18n/ui-text/Data/Ui/txt"
#endif

namespace {

std::string ReadFile(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool IsAscii(const std::string& bytes)
{
	for (unsigned char c : bytes) {
		if (c >= 0x80 || c == 0) return false;
	}
	return true;
}

std::vector<std::filesystem::path> Overrides()
{
	std::vector<std::filesystem::path> files;
	for (const auto& entry : std::filesystem::directory_iterator(UITEXT_DIR)) {
		if (entry.is_regular_file()) files.push_back(entry.path());
	}
	return files;
}

// A mail template (GameUI.cpp's notice handler opens them by name) starts
// with its window size: two integers on the first line. Briefing.txt and
// Computer.tre start the same way but are a description and a tree.
bool LooksLikeMailTemplate(const std::filesystem::path& path, const std::string& bytes)
{
	const std::string name = path.filename().string();
	if (name == "Briefing.txt" || name == "Computer.tre") return false;
	const size_t end = bytes.find_first_of("\r\n");
	const std::string first = bytes.substr(0, end);
	int width = 0, height = 0;
	char extra = 0;
	return sscanf(first.c_str(), "%d %d %c", &width, &height, &extra) == 2;
}

} // namespace

TEST(UiTextOverrides, EveryOverrideIsNonEmptyAscii)
{
	const auto files = Overrides();
	CHECK(files.size() > 1000);
	size_t ascii = 0;
	for (const auto& path : files) {
		const std::string bytes = ReadFile(path);
		if (!bytes.empty() && IsAscii(bytes)) ++ascii;
	}
	CHECK_EQ(files.size(), ascii);
}

TEST(UiTextOverrides, HelpTipsParseWithTheProductionParser)
{
	const std::string bytes = ReadFile(std::filesystem::path(UITEXT_DIR) / "helpmessage.txt");
	CHECK(!bytes.empty());
	// The file is ASCII, which the resource decoding (CP949 by default)
	// passes through unchanged, exactly as the client's loader sees it.
	MHelpMessageManager manager;
	CHECK(manager.LoadFromText(bytes));
	CHECK(manager.getSenderSize() > 0);
	CHECK(manager.getMessageSize() > 0);
}

TEST(UiTextOverrides, EveryMailTemplateParses)
{
	size_t templates = 0, parsed = 0;
	for (const auto& path : Overrides()) {
		const std::string bytes = ReadFile(path);
		if (!LooksLikeMailTemplate(path, bytes)) continue;
		++templates;
		MailTemplate::Data mail;
		if (MailTemplate::Parse(bytes, 1024, 768, mail) && !mail.sender.empty() && !mail.contents.empty()) ++parsed;
	}
	CHECK(templates > 10);
	CHECK_EQ(templates, parsed);
}

TEST(UiTextOverrides, QuestListsAreWellFormedXml)
{
	for (const char* name : {"SimpleGQuest.xml", "EventGQuest.xml"}) {
		std::string bytes = ReadFile(std::filesystem::path(UITEXT_DIR) / name);
		CHECK(!bytes.empty());
		// The quest manager parses these with repeated sibling names allowed
		// (C_VS_UI_QUEST_MANAGER::LoadQuestXML): one <Quest> per quest.
		XMLParser parser;
		XMLTree tree;
		parser.parse(bytes.data(), &tree, true);
		CHECK(tree.GetName() == "QuestList");
		CHECK(tree.GetChildCount() > 0);
	}
}
