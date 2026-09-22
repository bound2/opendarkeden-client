#include "test_framework.h"
#include "ShrineInfoManager.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <new>
#include <stdexcept>

namespace {
struct TowerLines {
	std::vector<std::string> lines;
	size_t cursor = 0;
	RegenTowerLineReader Reader() {
		return {this, [](void* context, char* line, int capacity) {
			auto& source = *static_cast<TowerLines*>(context);
			if (source.cursor == source.lines.size() || capacity <= 0) return false;
			const auto& text = source.lines[source.cursor++];
			const size_t size = (std::min)(text.size(), static_cast<size_t>(capacity - 1));
			std::memcpy(line, text.data(), size);
			line[size] = '\0';
			return true;
		}};
	}
};
}

TEST(RegenTower, ExtractedCoreReadsTheExistingLineFormat)
{
	RegenTowerInfoManager table;
	CHECK(!table.LoadRegenTowerInfoLines({}));
	CHECK_EQ(0, table.GetSize());
	TowerLines source{{"; positions", "*2", "0 72 18 21", "", "1 73 5 250"}};
	CHECK(table.LoadRegenTowerInfoLines(source.Reader()));
	CHECK_EQ(2, table.GetSize());
	CHECK_EQ(0, table.Get(0).num);
	CHECK_EQ(72, table.Get(0).zoneID);
	CHECK_EQ(18, table.Get(0).x);
	CHECK_EQ(21, table.Get(0).y);
	CHECK_EQ(1, table.Get(1).num);
	CHECK_EQ(73, table.Get(1).zoneID);
	CHECK_EQ(5, table.Get(1).x);
	CHECK_EQ(250, table.Get(1).y);
	CHECK_EQ(-1, table.Get(1).owner);
}

TEST(RegenTower, FreshRecordsHaveAnInvalidNumber)
{
	alignas(RegenTowerInfo) unsigned char storage[sizeof(RegenTowerInfo)];
	std::memset(storage, 0x5a, sizeof(storage));
	auto* row = new (storage) RegenTowerInfo;
	CHECK_EQ(-1, row->num);
	CHECK_EQ(-1, row->zoneID);
	CHECK_EQ(0, row->x);
	CHECK_EQ(0, row->y);
	CHECK_EQ(-1, row->owner);
	row->~RegenTowerInfo();
}

TEST(RegenTower, IncompleteOrInvalidRowsPreserveEveryPreviousField)
{
	for (const auto* bad : {"", " ", "2", "2 74", "2 74 99", "2 74 99 no",
		"-1 74 1 2", "1 -1 1 2", "1 74 -1 2", "1 74 1 -1", "1 74 1 2 extra"}) {
		RegenTowerInfo row;
		char valid[] = "0 72 18 21";
		row.LoadFromLine(valid);
		row.owner = 2;
		std::string line(bad);
		row.LoadFromLine(line.data());
		CHECK_EQ(0, row.num);
		CHECK_EQ(72, row.zoneID);
		CHECK_EQ(18, row.x);
		CHECK_EQ(21, row.y);
		CHECK_EQ(2, row.owner);
	}
}

TEST(RegenTower, InvalidTablesCannotReplaceTheLoadedPositions)
{
	const std::vector<std::vector<std::string>> malformed{
		{}, {"; no header"}, {"*-1"}, {"*4097"},
		{"0 73 1 2"}, {"*2", "0 73 1 2"},
		{"*1", "0 73 1"}, {"*1", "1 73 1 2"}, {"*1", "-1 73 1 2"},
		{"*2", "0 73 1 2", "0 74 3 4"}, {"*1", "0 73 1 2", "0 74 3 4"},
		{"*1", "0 73 1 2 trailing"}
	};
	for (const auto& lines : malformed) {
		RegenTowerInfoManager table;
		TowerLines original{{"*1", "0 72 18 21"}};
		CHECK(table.LoadRegenTowerInfoLines(original.Reader()));
		table.GetMutable(0)->owner = 2;
		TowerLines source{lines};
		CHECK(!table.LoadRegenTowerInfoLines(source.Reader()));
		CHECK_EQ(1, table.GetSize());
		// Every constructor initializes these fields even on the old parser;
		// do not inspect its unread, uninitialized row number on failure.
		CHECK_EQ(72, table.Get(0).zoneID);
		CHECK_EQ(18, table.Get(0).x);
		CHECK_EQ(21, table.Get(0).y);
		CHECK_EQ(2, table.Get(0).owner);
	}
}

TEST(RegenTower, AnExplicitEmptyTableClearsPreviouslyLoadedRows)
{
	RegenTowerInfoManager table;
	TowerLines original{{"*1", "0 72 18 21"}};
	CHECK(table.LoadRegenTowerInfoLines(original.Reader()));
	TowerLines empty{{"*0"}};
	CHECK(table.LoadRegenTowerInfoLines(empty.Reader()));
	CHECK_EQ(0, table.GetSize());
	CHECK(table.GetMutable(0) == nullptr);
}

