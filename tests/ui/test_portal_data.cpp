#include "test_framework.h"
#include "Platform.h"
#include "SlayerPortalData.h"
#include "RarFile.h"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

namespace {
struct PortalFile {
	static constexpr const char* path = "portal_data_test.bin";
	explicit PortalFile(const std::string& bytes) {
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}
	~PortalFile() { std::remove(path); }
};
void AppendInt(std::string& bytes, uint32_t value)
{
	for (int shift = 0; shift < 32; shift += 8)
		bytes.push_back(static_cast<char>((value >> shift) & 0xff));
}
std::string PortalDocument()
{
	std::string bytes;
	AppendInt(bytes, 6);
	for (uint32_t map = 0; map < 6; ++map) {
		AppendInt(bytes, 2);
		for (uint32_t row = 0; row < 2; ++row) {
			AppendInt(bytes, 21 + map);
			AppendInt(bytes, 292 + row);
			AppendInt(bytes, 27 + map);
			AppendInt(bytes, 0);
			AppendInt(bytes, 255);
		}
	}
	return bytes;
}
}

TEST(PortalData, ProductionReaderKeepsMapOrderCoordinatesAndRepeatedZones)
{
	PortalFile file(PortalDocument());
	CRarFile reader;
	CHECK(reader.Open(PortalFile::path));
	SlayerPortalData flags;
	CHECK(ReadSlayerPortalData(reader, flags));
	for (size_t map = 0; map < flags.size(); ++map) {
		CHECK_EQ(2, flags[map].size());
		if (flags[map].size() != 2) continue;
		for (size_t row = 0; row < flags[map].size(); ++row) {
			const auto& flag = flags[map][row];
			CHECK_EQ(21 + map, flag.zone_id);
			CHECK_EQ(292 + row, flag.x);
			CHECK_EQ(27 + map, flag.y);
			CHECK_EQ(0, flag.portal_x);
			CHECK_EQ(255, flag.portal_y);
		}
	}
	CHECK(reader.IsEOF());
}

TEST(PortalData, TruncatedLastCoordinateDoesNotPublishPartialMaps)
{
	std::string bytes = PortalDocument();
	bytes.pop_back();
	PortalFile file(bytes);
	CRarFile reader;
	CHECK(reader.Open(PortalFile::path));
	SlayerPortalData flags;
	flags[0].push_back({99, 1, 2, 3, 4});
	CHECK(!ReadSlayerPortalData(reader, flags));
	CHECK_EQ(1, flags[0].size());
	CHECK_EQ(99, flags[0][0].zone_id);
}

namespace {
void WriteIntAt(std::string& bytes, size_t offset, uint32_t value)
{
	std::string field;
	AppendInt(field, value);
	bytes.replace(offset, field.size(), field);
}
void CheckRejectedPortal(const std::string& bytes)
{
	PortalFile file(bytes);
	CRarFile reader;
	CHECK(reader.Open(PortalFile::path));
	const char* start = reader.GetFilePointer();
	SlayerPortalData flags;
	flags[0].push_back({99, 1, 2, 3, 4});
	flags[5].push_back({98, 5, 6, 7, 8});
	CHECK(!ReadSlayerPortalData(reader, flags));
	CHECK(reader.GetFilePointer() == start);
	CHECK_EQ(bytes.size(), reader.GetRemainingSize());
	CHECK_EQ(1, flags[0].size());
	CHECK_EQ(1, flags[5].size());
	if (!flags[0].empty()) CHECK_EQ(99, flags[0][0].zone_id);
	if (!flags[5].empty()) CHECK_EQ(98, flags[5][0].zone_id);
	for (size_t map = 1; map < 5; ++map) CHECK(flags[map].empty());
}
}

TEST(PortalData, MissingFilesAndEveryTruncatedPrefixPreserveDataAndCursor)
{
	CRarFile missing;
	SlayerPortalData flags;
	flags[0].push_back({99, 1, 2, 3, 4});
	CHECK(!ReadSlayerPortalData(missing, flags));
	CHECK_EQ(1, flags[0].size());
	CHECK_EQ(99, flags[0][0].zone_id);
	CHECK(missing.GetFilePointer() == nullptr);
	const auto bytes = PortalDocument();
	for (size_t size = 0; size < bytes.size(); ++size)
		CheckRejectedPortal(bytes.substr(0, size));
}

