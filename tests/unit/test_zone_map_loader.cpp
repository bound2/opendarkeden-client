#include "test_framework.h"
#include "ZoneMapData.h"
#include "MInteractionObject.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<char>;
constexpr const char* path = "zone_map_loader_test.bin";
struct Fixture { ~Fixture() { std::remove(path); } };

void Number(Bytes& bytes, std::uint32_t value, unsigned width)
{
	for (unsigned i = 0; i < width; ++i) bytes.push_back(char(value >> (8 * i)));
}
void String(Bytes& bytes, const char* value)
{
	Number(bytes, std::uint32_t(std::strlen(value)), 4);
	bytes.insert(bytes.end(), value, value + std::strlen(value));
}
Bytes Header(unsigned width = 2, unsigned height = 1, bool wordLayout = false)
{
	Bytes bytes;
	String(bytes, MAP_VERSION_2000_05_10);
	Number(bytes, 42, 2);
	Number(bytes, 7, 2);
	String(bytes, "zone");
	Number(bytes, 1, wordLayout ? 2 : 1);
	Number(bytes, 2, wordLayout ? 2 : 1);
	String(bytes, "description");
	Number(bytes, 123, 4);
	Number(bytes, 456, 4);
	Number(bytes, width, 2);
	Number(bytes, height, 2);
	return bytes;
}
void Sector(Bytes& bytes, unsigned sprite, unsigned flag = 8, unsigned light = 3)
{
	Number(bytes, sprite, 2); Number(bytes, flag, 1); Number(bytes, light, 1);
}
void Object(Bytes& bytes, unsigned type, unsigned id, unsigned x = 1, unsigned y = 0)
{
	Number(bytes, type, 1); // allocation tag
	Number(bytes, type, 1); // MObject's stored tag
	Number(bytes, id, 4);
	Number(bytes, 0xffff, 2); Number(bytes, 0, 2); // wall direction/value
	Number(bytes, 77, 4); Number(bytes, 9, 2); // image and sprite IDs
	Number(bytes, 120, 4); Number(bytes, 240, 4);
	Number(bytes, 1, 2); // viewpoint
	Number(bytes, type >= MObject::TYPE_ANIMATIONOBJECT, 1);
	Number(bytes, 3, 1); // transparency flags
	if (type >= MObject::TYPE_ANIMATIONOBJECT) {
		Number(bytes, 17, 2); Number(bytes, 4, 1); // frame/max frame
		Number(bytes, 0, 1); Number(bytes, 2, 1); Number(bytes, 1, 1);
		Number(bytes, 23, 2); // sound
		Number(bytes, 1, 1); Number(bytes, 300, 4); Number(bytes, 500, 4);
		Number(bytes, 0, 1); Number(bytes, 24, 1);
	}
	if (type == MObject::TYPE_INTERACTIONOBJECT) Number(bytes, 0x1234, 2);
	Number(bytes, 1, 2); Number(bytes, x, 2); Number(bytes, y, 2);
}
Bytes Map(bool wordLayout = false)
{
	auto bytes = Header(2, 1, wordLayout);
	Sector(bytes, 10); Sector(bytes, 11, 4, 0);
	Number(bytes, 5, 4);
	for (unsigned type = 3; type <= 7; ++type) Object(bytes, type, 100 + type);
	return bytes;
}
bool Load(ZoneMapData& map, const Bytes& bytes, std::size_t size = SIZE_MAX, bool skip = false)
{
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out.write(bytes.data(), std::streamsize((std::min)(size, bytes.size())));
	}
	std::ifstream in(path, std::ios::binary);
	const bool ok = map.LoadFromFile(in, skip);
	CHECK(ok == bool(in));
	return ok;
}
}

TEST(ZoneMapLoader, ReadsEveryObjectKindAndBothHeaderLayouts)
{
	Fixture fixture;
	for (bool wordLayout : {false, true}) {
		ZoneMapData map;
		CHECK(Load(map, Map(wordLayout)));
		CHECK_EQ(2, map.width); CHECK_EQ(1, map.height);
		CHECK_EQ(42, map.info.ZoneID); CHECK_EQ(7, map.info.ZoneGroupID);
		CHECK(map.info.ZoneName == "zone"); CHECK(map.info.Description == "description");
		CHECK_EQ(1, map.info.ZoneType); CHECK_EQ(2, map.info.ZoneLevel);
		CHECK_EQ(123, map.tileOffset); CHECK_EQ(456, map.imageOffset);
		CHECK_EQ(2, map.sectors.size()); CHECK_EQ(5, map.objects.size());
		if (map.sectors.size() != 2 || map.objects.size() != 5) continue;
		CHECK_EQ(10, map.sectors[0].sprite); CHECK_EQ(8, map.sectors[0].property);
		CHECK_EQ(3, map.sectors[0].light); CHECK_EQ(11, map.sectors[1].sprite);
		for (std::size_t i = 0; i < map.objects.size(); ++i) {
			const auto& entry = map.objects[i];
			CHECK_EQ(i + 3, entry.object->GetObjectType());
			CHECK_EQ(i + 103, entry.object->GetID());
			CHECK_EQ(77, entry.object->GetImageObjectID());
			CHECK_EQ(9, entry.object->GetSpriteID());
			CHECK_EQ(120, entry.object->GetPixelX());
			CHECK_EQ(240, entry.object->GetPixelY());
			CHECK_EQ(1, entry.positions.GetSize());
		}
		auto* interaction = static_cast<MInteractionObject*>(map.objects.back().object.get());
		CHECK_EQ(0x1234, interaction->GetInteractionObjectType());
	}
}