TEST(RegenTower, FileRowsMustFitTheThreeMinimapsAndTheWireIdentifier)
{
	for (const auto* bad : {"0 70 1 2", "0 74 1 2", "0 2147483647 1 2",
		"0 71 128 2", "0 71 1 256", "0 71 2147483647 2",
		"0 71 1 2147483647", "256 71 1 2", "2147483647 71 1 2"}) {
		RegenTowerInfo row;
		CHECK(row.LoadFromLine("0 72 18 21"));
		CHECK(!row.LoadFromLine(bad));
		CHECK_EQ(0, row.num);
		CHECK_EQ(72, row.zoneID);
		CHECK_EQ(18, row.x);
		CHECK_EQ(21, row.y);
	}
	RegenTowerInfoManager table;
	TowerLines tooMany{{"*257"}};
	CHECK(!table.LoadRegenTowerInfoLines(tooMany.Reader()));
	CHECK_EQ(0, table.GetSize());
}

TEST(RegenTower, CompleteUnorderedTablesAndGeometryEdgesRemainValid)
{
	RegenTowerInfoManager table;
	TowerLines source{{" \t; comment", " *+256 ; towers"}};
	for (int id = 255; id >= 0; --id)
		source.lines.push_back(std::to_string(id) + " 73 127 255 ; edge");
	CHECK(table.LoadRegenTowerInfoLines(source.Reader()));
	CHECK_EQ(256, table.GetSize());
	for (int id = 0; id < table.GetSize(); ++id) {
		CHECK_EQ(id, table.Get(id).num);
		CHECK(table.Get(id).IsValid());
	}
	TowerLines smaller{{"*+1", "+0 +71 +0 +0"}};
	CHECK(table.LoadRegenTowerInfoLines(smaller.Reader()));
	CHECK_EQ(1, table.GetSize());
	CHECK(table.Get(0).IsValid());
	CHECK(!table.Get(1).IsValid());
	// The render and hit-test callers use the same predicate before indexing
	// their rectangle array, including when another owner mutates a record.
	table.GetMutable(0)->zoneID = 74;
	CHECK(!table.Get(0).IsValid());
}

TEST(RegenTower, NumericOverflowAndIncompleteHeadersAreRejected)
{
	for (const auto* header : {"*", "*no", "*+", "*2147483648", "*-2147483649",
		"*1junk", "*1 2", "*1", "*1; duplicate"}) {
		RegenTowerInfoManager table;
		TowerLines original{{"*1", "0 72 18 21"}};
		CHECK(table.LoadRegenTowerInfoLines(original.Reader()));
		TowerLines source{{header}};
		if (std::string(header) == "*1; duplicate") source.lines.push_back("*1");
		CHECK(!table.LoadRegenTowerInfoLines(source.Reader()));
		CHECK_EQ(1, table.GetSize());
		CHECK_EQ(72, table.Get(0).zoneID);
	}
	for (const auto* line : {"2147483648 71 1 2", "0 2147483648 1 2",
		"0 71 2147483648 2", "0 71 1 -2147483649", "+-1 71 1 2", "0+71 1 2"}) {
		RegenTowerInfo row;
		CHECK(row.LoadFromLine("0 72 18 21"));
		CHECK(!row.LoadFromLine(line));
		CHECK_EQ(72, row.zoneID);
	}
	RegenTowerInfo row;
	CHECK(!row.LoadFromLine(nullptr));
	CHECK(!row.IsValid());
}

TEST(RegenTower, ClippedLinesAndExcessiveInputCannotBeAcceptedAsComplete)
{
	const std::string prefix = "0 71 0 0 ;";
	for (size_t size : {511U, 512U, 1000U}) {
		RegenTowerInfoManager table;
		TowerLines source{{"*1", prefix + std::string(size - prefix.size(), 'x')}};
		CHECK_EQ(size == 511, table.LoadRegenTowerInfoLines(source.Reader()));
		CHECK_EQ(size == 511 ? 1 : 0, table.GetSize());
	}
	RegenTowerInfoManager table;
	TowerLines excessive{{"*0"}};
	excessive.lines.insert(excessive.lines.end(), 2048, ";" + std::string(510, 'x'));
	CHECK(!table.LoadRegenTowerInfoLines(excessive.Reader()));
	size_t calls = 0;
	const RegenTowerLineReader endless{&calls, [](void* context, char* line, int) {
		++*static_cast<size_t*>(context);
		line[0] = ';'; line[1] = '\0';
		return true;
	}};
	CHECK(!table.LoadRegenTowerInfoLines(endless));
	CHECK(calls > 0 && calls <= 20000);
}

TEST(RegenTower, FailedReadersCannotReplaceTheCurrentTable)
{
	RegenTowerInfoManager table;
	TowerLines original{{"*1", "0 72 18 21"}};
	CHECK(table.LoadRegenTowerInfoLines(original.Reader()));
	const RegenTowerLineReader throwing{nullptr, [](void*, char*, int) -> bool {
		throw std::runtime_error("read failed");
	}};
	CHECK(!table.LoadRegenTowerInfoLines(throwing));
	CHECK_EQ(1, table.GetSize());
	CHECK_EQ(72, table.Get(0).zoneID);
	const RegenTowerLineReader unterminated{nullptr, [](void*, char* line, int capacity) {
		std::memset(line, '0', static_cast<size_t>(capacity));
		return true;
	}};
	CHECK(!table.LoadRegenTowerInfoLines(unterminated));
	CHECK_EQ(1, table.GetSize());
	CHECK_EQ(72, table.Get(0).zoneID);
}
