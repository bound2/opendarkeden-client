#include "test_framework.h"
#include "basic/CPositionList.h"
#include "basic/COrderedList.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
constexpr const char* path = "position_list_test.bin";
using Positions = CPositionList<std::uint16_t>;

void Write(const std::vector<char>& bytes, std::size_t limit)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), std::streamsize((std::min)(bytes.size(), limit)));
}

std::vector<char> Bytes(std::initializer_list<std::uint16_t> words)
{
	std::vector<char> result;
	for (auto word : words) {
		result.push_back(char(word));
		result.push_back(char(word >> 8));
	}
	return result;
}

struct Fixture {
	~Fixture() { std::remove(path); }
};
}

TEST(PositionLists, LoadedPositionsAreSortedAndUnique)
{
	Fixture fixture;
	const auto bytes = Bytes({4, 9, 2, 1, 5, 9, 2, 1, 3});
	Write(bytes, bytes.size());
	std::ifstream in(path, std::ios::binary);
	Positions positions;
	positions.LoadFromFile(in);
	CHECK(bool(in));
	CHECK_EQ(3, positions.GetSize());
	CHECK(!positions.Add(9, 2));
	CHECK(positions.Remove(1, 3));
	CHECK(positions.Remove(1, 5));
	CHECK(positions.Remove(9, 2));
	CHECK_EQ(0, positions.GetSize());
}

TEST(PositionLists, TruncatedRecordsPreserveThePreviousList)
{
	Fixture fixture;
	const auto bytes = Bytes({2, 1, 3, 9, 2});
	for (std::size_t limit = 0; limit < bytes.size(); ++limit) {
		Write(bytes, limit);
		std::ifstream in(path, std::ios::binary);
		Positions positions;
		positions.Add(7, 8);
		positions.LoadFromFile(in);
		CHECK(in.fail());
		CHECK_EQ(1, positions.GetSize());
		CHECK(positions.Remove(7, 8));
	}
}

TEST(PositionLists, SavesTheFullWordCountButRefusesAnOverflowBeforeWriting)
{
	Fixture fixture;
	Positions positions;
	for (int x = 65534; x >= 0; --x) positions.Add(std::uint16_t(x), 0);
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		positions.SaveToFile(out);
		CHECK(bool(out));
		CHECK_EQ(2 + 65535 * 4, out.tellp());
	}
	{
		std::ifstream in(path, std::ios::binary);
		Positions loaded;
		loaded.LoadFromFile(in);
		CHECK(bool(in));
		CHECK_EQ(65535, loaded.GetSize());
		CHECK(loaded.Remove(65534, 0));
	}
	positions.Add(65535, 0);
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	positions.SaveToFile(out);
	CHECK(out.fail());
	out.clear();
	CHECK_EQ(0, out.tellp());
}

TEST(PositionLists, AnEmptyRecordClearsTheListAndLeavesTheNextRecordReadable)
{
	Fixture fixture;
	const auto bytes = Bytes({0, 1, 3, 4});
	Write(bytes, bytes.size());
	std::ifstream in(path, std::ios::binary);
	Positions positions;
	positions.Add(5, 6);
	positions.LoadFromFile(in);
	CHECK_EQ(0, positions.GetSize());
	positions.LoadFromFile(in);
	CHECK(bool(in));
	CHECK_EQ(1, positions.GetSize());
	CHECK(positions.Remove(3, 4));
}

TEST(PositionLists, BothContainersExposeMatchingIteratorEnds)
{
	Positions positions;
	COrderedList<int> ordered;
	CHECK(positions.GetIterator() == positions.GetEnd());
	CHECK(ordered.GetIterator() == ordered.GetEnd());
	positions.Add(4, 2);
	positions.Add(1, 3);
	ordered.Add(4);
	ordered.Add(1);
	CHECK_EQ(2, std::distance(positions.GetIterator(), positions.GetEnd()));
	const std::vector<int> values(ordered.GetIterator(), ordered.GetEnd());
	CHECK(values == std::vector<int>({1, 4}));
	ordered -= ordered;
	CHECK(ordered.GetIterator() == ordered.GetEnd());
}
