//----------------------------------------------------------------------
// test_time_item_manager.cpp
//----------------------------------------------------------------------
//
// MTimeItemManager (Client/MTimeItemManager.h) - the register of items
// that carry a time limit. The client puts an object id in it when
// GCTimeLimitItemInfo says how many seconds are left on that item, and
// the description panel asks it for the days, hours, minutes and seconds
// still to run.
//
// Its stored deadline moved from a DWORD holding
// "timeGetTime()/1000 + lifetime" to an absolute point on
// MonotonicClock's clock, kept at whole-second resolution
// (docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md,
// modernization backlog priority 5). Three things are pinned here, and
// they are different kinds of claim.
//
// The first is the arithmetic that must NOT change: the countdown splits
// a remaining count of seconds the same way it always did; an item is
// expired when that count reaches zero and not a millisecond earlier;
// an item added part-way through a second still expires on the clock's
// second boundary rather than on its own, which costs it up to a second
// and is the rounding the shipped code has always had.
//
// The second is the defect the move removes. The old deadline was a
// 32-bit millisecond tick divided by 1000, so it went round every 49.7
// days; when it did, an item that was long past its deadline read as up
// to 49.7 days short of it. That is not reachable by waiting, so it is
// driven here through the injected clock, and each of those tests also
// works the old DWORD arithmetic out on the same numbers so the
// difference is visible rather than asserted.
//
// The third is that the register still works on the real clock, with no
// source injected - the epoch there is steady_clock's and not
// SDL_GetTicks', and flooring it to a whole second has to behave.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MonotonicClock.h"
#include "MTimeItemManager.h"

namespace {

//----------------------------------------------------------------------
// The injected clock: one absolute millisecond count the tests move
// wherever they need it, including past 2^32.
//----------------------------------------------------------------------
unsigned long long	g_ull_fake_millisec = 0;

MonotonicClock::TimePoint
FakeNow()
{
	return MonotonicClock::FromMillis(g_ull_fake_millisec);
}

void
SetNow(unsigned long long ull_millisec)
{
	g_ull_fake_millisec = ull_millisec;
}

//----------------------------------------------------------------------
// The wrap point of the legacy 32-bit millisecond tick.
//----------------------------------------------------------------------
const unsigned long long	LEGACY_WRAP = 0x100000000ull;

//----------------------------------------------------------------------
// One day, one hour, one minute and one second, in seconds.
//----------------------------------------------------------------------
const DWORD	ONE_OF_EACH = 86400 + 3600 + 60 + 1;

} // anonymous namespace


//----------------------------------------------------------------------
// The countdown
//----------------------------------------------------------------------

TEST(TimeItemManager, TheCountdownSplitsTheSecondsItWasGiven)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	SetNow(0);

	MTimeItemManager reg;
	CHECK(reg.AddTimeItem(1, ONE_OF_EACH));

	CHECK_EQ(1, reg.GetDay(1));
	CHECK_EQ(1, reg.GetHour(1));
	CHECK_EQ(1, reg.GetMinute(1));
	CHECK_EQ(1, reg.GetSecond(1));
	CHECK_EQ(false, reg.IsExpired(1));

	// A day in: the days field empties and the rest is untouched.
	SetNow(86400ull * 1000);
	CHECK_EQ(0, reg.GetDay(1));
	CHECK_EQ(1, reg.GetHour(1));
	CHECK_EQ(1, reg.GetMinute(1));
	CHECK_EQ(1, reg.GetSecond(1));
}

TEST(TimeItemManager, AnUnknownItemReportsMinusOneAndCountsAsExpired)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	SetNow(0);

	MTimeItemManager reg;

	CHECK_EQ(false, reg.IsExist(9));
	CHECK_EQ(-1, reg.GetDay(9));
	CHECK_EQ(-1, reg.GetHour(9));
	CHECK_EQ(-1, reg.GetMinute(9));
	CHECK_EQ(-1, reg.GetSecond(9));

	// Not in the register means "no time left on it". IsExpired has no
	// caller in the tree today - VS_UI_Description open-codes the same
	// test over the four accessors - so this pins the contract as
	// written rather than as used.
	CHECK(reg.IsExpired(9));
}

TEST(TimeItemManager, ExpiresWhenTheDeadlineIsReachedAndNotOneMillisecondEarlier)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	SetNow(0);

	MTimeItemManager reg;
	reg.AddTimeItem(1, 60);

	SetNow(59999);
	CHECK_EQ(0, reg.GetMinute(1));
	CHECK_EQ(1, reg.GetSecond(1));
	CHECK_EQ(false, reg.IsExpired(1));

	SetNow(60000);
	CHECK_EQ(0, reg.GetDay(1));
	CHECK_EQ(0, reg.GetHour(1));
	CHECK_EQ(0, reg.GetMinute(1));
	CHECK_EQ(0, reg.GetSecond(1));
	CHECK(reg.IsExpired(1));

	// An expired item is still IN the register: it reports zero, not -1,
	// and the item is only removed when something removes it.
	CHECK(reg.IsExist(1));
}