TEST(ZoneMapLoader, EveryTruncationPreservesThePreviousMap)
{
	Fixture fixture;
	const auto bytes = Map();
	ZoneMapData map;
	CHECK(Load(map, bytes));
	for (std::size_t size = 0; size < bytes.size(); ++size) {
		CHECK(!Load(map, bytes, size));
		CHECK_EQ(2, map.width); CHECK_EQ(5, map.objects.size());
	}
}

TEST(ZoneMapLoader, RefusesImpossibleDimensionsBeforeAllocatingSectors)
{
	Fixture fixture;
	for (auto dimensions : {std::pair{0u, 1u}, {1u, 0u}, {65535u, 65535u},
		{1025u, 1u}, {1024u, 1024u}}) {
		ZoneMapData map;
		CHECK(!Load(map, Header(dimensions.first, dimensions.second)));
		CHECK(map.sectors.empty()); CHECK(map.objects.empty());
	}
}

TEST(ZoneMapLoader, RefusesNegativeOversizedAndTruncatedObjectCounts)
{
	Fixture fixture;
	for (unsigned count : {0xffffffffu, ZoneMapData::MaxObjects + 1, 1u}) {
		auto bytes = Header(); Sector(bytes, 1); Sector(bytes, 2); Number(bytes, count, 4);
		ZoneMapData map;
		CHECK(!Load(map, bytes)); CHECK(map.sectors.empty()); CHECK(map.objects.empty());
	}
}

TEST(ZoneMapLoader, RefusesUnknownTypesDuplicateIdsAndOutsidePositions)
{
	Fixture fixture;
	for (unsigned mode = 0; mode < 5; ++mode) {
		auto bytes = Header(); Sector(bytes, 1); Sector(bytes, 2);
		Number(bytes, mode == 1 ? 2 : 1, 4);
		Object(bytes, mode == 0 ? 99 : 3, 123, mode == 2 ? 2 : 1, mode == 3 ? 1 : 0);
		if (mode == 1) Object(bytes, 3, 123);
		if (mode == 4) bytes[Header().size() + 8 + 4 + 1] = 4; // mismatched inner tag
		ZoneMapData map;
		CHECK(!Load(map, bytes)); CHECK(map.objects.empty());
	}
}

TEST(ZoneMapLoader, DimensionAndSectorBudgetsAreInclusive)
{
	Fixture fixture;
	for (auto dimensions : {std::pair{1024u, 1u}, {512u, 512u}}) {
		auto bytes = Header(dimensions.first, dimensions.second);
		const auto cells = dimensions.first * dimensions.second;
		for (unsigned i = 0; i < cells; ++i) Sector(bytes, i & 65535);
		Number(bytes, 0, 4);
		ZoneMapData map;
		CHECK(Load(map, bytes)); CHECK_EQ(cells, map.sectors.size());
	}
}

TEST(ZoneMapLoader, DemoCanSkipObjectsAfterAValidSectorTableAndBoundedCount)
{
	Fixture fixture;
	auto bytes = Header(); Sector(bytes, 1); Sector(bytes, 2); Number(bytes, 5, 4);
	ZoneMapData map;
	CHECK(Load(map, bytes, SIZE_MAX, true));
	CHECK_EQ(2, map.sectors.size()); CHECK(map.objects.empty());
	bytes.back() = char(0xff);
	CHECK(!Load(map, bytes, SIZE_MAX, true));
}

TEST(ZoneMapLoader, RefusesInvalidBooleanBytesAndAnimationOnAStaticObject)
{
	Fixture fixture;
	for (unsigned mode = 0; mode < 3; ++mode) {
		auto bytes = Header(); Sector(bytes, 1); Sector(bytes, 2); Number(bytes, 1, 4);
		const auto start = bytes.size();
		Object(bytes, mode == 2 ? 5 : 3, 1);
		bytes[start + (mode == 2 ? 36 : 26)] = mode == 0 ? 1 : 2;
		ZoneMapData map;
		CHECK(!Load(map, bytes)); CHECK(map.objects.empty());
	}
}

TEST(ZoneMapLoader, TotalPositionBudgetIsInclusiveAcrossAllObjects)
{
	Fixture fixture;
	auto bytes = Header(512, 512);
	for (unsigned i = 0; i < 512 * 512; ++i) Sector(bytes, 1);
	Number(bytes, 17, 4);
	for (unsigned id = 0; id < 17; ++id) {
		Object(bytes, 3, id);
		bytes.resize(bytes.size() - 6); // replace the one-position tail
		const unsigned count = id < 16 ? 65535 : 16;
		Number(bytes, count, 2);
		for (unsigned i = 0; i < count; ++i) {
			Number(bytes, i % 512, 2); Number(bytes, i / 512, 2);
		}
	}
	ZoneMapData map;
	CHECK(Load(map, bytes)); CHECK_EQ(17, map.objects.size());
	bytes[bytes.size() - 66] = 17; // last count grows from 16 to 17
	Number(bytes, 16, 2); Number(bytes, 0, 2);
	CHECK(!Load(map, bytes)); CHECK_EQ(17, map.objects.size());
}

TEST(ZoneMapLoader, FileByteBudgetIsInclusiveAndCheckedBeforeParsing)
{
	Fixture fixture;
	const auto bytes = Map();
	for (unsigned excess : {0u, 1u}) {
		{
			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			out.write(bytes.data(), bytes.size());
			out.seekp(ZoneMapData::MaxFileBytes - 1 + excess);
			out.put('\0');
		}
		std::ifstream in(path, std::ios::binary);
		ZoneMapData map;
		CHECK(map.LoadFromFile(in) == (excess == 0));
		if (excess) { CHECK(in.fail()); CHECK(map.objects.empty()); }
	}
}
