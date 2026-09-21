#include "test_framework.h"
#include "Platform.h"
#include "MHelpMessageManager.h"
#include "TextEncoding.h"
#include "ResourceText.h"
#include "RarFile.h"
#include "VS_UI_filepath.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <filesystem>
#include <iterator>
#include <stdexcept>

namespace {
struct HelpEncodingScope {
	TextEncoding::Encoding previous = TextEncoding::GetResourceEncoding();
	HelpEncodingScope() { TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Utf8); }
	~HelpEncodingScope() { TextEncoding::SetResourceEncoding(previous); }
};
struct HelpFile {
	static constexpr const char* path = "help_messages_test.txt";
	explicit HelpFile(const std::string& bytes) {
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}
	~HelpFile() { std::remove(path); }
};
std::string HelpDocument(const std::string& levels = "1 99 2 98 3 97")
{
	std::string text = "1\n[===Sender===]\nGuide\n1\n[===KeyWord===]\nintro\n"
		"[===MessageType===]\n1\n[===Event===]\nstart\n";
	for (int race = 0; race < RACE_MAX; ++race) {
		text += "[==Title==]\nTitle " + std::to_string(race) + "\n[==Sender==]\n0\n";
		text += "[==Level 조건표==]\n" + levels + "\n[==Detail==]\nFirst\n  Second\n{End}\n";
	}
	return text;
}
}

TEST(HelpMessages, ProductionMessageDefaultsLinkFromTheUiLibrary)
{
	MHelpMessage message;
	CHECK_EQ(MHelpMessage::MESSAGETYPE_NORMAL, message.m_messageType);
	CHECK_EQ(0, message.m_strKeyword.GetLength());
	CHECK_EQ(0, message.m_strEvent.GetLength());
	for (int race = 0; race < RACE_MAX; ++race) {
		CHECK_EQ(-1, message.m_iSender[race]);
		CHECK_EQ(-1, message.m_iLevelLow[race]);
		CHECK_EQ(-1, message.m_iLevelMax[race]);
		CHECK_EQ(-1, message.m_iDomainLow[race]);
		CHECK_EQ(-1, message.m_iDomainMax[race]);
		CHECK_EQ(-1, message.m_iAttrLow[race]);
		CHECK_EQ(-1, message.m_iAttrMax[race]);
		CHECK_EQ(0, message.m_strTitle[race].GetLength());
		CHECK_EQ(0, message.m_strDetail[race].GetLength());
	}
}

TEST(HelpMessages, LegacyStreamLoadsAllThreeRacePages)
{
	HelpEncodingScope scope;
	HelpFile file(HelpDocument());
	MHelpMessageManager manager;
	manager.LoadFromFile(HelpFile::path);
	CHECK_EQ(1, manager.getSenderSize());
	CHECK_EQ(1, manager.getMessageSize());
	if (manager.getMessageSize() == 1) {
		const auto& message = manager.getMessage(0);
		CHECK(std::string(message.m_strKeyword.GetString()) == "intro");
		for (int race = 0; race < RACE_MAX; ++race) {
			CHECK_EQ(3, message.m_iAttrLow[race]);
			CHECK_EQ(97, message.m_iAttrMax[race]);
			CHECK(std::string(message.m_strDetail[race].GetString()) == "First\r\n  Second");
		}
	}
}

TEST(HelpMessages, InvalidLevelRowsPreserveThePreviouslyLoadedCollection)
{
	HelpEncodingScope scope;
	MHelpMessageManager manager;
	CHECK(manager.LoadFromText(HelpDocument()));
	for (const auto* row : {"", "1 2", "1 2 3 4 5", "1 2 3 4 5 6 7",
		"1 2 3 4 5 6 7 8 9 10 11", "2147483648 2 3 4 5 6", "one 2 3 4 5 6",
		"-2 2 3 4 5 6", "3 2 3 4 5 6"}) {
		CHECK(!manager.LoadFromText(HelpDocument(row)));
		CHECK_EQ(1, manager.getMessageSize());
		if (manager.getMessageSize() == 1) CHECK_EQ(97, manager.getMessage(0).m_iAttrMax[0]);
	}
}

TEST(HelpMessages, TruncationInvalidCountsAndExtraPagesDoNotPublishPartialData)
{
	HelpEncodingScope scope;
	MHelpMessageManager manager;
	const std::string good = HelpDocument();
	CHECK(manager.LoadFromText(good));
	for (size_t size = 0; size < good.size() - 1; ++size) {
		CHECK(!manager.LoadFromText(std::string_view(good).substr(0, size)));
		CHECK_EQ(1, manager.getSenderSize());
		CHECK_EQ(1, manager.getMessageSize());
	}
	for (const auto* first : {"-1", "65537", "2147483648", "bad"}) {
		CHECK(!manager.LoadFromText(std::string(first) + good.substr(1)));
		CHECK_EQ(1, manager.getMessageSize());
	}
	CHECK(!manager.LoadFromText(good + "[==Detail==]\nextra\n{End}\n"));
	std::string damaged = good;
	damaged.insert(damaged.find("intro"), 1, '\0');
	CHECK(!manager.LoadFromText(damaged));
	damaged = good;
	damaged.replace(damaged.find("[==Sender==]\n0"), 14, "[==Sender==]\n9");
	CHECK(!manager.LoadFromText(damaged));
	CHECK(!manager.LoadFromText(std::string(ResourceText::MaxFileBytes + 1, 'x')));
	CHECK_EQ(1, manager.getMessageSize());
}

