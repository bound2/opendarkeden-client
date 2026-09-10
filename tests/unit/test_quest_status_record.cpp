//----------------------------------------------------------------------
// test_quest_status_record.cpp
//----------------------------------------------------------------------
//
// UI_GMissionInfo, the per-mission record the wire layer declares
// (Client/Packet/QuestStatusInfo.h) and the quest manager reads under
// its own name. The sixth clocks slice retyped its time limit from a
// DWORD tick (0 meaning none) to a flag and a MonotonicClock point,
// and the handlers and the manager's XML pass rest on the flag's
// default: a record built with new and never told otherwise has no
// limit. Nothing serialises the record, so this is the one place the
// field's contract is pinned.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "QuestStatusInfo.h"
#include "MonotonicClock.h"

TEST(UIGMissionInfo, AFreshRecordHasNoTimeLimit)
{
	UI_GMissionInfo* pMission = new UI_GMissionInfo;
	CHECK_EQ(false, pMission->bTimeLimited);
	CHECK(pMission->tpTimeLimitStart == MonotonicClock::TimePoint());
	delete pMission;

	UI_GMissionInfo mission;
	CHECK_EQ(false, mission.bTimeLimited);
}

// A quest record owns its missions through the same pointer type the
// manager iterates, so the unification of the two struct families is
// a typedef and not a cast.
TEST(UIGMissionInfo, AQuestRecordHoldsItsMissionsByTheSameType)
{
	UI_GQuestInfo quest;
	UI_GMissionInfo* pMission = new UI_GMissionInfo;
	pMission->bTimeLimited = true;
	pMission->tpTimeLimitStart = MonotonicClock::FromMillis(5000);
	quest.vMissionList.push_back(pMission);
	CHECK_EQ((size_t)1, quest.vMissionList.size());
	CHECK_EQ(true, quest.vMissionList[0]->bTimeLimited);
	CHECK(quest.vMissionList[0]->tpTimeLimitStart == MonotonicClock::FromMillis(5000));
	delete pMission;
}
