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
