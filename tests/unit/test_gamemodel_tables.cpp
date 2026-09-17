//----------------------------------------------------------------------
// test_gamemodel_tables.cpp
//----------------------------------------------------------------------
//
// The first tests against the gamemodel library (docs/RESTRUCTURING.md
// task 4.1): the pure data tables - experience, item options, sounds,
// fame, system availability - and the string table's language switch.
// Each table loads from a file the game data ships; the tests write the
// same byte layout to a scratch file and read it back through the real
// loader, the TArray pattern in test_tarray.cpp.
//
// Two contracts are the point, not the happy path: a count the file
// declares must never index past the table it sizes, and a lookup past
// the loaded range must yield the default entry rather than memory
// beyond the array (CTypeTable's own rule, pinned here on the tables
// the game reads through).
//
// Compiled with the packetwire defines (tests/CMakeLists.txt) so
// Properties and RaceType are seen as the library sees them.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "ExperienceTable.h"
#include "MItemOptionTable.h"
#include "MSoundTable.h"
#include "MStringArray.h"
#include "SkillDef.h"
#include "FameInfo.h"
#include "SystemAvailabilities.h"
#include "MGameStringTable.h"
#include "RankBonusTable.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

const char* const	kTempFile = "gamemodel_tables_test.bin";

//----------------------------------------------------------------------
// A little-endian byte builder for the binary table formats, then a
// scratch file written from it and opened the way the loaders expect.
//----------------------------------------------------------------------
struct Bytes
{
	std::vector<unsigned char>	data;

	Bytes&	Int(int v)			{ return Raw(&v, sizeof(v)); }
	Bytes&	UInt(unsigned int v)		{ return Raw(&v, sizeof(v)); }
	Bytes&	Byte(unsigned char v)		{ data.push_back(v); return *this; }
	Bytes&	Word(unsigned short v)		{ return Raw(&v, sizeof(v)); }
	Bytes&	Raw(const void* p, size_t n)
	{
		const unsigned char* c = static_cast<const unsigned char*>(p);
		data.insert(data.end(), c, c + n);
		return *this;
	}
	// MString's on-disk form: a 4-byte length, then the characters.
	Bytes&	Str(const char* s)
	{
		Int((int)std::strlen(s));
		return Raw(s, std::strlen(s));
	}
};

void	WriteScratch(const Bytes& b)
{
	std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
	if (!b.data.empty())
		out.write((const char*)&b.data[0], (std::streamsize)b.data.size());
}

void	WriteScratchText(const char* text)
{
	std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
	out << text;
}

void	RemoveScratch()
{
	std::remove(kTempFile);
}

// MString::GetString() is NULL for an entry that was never set, so a
// bare strcmp would crash the run instead of failing the check.
bool	StrEq(const char* actual, const char* expected)
{
	return actual != NULL && std::strcmp(actual, expected) == 0;
}

} // namespace

//----------------------------------------------------------------------
// ExperienceTable: [count:4] then count x [level:4][goal:4][accum:4],
// placed by level (the file starts at level 1, so the table holds
// count + 1 slots).
//----------------------------------------------------------------------
TEST(ExperienceTable, LoadPlacesEntriesByLevelAndOutOfRangeReadsTheDefault)
{
	Bytes b;
	b.Int(2);
	b.Int(1).Int(100).UInt(100);
	b.Int(2).Int(250).UInt(350);
	WriteScratch(b);

	ExperienceTable table;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFileSTR(in);
	}
	RemoveScratch();

	CHECK_EQ(3, table.m_STRExp.GetSize());
	CHECK_EQ(100, table.GetSTRInfo(1).GoalExp);
	CHECK_EQ(100, table.GetSTRInfo(1).AccumExp);
	CHECK_EQ(250, table.GetSTRInfo(2).GoalExp);
	CHECK_EQ(350, table.GetSTRInfo(2).AccumExp);
	// Past the table: the default entry, never memory beyond it.
	CHECK_EQ(0, table.GetSTRInfo(99).GoalExp);
	CHECK_EQ(0, table.GetSTRInfo(-1).GoalExp);
}

