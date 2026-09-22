#include "test_framework.h"
#include "Platform.h"
#include "RarFile.h"
#include "TextEncoding.h"
#include "TextUtf8.h"
#include "ResourceText.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <limits>
#include <filesystem>
#include <memory>
#include <algorithm>

namespace {
struct ReaderFile {
	static constexpr const char* path = "resource_reader_test.bin";
	explicit ReaderFile(const std::string& bytes) {
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}
	~ReaderFile() { std::remove(path); }
};
struct EncodingScope {
	TextEncoding::Encoding saved = TextEncoding::GetResourceEncoding();
	~EncodingScope() { TextEncoding::SetResourceEncoding(saved); }
};

std::string ArchiveFixture(const char* name)
{
	return (std::filesystem::path(__FILE__).parent_path().parent_path() /
		"fixtures" / "rar" / name).string();
}
}

TEST(ResourceReader, PackedFilesReadExactBytesAndResolveWindowsMemberNames)
{
	const auto archive = ArchiveFixture("test_read_format_rar.rar");
	CRarFile reader(archive.c_str(), nullptr);
	CHECK(reader.Open("TESTDIR\\TEST.TXT"));
	CHECK_EQ(20, reader.GetRemainingSize());
	char bytes[21] = {};
	CHECK(reader.Read(bytes, 20) != nullptr);
	CHECK(std::string(bytes) == "test text document\r\n");
	CHECK(reader.IsEOF());
	CHECK(!reader.Open("absent.txt"));
	CHECK(!reader.IsSet());
}

TEST(ResourceReader, PackedListsAreOwnedFilteredAndContainOnlyRegularMembers)
{
	std::unique_ptr<std::vector<std::string>> list;
	{
		const auto archive = ArchiveFixture("test_read_format_rar.rar");
		CRarFile reader(archive.c_str(), nullptr);
		list.reset(reader.GetList());
		char filter[] = "TESTDIR/*.TXT";
		std::unique_ptr<std::vector<std::string>> filtered(reader.GetList(filter));
		CHECK_EQ(1, filtered->size());
		if (!filtered->empty()) CHECK((*filtered)[0] == "testdir/test.txt");
	}
	CHECK_EQ(2, list->size());
	CHECK(std::find(list->begin(), list->end(), "test.txt") != list->end());
	CHECK(std::find(list->begin(), list->end(), "testdir/test.txt") != list->end());
}

TEST(ResourceReader, PackedEncryptedDataAndHeadersUseTheSuppliedPassword)
{
	for (const char* fixture : {"test_read_format_rar_encryption_data.rar",
		"test_read_format_rar_encryption_header.rar"}) {
		const auto archive = ArchiveFixture(fixture);
		CRarFile reader(archive.c_str(), "12345678");
		CHECK(reader.Open("foo.txt"));
		CHECK_EQ(16, reader.GetRemainingSize());
		reader.SetRAR(archive.c_str(), "wrong");
		CHECK(!reader.Open("foo.txt"));
		CHECK(!reader.IsSet());
		reader.SetRAR(archive.c_str(), nullptr);
		CHECK(!reader.Open("foo.txt"));
		CHECK(!reader.IsSet());
	}
}

TEST(ResourceReader, PackedTextAndListingsPreserveTheCursorAndLooseOverrides)
{
	const auto archive = ArchiveFixture("test_read_format_rar.rar");
	CRarFile reader(archive.c_str(), nullptr);
	CHECK(reader.OpenText("test.txt"));
	char line[64] = {};
	std::unique_ptr<std::vector<std::string>> names(reader.GetList());
	CHECK(reader.GetString(line, sizeof(line)));
	CHECK(std::string(line) == "test text document");
	CHECK(reader.IsEOF());
	ReaderFile loose("override");
	// With an archive in the current directory, loose files still work even
	// if that archive is unavailable (the supported extracted-resource layout).
	reader.SetRAR("absent_archive.rar", "ignored");
	CHECK(reader.Open(ReaderFile::path));
	CHECK_EQ(8, reader.GetRemainingSize());
}

TEST(ResourceReader, RejectedReadsPreserveTheCursorAndDestination)
{
	ReaderFile file("abc");
	CRarFile reader;
	CHECK(reader.Open(ReaderFile::path));
	char* start = reader.GetFilePointer();
	CHECK_EQ(3, reader.GetRemainingSize());
	CHECK(reader.Read(4) == nullptr);
	CHECK(reader.GetFilePointer() == start);
	// Restore the fixture before checking another rejected request on the old
	// implementation, whose unchecked read has already advanced its cursor.
	CHECK(reader.Open(ReaderFile::path));
	start = reader.GetFilePointer();
	CHECK(reader.Read(-1) == nullptr);
	CHECK(reader.GetFilePointer() == start);
	CHECK(reader.Open(ReaderFile::path));
	char output[8] = "kept";
	start = reader.GetFilePointer();
	CHECK(reader.Read(nullptr, 1) == nullptr);
	CHECK(reader.Read(output, -1) == nullptr);
	CHECK(reader.Read((std::numeric_limits<int>::max)()) == nullptr);
	CHECK(reader.Read(output, 4) == nullptr);
	CHECK(std::string(output) == "kept");
	CHECK(reader.GetFilePointer() == start);
	CHECK(reader.Read(output, 3) != nullptr);
	CHECK(std::string(output, 3) == "abc");
	CHECK_EQ(0, reader.GetRemainingSize());
	CHECK(reader.IsEOF());
	CHECK(reader.Read(1) == nullptr);
}