TEST(TimeItemManager, TheCountdownStepsOnTheClocksSecondAndNotTheItemsOwn)
{
	// The shipped rounding, preserved deliberately. AddTimeItem stores
	// "the current whole second plus the lifetime", so an item added 500
	// ms into a second gets 59.5 seconds of a 60 second lifetime. Making
	// the deadline millisecond-accurate would move every countdown the
	// description panel draws, so it is pinned here instead.
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	SetNow(1500);

	MTimeItemManager reg;
	reg.AddTimeItem(1, 60);

	// 59.499 seconds after the add.
	SetNow(60999);
	CHECK_EQ(1, reg.GetSecond(1));
	CHECK_EQ(false, reg.IsExpired(1));

	// 59.5 seconds after the add.
	SetNow(61000);
	CHECK(reg.IsExpired(1));
}

TEST(TimeItemManager, ZeroSecondsIsExpiredImmediately)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	SetNow(1234);

	MTimeItemManager reg;
	CHECK(reg.AddTimeItem(1, 0));

	CHECK(reg.IsExist(1));
	CHECK(reg.IsExpired(1));
	CHECK_EQ(0, reg.GetSecond(1));
}


//----------------------------------------------------------------------
// The register itself
//----------------------------------------------------------------------

TEST(TimeItemManager, AddingTheSameItemAgainRestartsItsCountdown)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	SetNow(0);

	MTimeItemManager reg;
	reg.AddTimeItem(1, 60);

	SetNow(30000);
	CHECK_EQ(30, reg.GetSecond(1));

	// The server re-sending the item's remaining time replaces the
	// deadline rather than adding a second entry.
	reg.AddTimeItem(1, 60);
	CHECK_EQ(1u, (unsigned)reg.size());
	CHECK_EQ(1, reg.GetMinute(1));
	CHECK_EQ(0, reg.GetSecond(1));

	SetNow(89999);
	CHECK_EQ(false, reg.IsExpired(1));

	SetNow(90000);
	CHECK(reg.IsExpired(1));
}

TEST(TimeItemManager, ItemsAreIndependentAndRemovalTakesOnlyTheOneNamed)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	SetNow(0);

	MTimeItemManager reg;
	reg.AddTimeItem(1, 60);
	reg.AddTimeItem(2, 120);

	CHECK_EQ(1, reg.GetMinute(1));
	CHECK_EQ(2, reg.GetMinute(2));

	CHECK(reg.RemoveTimeItem(1));
	CHECK_EQ(false, reg.RemoveTimeItem(1));
	CHECK_EQ(false, reg.IsExist(1));
	CHECK_EQ(2, reg.GetMinute(2));

	// clear() comes from the std::map base and is what GameMain calls on
	// a zone change; the mapped type changing must not have cost it.
	reg.clear();
	CHECK_EQ(false, reg.IsExist(2));
	CHECK(reg.empty());
}


//----------------------------------------------------------------------
// The wrap, which is what the migration was for
//----------------------------------------------------------------------

TEST(TimeItemManager, KeepsCountingAcrossTheWrapOfTheLegacyThirtyTwoBitTick)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);

	// A whole second, 30.296 seconds before the 32-bit millisecond tick
	// goes round, with a 60 second item: the deadline is on the far side
	// of the wrap.
	const unsigned long long	ull_start = 4294937000ull;

	SetNow(ull_start);
	CHECK(MonotonicClock::LegacyTicks() > 0xFFFF0000u);

	MTimeItemManager reg;
	reg.AddTimeItem(1, 60);

	// What the old code would have stored: the tick divided by 1000,
	// plus the lifetime.
	const DWORD	dw_legacy_deadline = 60u + (DWORD)(4294937000u / 1000u);
	CHECK_EQ(4294997u, dw_legacy_deadline);

	// Half a minute in, with the tick now past its wrap and reading a
	// small number again.
	SetNow(ull_start + 30500);
	CHECK(MonotonicClock::LegacyTicks() < 1000u);
	CHECK_EQ(30, reg.GetSecond(1));
	CHECK_EQ(false, reg.IsExpired(1));

	// One millisecond short of the deadline.
	SetNow(ull_start + 59999);
	CHECK_EQ(1, reg.GetSecond(1));
	CHECK_EQ(false, reg.IsExpired(1));

	// And exactly on it. This is where the old arithmetic went wrong:
	// the tick reads 29704 ms, so the current second is 29, which is not
	// past a deadline of 4294997 - the item reads as 49 days short of
	// expiring instead of expired.
	SetNow(ull_start + 60000);

	const DWORD	dw_legacy_now = MonotonicClock::LegacyTicks() / 1000u;
	CHECK_EQ(29u, dw_legacy_now);
	CHECK(!(dw_legacy_now > dw_legacy_deadline));
	CHECK_EQ(4294968u, dw_legacy_deadline - dw_legacy_now);
	CHECK_EQ(49, (int)((dw_legacy_deadline - dw_legacy_now) / 60 / 60 / 24));

	CHECK(reg.IsExpired(1));
	CHECK_EQ(0, reg.GetDay(1));
	CHECK_EQ(0, reg.GetSecond(1));
}