//----------------------------------------------------------------------
// ITEMOPTION_TABLE: [parts:4] then parts x [ename][name] MStrings, then
// the CTypeTable body [count:4] and count x ITEMOPTION_INFO. The part
// names land in two fixed arrays of MAX_PART entries, so the count the
// file declares for them is a bound the loader must enforce.
//----------------------------------------------------------------------
namespace {

void	AppendOptionInfo(Bytes& b, const char* ename, const char* name, int part, int plus)
{
	b.Str(ename).Str(name);
	b.Int(part).Int(plus).Int(150);		// part, plus point, price multiplier
	b.Int(1).Int(2).Int(3).Int(6).Int(10);	// STR, DEX, INT, SUM, level
	b.Int(7).Int(0).Int(0);			// colour set, upgrade, previous
}

} // namespace

TEST(ItemOptionTable, LoadReadsPartNamesThenTheEntries)
{
	Bytes b;
	b.Int(2);
	b.Str("E-Strength").Str("Strength");
	b.Str("E-Dexterity").Str("Dexterity");
	b.Int(2);
	AppendOptionInfo(b, "E-Weak", "Weak", ITEMOPTION_TABLE::PART_STR, 1);
	AppendOptionInfo(b, "E-Strong", "Strong", ITEMOPTION_TABLE::PART_DEX, 5);
	WriteScratch(b);

	ITEMOPTION_TABLE table;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFile(in);
	}
	RemoveScratch();

	CHECK_EQ(2, table.GetSize());
	CHECK(StrEq(table.ITEMOPTION_PARTNAME[1].GetString(), "Dexterity"));
	CHECK(StrEq(table.ITEMOPTION_PARTENAME[0].GetString(), "E-Strength"));
	CHECK(StrEq(table[1].Name.GetString(), "Strong"));
	CHECK_EQ(ITEMOPTION_TABLE::PART_DEX, table[1].Part);
	CHECK_EQ(5, table[1].PlusPoint);
	CHECK_EQ(150, table[1].PriceMultiplier);
	CHECK_EQ(10, table[1].RequireLevel);
}

TEST(ItemOptionTable, RejectsMorePartNamesThanTheArraysHold)
{
	Bytes b;
	b.Int(ITEMOPTION_TABLE::MAX_PART + 1);
	for (int i = 0; i < ITEMOPTION_TABLE::MAX_PART + 1; i++)
		b.Str("e").Str("n");
	b.Int(1);
	AppendOptionInfo(b, "E-X", "X", ITEMOPTION_TABLE::PART_STR, 1);
	WriteScratch(b);

	ITEMOPTION_TABLE table;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFile(in);
	}
	RemoveScratch();

	// The oversized part list is refused as a whole: nothing is loaded
	// rather than the last names written past the arrays.
	CHECK_EQ(0, table.GetSize());
	CHECK(table.ITEMOPTION_PARTNAME[0].GetString() == NULL);
}

TEST(ItemOptionTable, RejectsANegativePartCount)
{
	Bytes b;
	b.Int(-1);
	WriteScratch(b);

	ITEMOPTION_TABLE table;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFile(in);
	}
	RemoveScratch();
	CHECK_EQ(0, table.GetSize());
}

//----------------------------------------------------------------------
// SOUND_TABLE is a plain CTypeTable of file names: a save/load round
// trip through the two MString-backed entries.
//----------------------------------------------------------------------
TEST(SoundTable, SaveAndLoadRoundTripTheFileNames)
{
	SOUND_TABLE src;
	src.Init(2);
	src[0].Filename = "hit.wav";
	src[1].Filename = "miss.wav";
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		src.SaveToFile(out);
	}

	SOUND_TABLE dst;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		dst.LoadFromFile(in);
	}
	RemoveScratch();

	CHECK_EQ(2, dst.GetSize());
	CHECK(StrEq(dst[0].Filename.GetString(), "hit.wav"));
	CHECK(StrEq(dst[1].Filename.GetString(), "miss.wav"));
	CHECK(dst[2].Filename.GetString() == NULL);	// default entry past the end
}

//----------------------------------------------------------------------
// MStringArray's nickname layout: [count:4] then count x [index:2][str].
// An index at or past the count is refused instead of written past the
// array (the CTypeTable check the game's nickname table depends on).
//----------------------------------------------------------------------
TEST(StringArray, NicknameLayoutRejectsAnIndexPastTheCount)
{
	Bytes b;
	b.Int(2);
	b.Word(1).Str("second");
	b.Word(2).Str("past the end");
	WriteScratch(b);

	MStringArray table;
	bool bOk;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		bOk = table.LoadFromFile_NickNameString(in);
	}
	RemoveScratch();

	CHECK(!bOk);
	CHECK_EQ(2, table.GetSize());
	CHECK(StrEq(table[1].GetString(), "second"));
}

