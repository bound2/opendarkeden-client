#include "test_framework.h"
#include "ShrineInfoManager.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

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
