#include "test_framework.h"
#include "Platform.h"
#include "MZoneTable.h"
#include "MCreatureTable.h"
#include "MNPCTable.h"
#include "MGuildInfoMapper.h"
#include "MGameTime.h"
#include <string>

// Construct the real classes and call their out-of-line implementations.
// These tests require all five production objects to link from gamemodel;
// they provide no substitute definitions for executable symbols.
TEST(WorldMetadata, ZoneTableOwnsRowsAndRetainsTheFirstDuplicate)
{
	CZoneTable zones;
	CHECK(zones.Get(7) == nullptr);
	CHECK(!zones.Add(nullptr));
	auto* row = new ZONETABLE_INFO;
	row->ID = 7;
	row->Name = "Test zone";
	CHECK(zones.Add(row));
	CHECK(zones.Get(7) == row);
	CHECK(std::string(zones.Get(7)->Name.GetString()) == "Test zone");
	auto* duplicate = new ZONETABLE_INFO;
	duplicate->ID = 7;
	CHECK(!zones.Add(duplicate)); // Add consumes the rejected duplicate.
	CHECK(zones.Get(7) == row);
	zones.Release();
	CHECK(zones.GetZoneMap().empty());
}

TEST(WorldMetadata, CreatureRecordsOwnActionArraysAndSpriteMappings)
{
	CREATURE_TABLE creatures;
	creatures.Init(2);
	auto* row = creatures.GetMutable(1);
	CHECK(row != nullptr);
	if (!row) return;
	row->SetCreatureTribe(CREATURETRIBE_NPC, 3);
	row->SetActionSound(1, 4, 23);
	CHECK(row->IsNPC());
	CHECK_EQ(4, row->GetActionCount(1));
	CHECK_EQ(23, row->GetActionSound(1));
	CHECK_EQ(SOUNDID_NULL, row->GetActionSound(3));
	row->SetCreatureTribe(CREATURETRIBE_SLAYER, 2);
	CHECK(row->IsSlayer());
	CHECK_EQ(0, row->GetActionCount(1));
	CHECK_EQ(SOUNDID_NULL, row->GetActionSound(1));
	CreatureSpriteTypeMapper sprites;
	sprites.Init(2);
	sprites.AddCreatureType(1, 42);
	CHECK_EQ(42, sprites.GetRandomCreatureType(1));
	sprites.Release(); // This legacy mapper requires explicit cleanup.
}

TEST(WorldMetadata, ServerNpcRowsUpdateBothLibraryOwnedTables)
{
	CREATURE_TABLE creatures;
	creatures.Init(158);
	struct RestoreCreatureTable {
		CREATURE_TABLE* previous = g_pCreatureTable;
		~RestoreCreatureTable() { g_pCreatureTable = previous; }
	} restore;
	g_pCreatureTable = &creatures;
	MNPCTable npcs;
	MServerNPCTable server;
	auto* row = new SERVERNPC_INFO;
	row->Name = "Merchant";
	row->Description = "Supplies";
	row->ListShopTemplateID.push_back(3);
	CHECK(server.AddData(157, row));
	CHECK(server.AffectToNPCTable(&npcs));
	const auto* copied = npcs.GetData(157);
	CHECK(copied != nullptr);
	if (!copied) return;
	CHECK(std::string(copied->Name.GetString()) == "Merchant");
	CHECK(std::string(copied->Description.GetString()) == "Supplies");
	CHECK_EQ(1, copied->ListShopTemplateID.size());
	CHECK_EQ(3, copied->ListShopTemplateID.front());
	CHECK_EQ(1, copied->SpriteID);
	CHECK(std::string(creatures[157].Name.GetString()) == "Merchant");
	CHECK(npcs.RemoveData(157));
	CHECK(npcs.GetData(157) == nullptr);
}

TEST(WorldMetadata, GuildMapperOwnsAndReplacesItsSpriteRecords)
{
	MGuildInfoMapper guilds;
	CHECK(guilds.Get(5) == nullptr);
	auto* first = new GUILD_INFO;
	first->SetSpriteID(12);
	guilds.Set(5, first);
	CHECK_EQ(12, guilds.Get(5)->GetSpriteID());
	auto* replacement = new GUILD_INFO;
	replacement->SetSpriteID(34);
	guilds.Set(5, replacement);
	CHECK_EQ(34, guilds.Get(5)->GetSpriteID());
	std::string name = "Guild";
	guilds.SetGuildName(5, name);
	CHECK(std::string(guilds.GetGuildName(5)) == "Guild");
	guilds.Release();
	CHECK(guilds.Get(5) == nullptr);
}

TEST(WorldMetadata, CalendarUsesTheSuppliedClockAndTimeRatio)
{
	MGameTime calendar;
	const MonotonicClock::TimePoint start(MonotonicClock::Duration(4294967000LL));
	calendar.SetStartTime(start, 2026, 9, 24, 12, 58, 50);
	calendar.SetTimeRatio(2);
	calendar.SetCurrentTime(start + MonotonicClock::Duration(61000));
	CHECK_EQ(2026, calendar.GetYear());
	CHECK_EQ(9, calendar.GetMonth());
	CHECK_EQ(24, calendar.GetDay());
	CHECK_EQ(13, calendar.GetHour());
	CHECK_EQ(0, calendar.GetMinute());
	CHECK_EQ(52, calendar.GetSecond());
}