TEST(ResourceReader, FailedOpensAndSourceChangesCannotExposeOldData)
{
	ReaderFile file("data");
	CRarFile reader;
	CHECK(reader.Open(ReaderFile::path));
	CHECK(!reader.Open(nullptr));
	CHECK(!reader.IsSet());
	CHECK(reader.GetFilePointer() == nullptr);
	CHECK(reader.IsEOF());
	CHECK(reader.Open(ReaderFile::path));
	reader.SetRAR("unused.rpk", nullptr);
	CHECK(!reader.IsSet());
	CHECK(reader.Open(ReaderFile::path));
	reader.Release();
	reader.Release();
	CHECK(reader.Read(1) == nullptr);
}

TEST(ResourceReader, EmptyFilesHaveAValidTerminatorAndTextFilesHaveASizeLimit)
{
	CRarFile reader;
	{
		ReaderFile file("");
		CHECK(reader.OpenText(ReaderFile::path));
		CHECK(reader.IsSet());
		CHECK(reader.IsEOF());
		CHECK(reader.GetFilePointer() && *reader.GetFilePointer() == '\0');
		reader.Release();
	}
	{
		ReaderFile file(std::string(ResourceText::MaxFileBytes + 1, 'a'));
		CHECK(!reader.OpenText(ReaderFile::path));
		CHECK(!reader.IsSet());
		CHECK(reader.Open(ReaderFile::path));
		CHECK(reader.Read(1) != nullptr);
	}
}

TEST(ResourceReader, DamagedTextIsReplacedWithoutLosingFollowingLines)
{
	EncodingScope scope;
	TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Utf8);
	ReaderFile file("\xFF!\nnext\n");
	CRarFile reader;
	CHECK(reader.OpenText(ReaderFile::path));
	char line[8];
	CHECK(reader.GetString(line, sizeof(line)));
	CHECK(std::string(line) == "\xEF\xBF\xBD!");
	char* next = reader.GetFilePointer();
	CHECK(!reader.GetString(nullptr, 4));
	CHECK(!reader.GetString(line, 0));
	CHECK(reader.GetFilePointer() == next);
	CHECK(reader.GetString(line, sizeof(line)));
	CHECK(std::string(line) == "next");
	CHECK(reader.IsEOF());
}

TEST(ResourceReader, FileListsBelongToTheCallerAndOutliveTheReader)
{
	std::vector<std::string>* list = nullptr;
	{
		CRarFile reader;
		list = reader.GetList();
		CHECK(list != nullptr);
	}
	if (list) {
		CHECK(list->empty());
		delete list;
	}
}

TEST(ResourceReader, RawLinesKeepBytesAndAlwaysAdvanceByTheWholeLine)
{
	ReaderFile file(std::string("\xC7\xD1-long\r\nnext\nlast"));
	CRarFile reader;
	CHECK(reader.Open(ReaderFile::path));
	char line[6];
	CHECK(reader.GetString(line, 4));
	CHECK(std::string(line) == "\xC7\xD1-");
	CHECK(reader.GetString(line, sizeof(line)));
	CHECK(std::string(line) == "next");
	CHECK(reader.GetString(line, sizeof(line)));
	CHECK(std::string(line) == "last");
	CHECK(!reader.GetString(line, sizeof(line)));
	CHECK(line[0] == '\0');
}

TEST(ResourceReader, TextIsDecodedOnceAndLineClippingPreservesUtf8Scalars)
{
	EncodingScope scope;
	TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Cp949);
	ReaderFile file("\xC7\xD1\xC2\xA1\r\nnext");
	CRarFile reader;
	CHECK(reader.OpenText(ReaderFile::path));
	TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Gbk);
	char line[8];
	CHECK(reader.GetString(line, 5));
	CHECK(std::string(line) == "\xED\x95\x9C");
	CHECK(TextSystem::IsValidUtf8(line, std::char_traits<char>::length(line)));
	CHECK(reader.GetString(line, sizeof(line)));
	CHECK(std::string(line) == "next");
}

TEST(ResourceReader, Utf8BomOverridesThePackAndTinyBuffersStillAdvance)
{
	EncodingScope scope;
	TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Cp949);
	ReaderFile file("\xEF\xBB\xBF\xF0\x9F\x98\x80!\nnext");
	CRarFile reader;
	CHECK(reader.OpenText(ReaderFile::path));
	char line[8];
	CHECK(reader.GetString(line, 4));
	CHECK(std::string(line).empty());
	CHECK(reader.GetString(line, sizeof(line)));
	CHECK(std::string(line) == "next");
	CHECK(reader.OpenText(ReaderFile::path));
	CHECK(reader.GetString(line, 5));
	CHECK(std::string(line) == "\xF0\x9F\x98\x80");
	CHECK(reader.OpenText(ReaderFile::path));
	CHECK(reader.GetString(line, 1));
	CHECK(line[0] == '\0');
	CHECK(reader.GetString(line, sizeof(line)));
	CHECK(std::string(line) == "next");
}
