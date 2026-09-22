#include "test_framework.h"
#include "ExperienceTable.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <vector>

namespace {
using Bytes = std::vector<unsigned char>;
constexpr const char* kFilename = "experience_input_test.bin";
struct Variant {
	void (ExperienceTable::*load)(std::ifstream&);
	ExpTable ExperienceTable::*rows;
};
constexpr Variant kVariants[] = {
	{&ExperienceTable::LoadFromFileSTR, &ExperienceTable::m_STRExp},
	{&ExperienceTable::LoadFromFileDEX, &ExperienceTable::m_DEXExp},
	{&ExperienceTable::LoadFromFileINT, &ExperienceTable::m_INTExp},
	{&ExperienceTable::LoadFromFileVampire, &ExperienceTable::m_VampireExp},
	{&ExperienceTable::LoadFromFileOusters, &ExperienceTable::m_OustersExp},
	{&ExperienceTable::LoadFromFileSlayerRank, &ExperienceTable::m_SlayerRankExp},
	{&ExperienceTable::LoadFromFileVampireRank, &ExperienceTable::m_VampireRankExp},
	{&ExperienceTable::LoadFromFileOustersRank, &ExperienceTable::m_OustersRankExp},
	{&ExperienceTable::LoadFromFilePetExp, &ExperienceTable::m_PetExp},
	{&ExperienceTable::LoadFromFileAdvanceMent, &ExperienceTable::m_advanceSkillExp}
};

void Int(Bytes& bytes, int value)
{
	const auto bits = static_cast<std::uint32_t>(value);
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes.push_back(static_cast<unsigned char>(bits >> shift));
}

struct Fixture {
	explicit Fixture(const Bytes& bytes) {
		std::ofstream out(kFilename, std::ios::binary | std::ios::trunc);
		if (!bytes.empty()) out.write(reinterpret_cast<const char*>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
	}
	~Fixture() { std::remove(kFilename); }
	std::ifstream Open() const { return std::ifstream(kFilename, std::ios::binary); }
};

void Seed(ExpTable& rows)
{
	rows.Init(2);
	rows.GetInternalPointer()[1].GoalExp = 77;
	rows.GetInternalPointer()[1].AccumExp = 88;
}
}

TEST(ExperienceInput, InvalidCountsPreserveExistingRows)
{
	for (const auto& variant : kVariants)
		for (int count : {-1, 60000, (std::numeric_limits<int>::max)()}) {
			Bytes bytes;
			Int(bytes, count);
			Int(bytes, 1); Int(bytes, 100); Int(bytes, 100);
			Fixture fixture(bytes);
			auto input = fixture.Open();
			ExperienceTable table;
			auto& rows = table.*variant.rows;
			Seed(rows);
			(table.*variant.load)(input);
			CHECK(input.fail());
			CHECK_EQ(2, rows.GetSize());
			CHECK_EQ(77, rows[1].GoalExp);
			CHECK_EQ(88, rows[1].AccumExp);
		}
}

TEST(ExperienceInput, InvalidLevelsFailWithoutChangingARealEntry)
{
	for (const auto& variant : kVariants)
		for (int level : {-1, 2}) {
			Bytes bytes;
			Int(bytes, 1);
			Int(bytes, level); Int(bytes, 100); Int(bytes, 200);
			Fixture fixture(bytes);
			auto input = fixture.Open();
			ExperienceTable table;
			(table.*variant.load)(input);
			const auto& rows = table.*variant.rows;
			CHECK(input.fail());
			CHECK_EQ(2, rows.GetSize());
			CHECK_EQ(0, rows[0].GoalExp);
			CHECK_EQ(0, rows[1].GoalExp);
		}
}

TEST(ExperienceInput, ValidEmptyAndNonemptyLoadsPreserveTheNextRecord)
{
	for (const auto& variant : kVariants)
		for (int count : {0, 1}) {
			Bytes bytes;
			Int(bytes, count);
			if (count) { Int(bytes, 1); Int(bytes, 100); Int(bytes, 200); }
			bytes.push_back(0x7E);
			Fixture fixture(bytes);
			auto input = fixture.Open();
			ExperienceTable table;
			Seed(table.*variant.rows);
			(table.*variant.load)(input);
			const auto& rows = table.*variant.rows;
			CHECK(input.good());
			CHECK_EQ(count + 1, rows.GetSize());
			CHECK_EQ(0, rows[0].GoalExp);
			if (count) {
				CHECK_EQ(100, rows[1].GoalExp);
				CHECK_EQ(200, rows[1].AccumExp);
			}
			CHECK_EQ(0x7E, input.get());
		}
}

TEST(ExperienceInput, EveryTruncatedHeaderOrRowPreservesExistingRows)
{
	Bytes complete;
	Int(complete, 1);
	Int(complete, 1); Int(complete, 100); Int(complete, 200);
	for (const auto& variant : kVariants)
		for (std::size_t size = 0; size < complete.size(); ++size) {
			Fixture fixture(Bytes(complete.begin(), complete.begin() + size));
			auto input = fixture.Open();
			ExperienceTable table;
			auto& rows = table.*variant.rows;
			Seed(rows);
			(table.*variant.load)(input);
			CHECK(input.fail());
			CHECK_EQ(2, rows.GetSize());
			CHECK_EQ(77, rows[1].GoalExp);
			CHECK_EQ(88, rows[1].AccumExp);
		}
}

TEST(ExperienceInput, FailedStreamCannotReplaceExistingRows)
{
	Bytes bytes;
	Int(bytes, 1);
	Int(bytes, 1); Int(bytes, 100); Int(bytes, 200);
	for (const auto& variant : kVariants) {
		Fixture fixture(bytes);
		auto input = fixture.Open();
		input.setstate(std::ios::failbit);
		ExperienceTable table;
		auto& rows = table.*variant.rows;
		Seed(rows);
		(table.*variant.load)(input);
		CHECK(input.fail());
		CHECK_EQ(2, rows.GetSize());
		CHECK_EQ(77, rows[1].GoalExp);
		CHECK_EQ(88, rows[1].AccumExp);
	}
}

TEST(ExperienceInput, InvalidLaterLevelKeepsOnlyCompletedRows)
{
	Bytes bytes;
	Int(bytes, 2);
	Int(bytes, 1); Int(bytes, 100); Int(bytes, 200);
	Int(bytes, 3); Int(bytes, 300); Int(bytes, 400);
	for (const auto& variant : kVariants) {
		Fixture fixture(bytes);
		auto input = fixture.Open();
		ExperienceTable table;
		(table.*variant.load)(input);
		const auto& rows = table.*variant.rows;
		CHECK(input.fail());
		CHECK_EQ(3, rows.GetSize());
		CHECK_EQ(100, rows[1].GoalExp);
		CHECK_EQ(200, rows[1].AccumExp);
		CHECK_EQ(0, rows[2].GoalExp);
		CHECK_EQ(0u, rows[2].AccumExp);
		CHECK_EQ(0, rows[3].GoalExp);
	}
}
