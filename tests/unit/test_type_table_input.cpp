#include "test_framework.h"
#include "Platform.h"
#include "MStringArray.h"

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<unsigned char>;
constexpr const char* kFilename = "type_table_input_test.bin";

void Int(Bytes& bytes, int value)
{
	const auto bits = static_cast<std::uint32_t>(value);
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes.push_back(static_cast<unsigned char>(bits >> shift));
}

void Index(Bytes& bytes, unsigned value)
{
	bytes.push_back(static_cast<unsigned char>(value));
	bytes.push_back(static_cast<unsigned char>(value >> 8));
}

void String(Bytes& bytes, const std::string& value)
{
	Int(bytes, static_cast<int>(value.size()));
	bytes.insert(bytes.end(), value.begin(), value.end());
}

struct Fixture
{
	explicit Fixture(const Bytes& bytes)
	{
		std::ofstream output(kFilename, std::ios::binary | std::ios::trunc);
		if (!bytes.empty())
			output.write(reinterpret_cast<const char*>(bytes.data()),
				static_cast<std::streamsize>(bytes.size()));
	}
	~Fixture() { std::remove(kFilename); }
	std::ifstream Open() const { return std::ifstream(kFilename, std::ios::binary); }
};

void Seed(MStringArray& table)
{
	table.Init(2);
	table[0] = "keep zero";
	table[1] = "keep one";
}

void CheckRow(const MStringArray& table, int index, const char* expected)
{
	const char* actual = table[index].GetString();
	CHECK(actual != nullptr);
	if (actual) CHECK(std::string(actual) == expected);
}

void CheckUnchanged(const MStringArray& table)
{
	CHECK_EQ(2, table.GetSize());
	CheckRow(table, 0, "keep zero");
	CheckRow(table, 1, "keep one");
}

void Load(MStringArray& table, std::ifstream& input, bool nickname)
{
	if (nickname) CHECK(!table.LoadFromFile_NickNameString(input));
	else table.LoadFromFile(input);
}
}

TEST(TypeTableInput, ShortCountNeverReplacesExistingRows)
{
	for (bool nickname : {false, true})
		for (size_t length = 0; length < 4; ++length) {
			Fixture fixture(Bytes(length, 0));
			auto input = fixture.Open();
			MStringArray table;
			Seed(table);
			Load(table, input, nickname);
			CHECK(input.fail());
			CheckUnchanged(table);
		}
}

TEST(TypeTableInput, RejectsUnbackedCountsBeforeReplacingRows)
{
	for (bool nickname : {false, true})
		for (int count : {-1, 1, 2, 60000, 65536, 65537, (std::numeric_limits<int>::max)()}) {
			Bytes bytes;
			Int(bytes, count);
			// Enough to initialize one nickname index, never enough entries.
			if (nickname) Index(bytes, 0);
			Fixture fixture(bytes);
			auto input = fixture.Open();
			MStringArray table;
			Seed(table);
			Load(table, input, nickname);
			CHECK(input.fail());
			CheckUnchanged(table);
		}
}

TEST(TypeTableInput, AlreadyFailedInputPreservesItsStateAndTheTable)
{
	Bytes bytes;
	Int(bytes, 0);
	for (bool nickname : {false, true})
		for (auto state : {std::ios::failbit, std::ios::badbit, std::ios::eofbit}) {
			Fixture fixture(bytes);
			auto input = fixture.Open();
			input.setstate(state);
			MStringArray table;
			Seed(table);
			Load(table, input, nickname);
			CHECK((input.rdstate() & state) != 0);
			CHECK(input.fail());
			CheckUnchanged(table);
		}
}

TEST(TypeTableInput, NicknameFailureKeepsCompletedRowsAndRejectsTheUnreadTail)
{
	for (bool shortIndex : {false, true}) {
		Bytes bytes;
		Int(bytes, 2);
		Index(bytes, 1);
		String(bytes, "new one");
		bytes.push_back(0);
		if (!shortIndex) {
			bytes.push_back(0);
			Int(bytes, 4);
			bytes.push_back('a'); // Truncated string body after a complete index.
		}
		Fixture fixture(bytes);
		auto input = fixture.Open();
		MStringArray table;
		Seed(table);
		CHECK(!table.LoadFromFile_NickNameString(input));
		CHECK(input.fail());
		CHECK_EQ(2, table.GetSize());
		CheckRow(table, 0, "keep zero");
		CheckRow(table, 1, "new one");
	}
}

TEST(TypeTableInput, NicknameRangeFailureMarksTheStream)
{
	Bytes bytes;
	Int(bytes, 2);
	Index(bytes, 1);
	String(bytes, "new one");
	Index(bytes, 2);
	String(bytes, "outside");
	Fixture fixture(bytes);
	auto input = fixture.Open();
	MStringArray table;
	Seed(table);
	CHECK(!table.LoadFromFile_NickNameString(input));
	CHECK(input.fail());
	CheckRow(table, 0, "keep zero");
	CheckRow(table, 1, "new one");
}

TEST(TypeTableInput, ValidEmptyAndNonemptyTablesLeaveTheFollowingRecordReadable)
{
	for (bool nickname : {false, true})
		for (bool empty : {false, true}) {
			Bytes bytes;
			Int(bytes, empty ? 0 : 2);
			if (!empty) {
				if (nickname) Index(bytes, 1);
				String(bytes, "first");
				if (nickname) Index(bytes, 0);
				String(bytes, "second");
			}
			bytes.push_back(0x7E);
			Fixture fixture(bytes);
			auto input = fixture.Open();
			MStringArray table;
			Seed(table);
			if (nickname) CHECK(table.LoadFromFile_NickNameString(input));
			else table.LoadFromFile(input);
			CHECK(input.good());
			CHECK_EQ(empty ? 0 : 2, table.GetSize());
			if (!empty) {
				CheckRow(table, nickname ? 1 : 0, "first");
				CheckRow(table, nickname ? 0 : 1, "second");
			}
			CHECK_EQ(0x7E, input.get());
		}
}
