#include "Platform.h"
#include "SlayerPortalData.h"
#include "RarFile.h"
#include <cstdint>
#include <limits>
#include <string_view>

namespace {
constexpr size_t MaxFlagsPerMap = 65536;
constexpr size_t FlagBytes = 5 * 4;
constexpr size_t MapCount = std::tuple_size_v<SlayerPortalData>;
constexpr size_t MaxFileBytes = 4 + MapCount * (4 + MaxFlagsPerMap * FlagBytes);

bool ReadInt(std::string_view& bytes, int& value)
{
	if (bytes.size() < 4) return false;
	uint32_t raw = 0;
	for (unsigned int i = 0; i < 4; ++i)
		raw |= static_cast<uint32_t>(static_cast<unsigned char>(bytes[i])) << (8 * i);
	if (raw > static_cast<uint32_t>((std::numeric_limits<int>::max)())) return false;
	value = static_cast<int>(raw);
	bytes.remove_prefix(4);
	return true;
}
}

bool ReadSlayerPortalData(CRarFile& file, SlayerPortalData& flags)
{
	const size_t size = file.GetRemainingSize();
	if (!file.GetFilePointer() || size < 4 + MapCount * 4 || size > MaxFileBytes)
		return false;
	std::string_view bytes(file.GetFilePointer(), size);
	int mapCount;
	if (!ReadInt(bytes, mapCount) || mapCount != static_cast<int>(MapCount)) return false;
	SlayerPortalData parsed;
	for (auto& map : parsed)
	{
		int count;
		if (!ReadInt(bytes, count) || count == 0 ||
			static_cast<size_t>(count) > MaxFlagsPerMap ||
			static_cast<size_t>(count) > bytes.size() / FlagBytes) return false;
		map.reserve(static_cast<size_t>(count));
		for (int i = 0; i < count; ++i)
		{
			UI_PORTAL_FLAG flag;
			if (!ReadInt(bytes, flag.zone_id) || flag.zone_id > 65535 ||
				!ReadInt(bytes, flag.x) || !ReadInt(bytes, flag.y) ||
				!ReadInt(bytes, flag.portal_x) || flag.portal_x > 255 ||
				!ReadInt(bytes, flag.portal_y) || flag.portal_y > 255) return false;
			map.push_back(flag);
		}
	}
	if (!bytes.empty()) return false;
	// Commit both results only after the complete file has validated.
	if (!file.Read(static_cast<int>(size))) return false;
	flags.swap(parsed);
	return true;
}