TEST(HelpMessages, LongEncodedFieldsAndRepeatedLoadsRetainWholeText)
{
	HelpEncodingScope scope;
	MHelpMessageManager manager;
	std::string good = HelpDocument();
	const std::string title(10000, 'x');
	good.replace(good.find("Title 0"), 7, title + "한");
	std::string encoded;
	CHECK(TextEncoding::Convert(good, TextEncoding::Encoding::Utf8, TextEncoding::Encoding::Cp949, encoded));
	TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Cp949);
	CHECK(manager.LoadFromText(encoded));
	CHECK(manager.LoadFromText(encoded));
	CHECK_EQ(1, manager.getMessageSize());
	if (manager.getMessageSize() == 1) CHECK(std::string(manager.getMessage(0).m_strTitle[0].GetString()) == title + "한");
	CHECK(manager.LoadFromText("\xEF\xBB\xBF" + good));
	CHECK(manager.LoadFromText("0\n[===Sender===]\n0\n"));
	CHECK_EQ(0, manager.getMessageSize());
	CHECK_EQ(0, manager.getSenderSize());
}

TEST(HelpMessages, SavedUtf8DocumentsPreserveDistinctBoundsAndBlankDetailLines)
{
	HelpEncodingScope scope;
	MHelpMessageManager manager;
	std::string good = HelpDocument();
	good.replace(good.find("Guide"), 5, "안내");
	good.replace(good.find("First\n  Second"), 14, "First\n  Second\n");
	CHECK(manager.LoadFromText(good));
	HelpFile output("old");
	manager.SaveToFile(HelpFile::path);
	std::ifstream stream(HelpFile::path, std::ios::binary);
	std::string bytes((std::istreambuf_iterator<char>(stream)), {});
	CHECK(bytes.starts_with("\xEF\xBB\xBF"));
	TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Gbk);
	MHelpMessageManager loaded;
	loaded.LoadFromFile(HelpFile::path);
	CHECK_EQ(1, loaded.getMessageSize());
	CHECK_EQ(1, loaded.getSenderSize());
	if (loaded.getMessageSize() == 1 && loaded.getSenderSize() == 1) {
		CHECK(std::string(loaded.getSender(0).GetString()) == "안내");
		CHECK_EQ(3, loaded.getMessage(0).m_iAttrLow[0]);
		CHECK_EQ(97, loaded.getMessage(0).m_iAttrMax[0]);
		CHECK(std::string(loaded.getMessage(0).m_strDetail[0].GetString()) == "First\r\n  Second\r\n");
	}
}

TEST(HelpMessages, StreamFailuresAndInvalidLookupIndicesAreExplicit)
{
	HelpEncodingScope scope;
	MHelpMessageManager manager;
	CHECK(manager.LoadFromText(HelpDocument()));
	{
		HelpFile file("truncated");
		std::ifstream stream(HelpFile::path, std::ios::binary);
		manager.LoadFromFile(stream);
		CHECK(stream.fail());
		CHECK_EQ(1, manager.getMessageSize());
	}
	{
		HelpFile file(HelpDocument());
		std::ifstream stream(HelpFile::path, std::ios::binary);
		manager.LoadFromFile(stream);
		CHECK(!stream.fail());
		CHECK_EQ(1, manager.getMessageSize());
	}
	for (int index : {-1, 1, 999}) {
		bool senderRejected = false, messageRejected = false;
		try { manager.getSender(index); } catch (const std::out_of_range&) { senderRejected = true; }
		try { manager.getMessage(index); } catch (const std::out_of_range&) { messageRejected = true; }
		CHECK(senderRejected);
		CHECK(messageRejected);
	}
}

TEST(HelpMessages, ExtractedResourceLoaderUsesItsOpenedFileWithoutSingletonRecursion)
{
	HelpEncodingScope scope;
	const auto previous = std::filesystem::current_path();
	std::filesystem::path directory;
	for (int i = 0; i < 1000; ++i) {
		auto candidate = std::filesystem::temp_directory_path() / ("opendarkeden-help-test-" + std::to_string(i));
		if (std::filesystem::create_directory(candidate)) { directory = candidate; break; }
	}
	if (directory.empty()) throw std::runtime_error("cannot create help fixture directory");
	std::filesystem::current_path(directory);
	const std::filesystem::path resource = std::filesystem::path(TXT_ROOT) / "helpmessage.txt";
	std::filesystem::create_directories(resource.parent_path());
	{
		std::ofstream file(resource, std::ios::binary);
		file << "\xEF\xBB\xBF" << HelpDocument();
	}
	MHelpMessageManager manager;
	CHECK(manager.LoadHelpMessageRpk("helpmessage.txt"));
	CHECK_EQ(1, manager.getMessageSize());
	// This is the first singleton access in this test binary. Its default
	// resource exists, so recursive initialization would fail here.
	CHECK_EQ(1, MHelpMessageManager::Instance().getMessageSize());
	CHECK(!manager.LoadHelpMessageRpk("missing.txt"));
	CHECK_EQ(1, manager.getMessageSize());
	std::filesystem::remove(resource);
	std::filesystem::remove(resource.parent_path());
	std::filesystem::remove(resource.parent_path().parent_path());
	std::filesystem::remove(resource.parent_path().parent_path().parent_path());
	std::filesystem::current_path(previous);
	std::filesystem::remove(directory);
}