//----------------------------------------------------------------------
// FameInfoTable: entries are [domain:1][level:1][fame:4], grouped by
// skill domain with an equal number of levels each; GetFameForLevel
// indexes domain * levels + level and answers 0 for a domain mismatch.
//----------------------------------------------------------------------
TEST(FameInfoTable, LooksUpFameByDomainAndLevel)
{
	const int levels = 2;
	Bytes b;
	b.Int(MAX_SKILLDOMAIN * levels);
	for (int d = 0; d < MAX_SKILLDOMAIN; d++)
		for (int l = 0; l < levels; l++)
			b.Byte((unsigned char)d).Byte((unsigned char)l).UInt((unsigned)(d * 100 + l));
	WriteScratch(b);

	FameInfoTable table;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFile(in);
	}
	RemoveScratch();

	CHECK_EQ(MAX_SKILLDOMAIN * levels, table.GetSize());
	CHECK_EQ(201, table.GetFameForLevel(SKILLDOMAIN_GUN, 1));
	CHECK_EQ(0, table.GetFameForLevel(SKILLDOMAIN_BLADE, 0));
	CHECK_EQ(600, table.GetFameForLevel(SKILLDOMAIN_VAMPIRE, 0));
	// A level past the domain's rows reads the next domain's entry,
	// whose domain byte does not match: 0, not a foreign value.
	CHECK_EQ(0, table.GetFameForLevel(SKILLDOMAIN_BLADE, levels));
	// Past the whole table: the default entry.
	CHECK_EQ(0, table.GetFameForLevel(SKILLDOMAIN_OUSTERS, 50));
}

//----------------------------------------------------------------------
// SystemAvailabilitiesManager parses the filter script from a stream:
// script filters per system kind, zone lists per open degree, and the
// per-degree script list.
//----------------------------------------------------------------------
namespace {

const char* const	kFilterScript =
	"; system 4 (guild) blocks script 10 answer 2 - the file counts answers from 1\r\n"
	"*4 1\n"
	"10 2\n"
	"Z0\n"
	"100\n"
	"200\n"
	"99999\n"
	"Z9\n"
	"300\n"
	"99999\n"
	"S0 1\n"
	"11 3 300\n";

bool	LoadScript(SystemAvailabilitiesManager& m, const char* text)
{
	std::istringstream in(text);
	return m.LoadFromStream(in);
}

} // namespace

TEST(SystemAvailabilities, ScriptFilterAppliesOnlyWhenItsSystemIsClosed)
{
	SystemAvailabilitiesManager m;
	CHECK(LoadScript(m, kFilterScript));

	// Every system open (the constructor's default): nothing filtered.
	CHECK(m.ScriptFiltering(10, 1));

	// Close system 4 (bit 4 clear): script 10 / answer 2 (stored as 1)
	// is refused, its neighbours are not.
	m.SetFlag(~(1u << 4));
	CHECK(!m.ScriptFiltering(10, 1));
	CHECK(m.ScriptFiltering(10, 0));
	CHECK(m.ScriptFiltering(12, 1));
}

TEST(SystemAvailabilities, ZoneListsGateByOpenDegree)
{
	SystemAvailabilitiesManager m;
	CHECK(LoadScript(m, kFilterScript));

	// The default open degree (0xFF) is past every list: everything opens.
	CHECK(m.ZoneFiltering(300));

	m.SetOpenDegree(0);
	CHECK(m.ZoneFiltering(100));
	CHECK(m.ZoneFiltering(200));
	CHECK(!m.ZoneFiltering(300));
	// The degree-9 list is out of range (MAX_OPEN_DEGREE is 9) and was
	// dropped by the loader, so zone 300 opens for no degree.
	m.SetOpenDegree(8);
	CHECK(!m.ZoneFiltering(300));
}

