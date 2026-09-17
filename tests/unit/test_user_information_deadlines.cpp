//----------------------------------------------------------------------
// test_user_information_deadlines.cpp
//----------------------------------------------------------------------
//
// UserInformation's two deadlines on the frame stamp (gamemodel): the
// scheduled logout, with its whole-second countdown, and the point
// after which an item may be dropped again. Both were DWORD ticks
// compared as "now > stamp + delay"; these pin the contract over the
// time point, including the legacy wrap the sum failed at.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "UserInformation.h"

namespace {

MonotonicClock::TimePoint
At(unsigned long long ull_millisec)
{
	return MonotonicClock::FromMillis(ull_millisec);
}

const unsigned long long	LEGACY_WRAP = 0x100000000ull;

} // namespace

TEST(UserInformationDeadlines, StartsWithNoLogoutAndDropsAllowed)
{
	UserInformation info;

	CHECK(!info.IsLogoutScheduled());
	CHECK(!info.IsLogoutDue(At(1000)));
	CHECK_EQ(0u, info.SecondsToLogout(At(1000)));
	CHECK(info.IsItemDropEnabled(At(1000)));
}

TEST(UserInformationDeadlines, LogoutCountsDownInWholeSecondsAndIsDueOnceStrictlyPast)
{
	UserInformation info;
	info.LogoutTime = At(10000) + MonotonicClock::Millis(5000);

	CHECK(info.IsLogoutScheduled());
	CHECK_EQ(5u, info.SecondsToLogout(At(10000)));
	CHECK_EQ(4u, info.SecondsToLogout(At(11000)));
	CHECK_EQ(3u, info.SecondsToLogout(At(11001)));
	CHECK_EQ(0u, info.SecondsToLogout(At(14500)));

	CHECK(!info.IsLogoutDue(At(14999)));
	CHECK(!info.IsLogoutDue(At(15000)));
	CHECK(info.IsLogoutDue(At(15001)));
	CHECK_EQ(0u, info.SecondsToLogout(At(15001)));
}

TEST(UserInformationDeadlines, CancelLogoutClearsTheSchedule)
{
	UserInformation info;
	info.LogoutTime = At(10000) + MonotonicClock::Millis(5000);
	info.CancelLogout();

	CHECK(!info.IsLogoutScheduled());
	CHECK(!info.IsLogoutDue(At(20000)));
	CHECK_EQ(0u, info.SecondsToLogout(At(10000)));
}

TEST(UserInformationDeadlines, LogoutScheduledAcrossTheLegacyWrapIsNotDueEarly)
{
	// The DWORD sum wrapped: (2^32 - 1000) + 5000 is 4000 mod 2^32, and
	// "now > 4000" logged the player out at once.
	const DWORD dw_now = (DWORD)(LEGACY_WRAP - 1000);
	const DWORD dw_old_deadline = dw_now + 5000;
	CHECK(dw_now > dw_old_deadline);

	UserInformation info;
	info.LogoutTime = At(LEGACY_WRAP - 1000) + MonotonicClock::Millis(5000);

	CHECK(!info.IsLogoutDue(At(LEGACY_WRAP - 1000)));
	CHECK_EQ(5u, info.SecondsToLogout(At(LEGACY_WRAP - 1000)));
	CHECK(!info.IsLogoutDue(At(LEGACY_WRAP + 4000)));
	CHECK(info.IsLogoutDue(At(LEGACY_WRAP + 4001)));
}

TEST(UserInformationDeadlines, ItemDropIsEnabledStrictlyAfterThePoint)
{
	UserInformation info;
	info.ItemDropEnableTime = At(30000);

	CHECK(!info.IsItemDropEnabled(At(29999)));
	CHECK(!info.IsItemDropEnabled(At(30000)));
	CHECK(info.IsItemDropEnabled(At(30001)));
}
