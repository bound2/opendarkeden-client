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
