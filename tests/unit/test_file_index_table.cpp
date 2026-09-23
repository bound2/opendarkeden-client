#include "test_framework.h"
#include "CFileIndexTable.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {
constexpr const char* path = "file_index_table_test.bin";
struct File { ~File() { std::remove(path); } };
std::vector<char> Encode(const std::vector<std::uint32_t>& offsets)
{
	std::vector<char> bytes{char(offsets.size()), char(offsets.size() >> 8)};
	for (auto offset : offsets)
		for (int shift = 0; shift < 32; shift += 8) bytes.push_back(char(offset >> shift));
	return bytes;
}
void Write(const std::vector<char>& bytes, size_t size = SIZE_MAX)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(bytes.data(), static_cast<std::streamsize>((std::min)(size, bytes.size())));
}
bool Load(CFileIndexTable& table)
{
	std::ifstream input(path, std::ios::binary);
	return table.LoadFromFile(input);
}
void Seed(CFileIndexTable& table)
{
	Write(Encode({2, 20}));
	CHECK(Load(table));
}
void CheckSeed(CFileIndexTable& table)
{
	CHECK_EQ(2, table.GetSize());
	if (table.GetSize() != 2) return;
	CHECK_EQ(2, table[0]);
	CHECK_EQ(20, table[1]);
}
}

TEST(FileIndexTable, TruncatedInputPreservesThePreviousTable)
{
	File file;
	const auto bytes = Encode({100, 200});
	for (size_t size = 0; size < bytes.size(); ++size) {
		CFileIndexTable table;
		Seed(table);
		Write(bytes, size);
		CHECK(!Load(table));
		CheckSeed(table);
	}
}

TEST(FileIndexTable, EmptyInputReplacesThePreviousTable)
{
	File file;
	CFileIndexTable table;
	Seed(table);
	Write(Encode({}));
	CHECK(Load(table));
	CHECK_EQ(0, table.GetSize());
	Seed(table);
	CheckSeed(table);
}

TEST(FileIndexTable, InvalidOffsetsAreRejected)
{
	File file;
	for (std::uint32_t offset : {0u, 1u, 0x80000000u, 0xffffffffu}) {
		CFileIndexTable table;
		Seed(table);
		Write(Encode({2, offset}));
		CHECK(!Load(table));
		CheckSeed(table);
	}
}

TEST(FileIndexTable, AccessChecksTheFullIdAndLeavesMissingOutputUnchanged)
{
	File file;
	CFileIndexTable table;
	long offset = 123;
	CHECK(!table.TryGetOffset(0, offset));
	CHECK_EQ(123, offset);
	Seed(table);
	const auto& readOnly = table;
	CHECK(readOnly.TryGetOffset(1, offset));
	CHECK_EQ(20, offset);
	for (size_t id : {size_t(2), size_t(65535), size_t(65536), SIZE_MAX}) {
		CHECK(!readOnly.TryGetOffset(id, offset));
		CHECK_EQ(20, offset);
		bool rejected = false;
		try { (void)readOnly[id]; } catch (const std::out_of_range&) { rejected = true; }
		CHECK(rejected);
	}
	table.Release();
	table.Release();
	CHECK_EQ(0, table.GetSize());
	CHECK(!table.TryGetOffset(0, offset));
	CHECK_EQ(20, offset);
}

TEST(FileIndexTable, FullWidthOffsetsMayBeRepeatedOrOutOfOrder)
{
	File file;
	CFileIndexTable table;
	Write(Encode({0x7fffffffu, 0x12345678u, 2, 0x12345678u}));
	CHECK(Load(table));
	CHECK_EQ(4, table.GetSize());
	CHECK_EQ(0x7fffffff, table[0]);
	CHECK_EQ(0x12345678, table[1]);
	CHECK_EQ(2, table[2]);
	CHECK_EQ(0x12345678, table[3]);
}

TEST(FileIndexTable, MaximumCountAndFollowingRecordsArePreserved)
{
	File file;
	CFileIndexTable table;
	const auto first = Encode(std::vector<std::uint32_t>(65535, 42));
	auto bytes = first;
	const auto next = Encode({2});
	bytes.insert(bytes.end(), next.begin(), next.end());
	Write(bytes);
	std::ifstream input(path, std::ios::binary);
	CHECK(table.LoadFromFile(input));
	CHECK_EQ(65535, table.GetSize());
	CHECK_EQ(first.size(), input.tellg());
	for (size_t id = 0; id < table.GetSize(); ++id) CHECK_EQ(42, table[id]);
	CHECK(table.LoadFromFile(input));
	CHECK_EQ(1, table.GetSize());
	CHECK_EQ(2, table[0]);
	CHECK_EQ(bytes.size(), input.tellg());
}

TEST(FileIndexTable, CompleteMalformedRecordsRetainTheFollowingCursor)
{
	File file;
	CFileIndexTable table;
	Seed(table);
	auto bytes = Encode({0, 10, 20});
	const auto next = Encode({50});
	bytes.insert(bytes.end(), next.begin(), next.end());
	Write(bytes);
	std::ifstream input(path, std::ios::binary);
	CHECK(!table.LoadFromFile(input));
	CheckSeed(table);
	CHECK_EQ(14, input.tellg());
	CHECK(table.LoadFromFile(input));
	CHECK_EQ(1, table.GetSize());
	CHECK_EQ(50, table[0]);
}

TEST(FileIndexTable, CopiesOwnTheirOffsetsIndependently)
{
	File file;
	CFileIndexTable table;
	Seed(table);
	CFileIndexTable copy(table), assigned;
	assigned = table;
	table.Release();
	CheckSeed(copy);
	CheckSeed(assigned);
	copy = copy;
	CheckSeed(copy);
	copy.Release();
	CheckSeed(assigned);
	assigned = table;
	CHECK_EQ(0, assigned.GetSize());
}

TEST(FileIndexTable, FailedAndThrowingStreamsPreserveTheTable)
{
	File file;
	CFileIndexTable table;
	Seed(table);
	std::ifstream missing;
	CHECK(!table.LoadFromFile(missing));
	CheckSeed(table);
	Write(Encode({100, 200}), 7);
	{
		std::ifstream input(path, std::ios::binary);
		input.exceptions(std::ios::failbit | std::ios::badbit);
		CHECK(!table.LoadFromFile(input));
		CHECK(input.fail());
		CheckSeed(table);
	}
	{
		std::ifstream input(path, std::ios::binary);
		input.setstate(std::ios::failbit);
		CHECK(!table.LoadFromFile(input));
		CHECK(input.fail());
		CheckSeed(table);
	}
}