TEST(TimeItemManager, DoesNotLoseAnItemToMoreThanTheLegacyTickCanCount)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	SetNow(1000);

	MTimeItemManager reg;
	reg.AddTimeItem(1, 60);

	// 49.7 days and 50 ms later. The 32-bit tick has lost the whole 2^32
	// and reads 1050 ms, so the old code's current second is 1 - exactly
	// where it stood when the item was added - and the stored deadline of
	// 61 still has a full minute left to run.
	const unsigned long long	ull_late = 1000 + LEGACY_WRAP + 50;

	SetNow(ull_late);

	const DWORD	dw_legacy_now = MonotonicClock::LegacyTicks() / 1000u;
	CHECK_EQ(1u, dw_legacy_now);
	CHECK_EQ(60u, (60u + 1u) - dw_legacy_now);

	// The typed deadline kept all of it.
	CHECK(reg.IsExpired(1));
	CHECK_EQ(0, reg.GetDay(1));
	CHECK_EQ(0, reg.GetHour(1));
	CHECK_EQ(0, reg.GetMinute(1));
	CHECK_EQ(0, reg.GetSecond(1));
}

TEST(TimeItemManager, ALifetimeTheOldSumCouldNotHoldIsNoLongerInstantlyExpired)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	SetNow(2000000);

	// The largest lifetime GCTimeLimitItemInfo can carry: a DWORD of
	// seconds, which is a little over 136 years.
	MTimeItemManager reg;
	reg.AddTimeItem(1, 0xFFFFFFFFu);

	// The old code added that to the current second in DWORD arithmetic,
	// where it wrapped to a deadline one second behind the present - so
	// the item was expired the moment it arrived.
	const DWORD	dw_lifetime = 0xFFFFFFFFu;
	const DWORD	dw_legacy_now = 2000u;
	const DWORD	dw_legacy_deadline = dw_lifetime + dw_legacy_now;

	CHECK_EQ(1999u, dw_legacy_deadline);
	CHECK(dw_legacy_now > dw_legacy_deadline);

	// 49710 days, 6 hours, 28 minutes and 15 seconds.
	CHECK_EQ(false, reg.IsExpired(1));
	CHECK_EQ(49710, reg.GetDay(1));
	CHECK_EQ(6, reg.GetHour(1));
	CHECK_EQ(28, reg.GetMinute(1));
	CHECK_EQ(15, reg.GetSecond(1));
}


//----------------------------------------------------------------------
// The real clock
//----------------------------------------------------------------------

TEST(TimeItemManager, WorksOnTheRealClockWithNoSourceInjected)
{
	// steady_clock's epoch is not SDL_GetTicks' and its count is much
	// larger; flooring it to a whole second and adding a lifetime has to
	// behave there too. Only assertions that cannot move with how long
	// this test takes to run belong here, so the lifetime is ten and a
	// half days and the field read is the one that is half a day from
	// its next step.
	CHECK(MonotonicClock::GetTestSource() == NULL);

	MTimeItemManager reg;
	CHECK(reg.AddTimeItem(1, 10 * 86400 + 12 * 3600));

	CHECK(reg.IsExist(1));
	CHECK_EQ(false, reg.IsExpired(1));
	CHECK_EQ(10, reg.GetDay(1));

	// And the deterministic end of the same path: a zero lifetime lands
	// on the current second, so it is expired without waiting.
	CHECK(reg.AddTimeItem(2, 0));
	CHECK(reg.IsExpired(2));
	CHECK_EQ(0, reg.GetDay(2));
}


//----------------------------------------------------------------------
// The remaining count is a 64-bit number of seconds all the way to the
// accessors. A clock that goes backwards cannot happen on the real
// steady_clock, but the injected source can do it, and a remainder that
// no longer fits a DWORD must not be truncated on its way to GetDay:
// 5 000 000 060 seconds is 57 870 days, and a DWORD would have made it
// 8 160.
//----------------------------------------------------------------------
TEST(TimeItemManager, TheRemainingCountIsNotNarrowedToThirtyTwoBits)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	SetNow(5000000000000ull);

	MTimeItemManager reg;
	CHECK(reg.AddTimeItem(1, 60));

	SetNow(0);

	// 5 000 000 060 = 57 870 * 86 400 + 8 * 3 600 + 54 * 60 + 20.
	CHECK_EQ(57870, reg.GetDay(1));
	CHECK_EQ(8, reg.GetHour(1));
	CHECK_EQ(54, reg.GetMinute(1));
	CHECK_EQ(20, reg.GetSecond(1));
	CHECK_EQ(false, reg.IsExpired(1));
}