TEST(PortalData, InvalidCountsAndTrailingBytesRejectTheWholeFile)
{
	for (uint32_t count : {0u, 1u, 5u, 7u, 0x7fffffffu, 0xffffffffu}) {
		auto bytes = PortalDocument();
		WriteIntAt(bytes, 0, count);
		CheckRejectedPortal(bytes);
	}
	for (size_t map = 0; map < 6; ++map) {
		for (uint32_t count : {0u, 1u, 3u, 65537u, 0x7fffffffu, 0xffffffffu}) {
			auto bytes = PortalDocument();
			WriteIntAt(bytes, 4 + map * 44, count);
			CheckRejectedPortal(bytes);
		}
	}
	CheckRejectedPortal(PortalDocument() + std::string(1, '\0'));
	CheckRejectedPortal(PortalDocument() + PortalDocument());
}

TEST(PortalData, NegativeFieldsAndUnrepresentableDestinationsAreRejected)
{
	for (size_t field = 0; field < 5; ++field) {
		for (uint32_t value : {0x80000000u, 0xffffffffu}) {
			auto bytes = PortalDocument();
			WriteIntAt(bytes, 8 + field * 4, value);
			CheckRejectedPortal(bytes);
		}
	}
	for (uint32_t zone : {65536u, 0x7fffffffu}) {
		auto bytes = PortalDocument();
		WriteIntAt(bytes, 8, zone);
		CheckRejectedPortal(bytes);
	}
	for (size_t field : {3u, 4u}) {
		for (uint32_t value : {256u, 0x7fffffffu}) {
			auto bytes = PortalDocument();
			WriteIntAt(bytes, 8 + field * 4, value);
			CheckRejectedPortal(bytes);
		}
	}
}

TEST(PortalData, SuccessfulReloadReplacesMapsAndReadsUnalignedLittleEndianBytes)
{
	auto bytes = PortalDocument();
	WriteIntAt(bytes, 8, 65535);
	WriteIntAt(bytes, 12, 0x7fffffff);
	WriteIntAt(bytes, 16, 0);
	// The format reader must not assume an aligned native-int pointer.
	PortalFile file("x" + bytes);
	CRarFile reader;
	CHECK(reader.Open(PortalFile::path));
	CHECK(reader.Read(1) != nullptr);
	SlayerPortalData flags;
	for (auto& map : flags) map.push_back({99, 1, 2, 3, 4});
	CHECK(ReadSlayerPortalData(reader, flags));
	for (const auto& map : flags) CHECK_EQ(2, map.size());
	if (flags[0].size() == 2) {
		CHECK_EQ(65535, flags[0][0].zone_id);
		CHECK_EQ(0x7fffffff, flags[0][0].x);
		CHECK_EQ(0, flags[0][0].y);
	}
	CHECK(reader.IsEOF());
	CHECK(!ReadSlayerPortalData(reader, flags));
	CHECK_EQ(2, flags[0].size());
	CHECK(reader.Open(PortalFile::path));
	CHECK(reader.Read(1) != nullptr);
	CHECK(ReadSlayerPortalData(reader, flags));
	CHECK_EQ(2, flags[0].size());
}

TEST(PortalData, CountLimitIsCheckedBeforeAllocationEvenWhenBytesExist)
{
	for (uint32_t count : {65536u, 65537u}) {
		std::string bytes;
		AppendInt(bytes, 6);
		for (uint32_t map = 0; map < 6; ++map) {
			const uint32_t rows = map == 0 ? count : 1;
			AppendInt(bytes, rows);
			for (uint32_t row = 0; row < rows; ++row) {
				AppendInt(bytes, 21);
				AppendInt(bytes, 292);
				AppendInt(bytes, 27);
				AppendInt(bytes, 227);
				AppendInt(bytes, 59);
			}
		}
		if (count > 65536) {
			CheckRejectedPortal(bytes);
			continue;
		}
		PortalFile file(bytes);
		CRarFile reader;
		CHECK(reader.Open(PortalFile::path));
		SlayerPortalData flags;
		CHECK(ReadSlayerPortalData(reader, flags));
		CHECK_EQ(count, flags[0].size());
		CHECK_EQ(1, flags[5].size());
	}
	CheckRejectedPortal(std::string(4 + 6 * (4 + 20 * 65536) + 1, '\0'));
}