TEST(SystemAvailabilities, DegreeScriptsFollowTheirZone)
{
	SystemAvailabilitiesManager m;
	CHECK(LoadScript(m, kFilterScript));

	// Script 11 / answer 3 (stored as 2) belongs to zone 300, which no
	// degree opens once a degree is set.
	m.SetOpenDegree(0);
	CHECK(!m.ScriptFiltering(11, 2));
	CHECK(m.ScriptFiltering(11, 1));
}

TEST(SystemAvailabilities, AnUnreadableStreamIsRefused)
{
	SystemAvailabilitiesManager m;
	std::ifstream missing("gamemodel_tables_test_no_such_file.txt");
	CHECK(!m.LoadFromStream(missing));
}

//----------------------------------------------------------------------
// UseEnglishText: the language file selects the built-in English text
// unless it names another language; anything unreadable means English.
//----------------------------------------------------------------------
TEST(GameStringTable, LanguageFileSelectsEnglishUnlessItSaysOtherwise)
{
	CHECK(UseEnglishText(NULL));
	CHECK(UseEnglishTextFrom(NULL));
	CHECK(UseEnglishTextFrom("gamemodel_tables_test_no_such_file.txt"));

	WriteScratchText("; language\nLANGUAGE 3\n");
	CHECK(UseEnglishTextFrom(kTempFile));

	WriteScratchText("; language\nLANGUAGE 0\n");
	CHECK(!UseEnglishTextFrom(kTempFile));

	WriteScratchText("; nothing but comments\n");
	CHECK(UseEnglishTextFrom(kTempFile));
	RemoveScratch();
}

//----------------------------------------------------------------------
// An option row the file never fills is read like any other: the none
// row (option 0) is what an unoptioned item reports its colour from.
// Every field must start defined; under /RTC1 an unset one reads 0xCC.
//----------------------------------------------------------------------
TEST(ItemOptionInfo, ConstructsWithEveryFieldZero)
{
	ITEMOPTION_INFO info;

	CHECK_EQ(0, info.Part);
	CHECK_EQ(0, info.PlusPoint);
	CHECK_EQ(0, info.PriceMultiplier);
	CHECK_EQ(0, info.RequireSTR);
	CHECK_EQ(0, info.RequireDEX);
	CHECK_EQ(0, info.RequireINT);
	CHECK_EQ(0, info.RequireSUM);
	CHECK_EQ(0, info.RequireLevel);
	CHECK_EQ(0, info.ColorSet);
	CHECK_EQ(0, info.UpgradeOptionType);
	CHECK_EQ(0, info.PreviousOptionType);
	CHECK(info.Name.GetString() == NULL);
	CHECK(info.EName.GetString() == NULL);
}

//----------------------------------------------------------------------
// RankBonus.inf: [count:4], then rows of [type:2][name:MString]
// [level:1][race:1][points:4][icon:2]. Rows are indexed by their file
// position; the type is data, and learned status comes from packets.
// Calling the real constructor and loader also pins the library link:
// removing RankBonusTable.cpp from gamemodel must fail to link.
//----------------------------------------------------------------------
TEST(RankBonusInfo, DefaultsAreAnUnlearnedSlayerBonus)
{
	RankBonusInfo info;
	CHECK_EQ(0, info.GetType());
	CHECK(info.GetName() == NULL);
	CHECK_EQ(0, info.GetLevel());
	CHECK(info.IsSlayerSkill());
	CHECK(!info.IsVampireSkill());
	CHECK(!info.IsOustersSkill());
	CHECK_EQ(0, info.GetPoint());
	CHECK_EQ(0, info.GetSkillIconID());
	CHECK_EQ(RankBonusInfo::STATUS_NULL, info.GetStatus());
}

