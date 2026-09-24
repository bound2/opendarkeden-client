#include "test_framework.h"
#include "MMonsterKillQuestInfo.h"
#include <string>

TEST(QuestMetadata, ServerUpdatesCreateAndThenModifyTheOwnedRecord)
{
	MQuestInfoManager quests;
	CHECK(quests.GetInfo(7) == nullptr);
	quests.SetInfo(7, 12, 90, "First quest");
	auto* first = quests.GetInfo(7);
	CHECK(first != nullptr);
	if (!first) return;
	CHECK_EQ(7, first->GetID());
	CHECK_EQ(QUEST_INFO_MONSTER_KILL, first->GetType());
	CHECK_EQ(12, first->GetGoal());
	CHECK_EQ(90, first->GetTimeLimit());
	CHECK(std::string(first->GetName()) == "First quest");
	quests.SetInfo(7, 20, 180, "Updated quest");
	CHECK(quests.GetInfo(7) == first);
	CHECK_EQ(20, first->GetGoal());
	CHECK_EQ(180, first->GetTimeLimit());
	CHECK(std::string(first->GetName()) == "Updated quest");
	quests.SetInfo(8, 1, 30, "Second quest");
	CHECK(quests.GetInfo(8) != nullptr);
	CHECK(quests.RemoveData(7));
	CHECK(quests.GetInfo(7) == nullptr);
	CHECK(quests.GetInfo(8) != nullptr);
}
