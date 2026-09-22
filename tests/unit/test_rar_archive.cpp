#include "test_framework.h"
#include "RarArchive.h"
#include "TextUtf8.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {
std::string Fixture(const char* name)
{
	return (std::filesystem::path(__FILE__).parent_path().parent_path() /
		"fixtures" / "rar" / name).string();
}

struct ChangedArchive {
	static constexpr const char* path = "changed_archive_test.rar";
	~ChangedArchive() { std::filesystem::remove(path); }
	void Write(const std::string& data) const {
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(data.data(), static_cast<std::streamsize>(data.size()));
	}
};

std::string OriginalBytes()
{
	std::ifstream file(Fixture("test_read_format_rar.rar"), std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(file), {});
}

// Repair the header checksum after mutating metadata, so a size guard must
// reject the record rather than the decoder stopping at a bad header CRC.
void HeaderChecksum(std::string& bytes)
{
	uint32_t crc = 0xffffffff;
	for (size_t i = 22; i < 70; ++i) {
		crc ^= static_cast<unsigned char>(bytes[i]);
		for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
	crc = ~crc;
	bytes[20] = static_cast<char>(crc);
	bytes[21] = static_cast<char>(crc >> 8);
}
}

TEST(RarArchive, LimitsAndRejectedMembersClearTheOutput)
{
	const auto archive = Fixture("test_read_format_rar.rar");
	std::string data = "old";
	CHECK(!RarArchive::Read(archive, "", "test.txt", 19, data));
	CHECK(data.empty());
	CHECK(RarArchive::Read(archive, "", "test.txt", 20, data));
	CHECK(data == "test text document\r\n");
	for (const auto* member : {"testlink", "testdir", "missing", "../test.txt", "/test.txt", "C:\\test.txt"}) {
		data = "old";
		CHECK(!RarArchive::Read(archive, "", member, 100, data));
		CHECK(data.empty());
	}
	CHECK(!RarArchive::Read(archive, "", std::string("test.txt\0other", 14), 100, data));
}

TEST(RarArchive, TruncatedAndCorruptedMembersNeverPublishPartialBytes)
{
	const auto original = OriginalBytes();
	CHECK_EQ(336, original.size());
	if (original.size() != 336) return;
	ChangedArchive changed;
	std::string data;
	// First member header is [20,70); its stored data is [70,90).
	for (size_t size = 0; size < 90; ++size) {
		changed.Write(original.substr(0, size));
		data = "old";
		CHECK(!RarArchive::Read(changed.path, "", "test.txt", 100, data));
		CHECK(data.empty());
	}
	auto corrupted = original;
	corrupted[75] ^= 0x40;
	changed.Write(corrupted);
	CHECK(!RarArchive::Read(changed.path, "", "test.txt", 100, data));
	CHECK(data.empty());
	// A valid first entry followed by a broken second header must not publish
	// the partial enumeration as if the archive had ended successfully.
	corrupted = original;
	corrupted[92] = 0;
	changed.Write(corrupted);
	std::vector<std::string> names{"old"};
	CHECK(!RarArchive::List(changed.path, "", "", names));
	CHECK(names.empty());
}

TEST(RarArchive, OversizedOrMismatchedMetadataIsRejectedBeforePublication)
{
	const auto original = OriginalBytes();
	if (original.size() != 336) { CHECK(false); return; }
	ChangedArchive changed;
	std::string data;
	for (uint32_t size : {0U, 19U, 21U, 0xffffffffU}) {
		auto modified = original;
		for (unsigned byte = 0; byte < 4; ++byte) modified[31 + byte] = static_cast<char>(size >> (8 * byte));
		HeaderChecksum(modified);
		changed.Write(modified);
		CHECK(!RarArchive::Read(changed.path, "", "test.txt", RarArchive::MaxMemberBytes, data));
		CHECK(data.empty());
	}
}

TEST(RarArchive, SolidEncryptedArchivesReachLaterMembers)
{
	for (const auto* fixture : {"test_read_format_rar4_solid_encrypted.rar",
		"test_read_format_rar5_solid_encrypted_filenames.rar"}) {
		std::string data;
		const auto archive = Fixture(fixture);
		CHECK(RarArchive::Read(archive, "password", "d.txt", 18, data));
		CHECK(data == std::string("This is from d.txt", 18));
		std::vector<std::string> names;
		CHECK(RarArchive::List(archive, "password", "?.TXT", names));
		CHECK_EQ(4, names.size());
		CHECK(!RarArchive::Read(archive, "wrong", "d.txt", 18, data));
		CHECK(data.empty());
	}
}

TEST(RarArchive, BestCompressedMembersRetainTheirCompleteContents)
{
	const auto archive = Fixture("test_read_format_rar_compress_best.rar");
	std::string first, second;
	CHECK(RarArchive::Read(archive, "", "LibarchiveAddingTest.html", 20111, first));
	CHECK_EQ(20111, first.size());
	CHECK(first.ends_with("<P STYLE=\"margin-bottom: 0in\"><BR>\n</P>\n</BODY>\n</HTML>"));
	CHECK(RarArchive::Read(archive, "", "testdir/LibarchiveAddingTest.html", 20111, second));
	CHECK(first == second);
	CHECK(RarArchive::Read(archive, "", "testdir/test.txt", 20, second));
	CHECK(second == "test text document\r\n");
}

TEST(RarArchive, UnicodeNamesRoundTripWithoutDependingOnTheProcessLocale)
{
	const auto archive = Fixture("test_read_format_rar_unicode.rar");
	std::vector<std::string> names;
	CHECK(RarArchive::List(archive, "", "*.txt", names));
	CHECK_EQ(3, names.size());
	bool foundKanji = false;
	for (const auto& name : names) {
		CHECK(TextSystem::IsValidUtf8(name.data(), name.size()));
		std::string data;
		CHECK(RarArchive::Read(archive, "", name, 100, data));
		if (data == "kanji") foundKanji = true;
		else if (name.find("abcdefghijklmnopqrs") == 0) CHECK_EQ(16, data.size());
		else CHECK(data.empty());
	}
	CHECK(foundKanji);
	// UTF-8 archive paths also round-trip when Linux starts in the C locale.
	const std::string utf8Path = "archive_\xED\x95\x9C.rar";
	const auto path = std::filesystem::path(u8"archive_\uD55C.rar");
	std::filesystem::copy_file(archive, path, std::filesystem::copy_options::overwrite_existing);
	CHECK(RarArchive::List(utf8Path, "", "*.TXT", names));
	CHECK_EQ(3, names.size());
	std::filesystem::remove(path);
}