TEST(RankBonusTable, LoadsEveryRaceAndPreservesTheOnDiskFieldWidths)
{
	Bytes b;
	b.Int(3);
	b.Word(513).Str("Slayer bonus").Byte(5).Byte(RACE_SLAYER).Int(70000).Word(1025);
	b.Word(257).Str("Vampire bonus").Byte(10).Byte(RACE_VAMPIRE).Int(-250).Word(65535);
	b.Word(65535).Str("").Byte(15).Byte(RACE_OUSTERS).Int(123456).Word(256);
	WriteScratch(b);

	RankBonusTable table;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFile(in);
		CHECK(in.good());
		CHECK_EQ(static_cast<std::streamoff>(b.data.size()), static_cast<std::streamoff>(in.tellg()));
	}
	RemoveScratch();

	CHECK_EQ(3, table.GetSize());
	CHECK_EQ(513, table[0].GetType());
	CHECK(StrEq(table[0].GetName(), "Slayer bonus"));
	CHECK_EQ(5, table[0].GetLevel());
	CHECK_EQ(70000, table[0].GetPoint());
	CHECK_EQ(1025, table[0].GetSkillIconID());
	CHECK_EQ(257, table[1].GetType());
	CHECK(StrEq(table[1].GetName(), "Vampire bonus"));
	CHECK_EQ(10, table[1].GetLevel());
	CHECK_EQ(-250, table[1].GetPoint());
	CHECK_EQ(65535, table[1].GetSkillIconID());
	CHECK_EQ(65535, table[2].GetType());
	CHECK(StrEq(table[2].GetName(), ""));
	CHECK_EQ(15, table[2].GetLevel());
	CHECK_EQ(123456, table[2].GetPoint());
	CHECK_EQ(256, table[2].GetSkillIconID());
	for (int i = 0; i < 3; ++i)
	{
		CHECK_EQ(i == 0, table[i].IsSlayerSkill());
		CHECK_EQ(i == 1, table[i].IsVampireSkill());
		CHECK_EQ(i == 2, table[i].IsOustersSkill());
		CHECK_EQ(RankBonusInfo::STATUS_NULL, table[i].GetStatus());
	}

	// Packet-driven status changes are independent for each row.
	table[1].SetStatus(RankBonusInfo::STATUS_LEARNED);
	table[2].SetStatus(RankBonusInfo::STATUS_CANNOT_LEARN);
	CHECK_EQ(RankBonusInfo::STATUS_NULL, table[0].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, table[1].GetStatus());
	CHECK_EQ(RankBonusInfo::STATUS_CANNOT_LEARN, table[2].GetStatus());
}

TEST(RankBonusTable, MissingRowsReturnTheDefaultAndReleaseEmptiesTheTable)
{
	RankBonusTable table;
	table.Init(2);
	table[0].SetStatus(RankBonusInfo::STATUS_LEARNED);
	const RankBonusTable& readOnly = table;
	for (int index : { -1, 2, 65535 })
	{
		CHECK_EQ(0, table[index].GetPoint());
		CHECK(table[index].GetName() == NULL);
		CHECK_EQ(RankBonusInfo::STATUS_NULL, readOnly[index].GetStatus());
		CHECK_EQ(0, table.Get(index).GetType());
	}
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, table[0].GetStatus());
	table.Release();
	CHECK_EQ(0, table.GetSize());
	CHECK(table.GetInternalPointer() == NULL);
	CHECK_EQ(RankBonusInfo::STATUS_NULL, readOnly[0].GetStatus());
}

TEST(RankBonusTable, EmptyReloadClearsExistingRowsAndNegativeCountIsRejected)
{
	RankBonusTable table;
	table.Init(1);
	table[0].SetStatus(RankBonusInfo::STATUS_LEARNED);
	Bytes negative;
	negative.Int(-1);
	WriteScratch(negative);
	table.LoadFromFile(kTempFile);
	CHECK_EQ(1, table.GetSize());
	CHECK_EQ(RankBonusInfo::STATUS_LEARNED, table[0].GetStatus());

	Bytes empty;
	empty.Int(0);
	WriteScratch(empty);
	table.LoadFromFile(kTempFile);
	RemoveScratch();
	CHECK_EQ(0, table.GetSize());
	CHECK(table.GetInternalPointer() == NULL);
}

TEST(GameStringTable, EnglishItemCounterIsReadableEmptyText)
{
    MStringArray table;
    MStringArray* saved = g_pGameStringTable;
    g_pGameStringTable = &table;
    InitGameStringTable();
    const char* counter = table[UI_STRING_MESSAGE_DESC_NUMBER].GetString();
    CHECK(counter != nullptr);
    if (counter) CHECK_EQ(0, std::strlen(counter));
    CHECK(std::strcmp(GetGameString(UI_STRING_MESSAGE_DESC_NUMBER), "") == 0);
    // Older localized tables may omit the counter entirely.
    table.Release();
    CHECK(std::strcmp(GetGameString(UI_STRING_MESSAGE_DESC_NUMBER), "") == 0);
    g_pGameStringTable = saved;
}
