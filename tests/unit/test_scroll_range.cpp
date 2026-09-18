#include "test_framework.h"
#include "ScrollRange.h"
#include <limits>

TEST(ScrollRange, OrdinaryPositionsAndReverseButtons)
{
	Basic::ScrollRange range;
	range.SetPosMax(6);
	range.ScrollDown(3);
	CHECK_EQ(3, range.GetScrollPos());
	range.ScrollUp();
	CHECK_EQ(2, range.GetScrollPos());
	range.SetScrollPos(30);
	CHECK_EQ(5, range.GetScrollPos());
	range.SetReverse(true);
	range.ScrollDown();
	CHECK_EQ(4, range.GetScrollPos());
	range.ScrollUp();
	CHECK_EQ(5, range.GetScrollPos());
	range.SetPosMax(3);
	CHECK_EQ(0, range.GetScrollPos());
}

TEST(ScrollRange, OrdinaryPixelMapping)
{
	Basic::ScrollRange range;
	range.SetPosMax(6);
	range.SetPixelPosition(135, 100, 70, 10);
	CHECK_EQ(3, range.GetScrollPos());
	range.SetPixelPosition(0, 100, 70, 10);
	CHECK_EQ(0, range.GetScrollPos());
	range.SetPixelPosition(200, 100, 70, 10);
	CHECK_EQ(5, range.GetScrollPos());
}

TEST(ScrollRange, EmptyAndNegativeRangesNeverProduceNegativeIndices)
{
	for (const int maximum : {-20, -1, 0, 1, std::numeric_limits<int>::min()})
	{
		for (const bool reverse : {false, true})
		{
			Basic::ScrollRange range;
			range.SetPosMax(maximum);
			range.SetReverse(reverse);
			range.ScrollDown();
			CHECK_EQ(0, range.GetScrollPos());
			range.ScrollUp();
			CHECK_EQ(0, range.GetScrollPos());
			range.SetScrollPos(100);
			CHECK_EQ(0, range.GetScrollPos());
		}
	}
	Basic::ScrollRange unconfigured;
	unconfigured.ScrollDown();
	CHECK_EQ(0, unconfigured.GetScrollPos());
}

TEST(ScrollRange, ExtremeScrollDeltasSaturateAtBothEnds)
{
	Basic::ScrollRange range;
	range.SetPosMax(std::numeric_limits<int>::max());
	range.SetScrollPos(std::numeric_limits<int>::max() - 2);
	range.ScrollDown(std::numeric_limits<int>::max());
	CHECK_EQ(std::numeric_limits<int>::max() - 1, range.GetScrollPos());
	range.ScrollUp(std::numeric_limits<int>::min());
	CHECK_EQ(std::numeric_limits<int>::max() - 1, range.GetScrollPos());
	range.ScrollDown(std::numeric_limits<int>::min());
	CHECK_EQ(0, range.GetScrollPos());
	range.SetReverse(true);
	range.ScrollDown(std::numeric_limits<int>::min());
	CHECK_EQ(std::numeric_limits<int>::max() - 1, range.GetScrollPos());
	range.ScrollUp(std::numeric_limits<int>::min());
	CHECK_EQ(0, range.GetScrollPos());
}

TEST(ScrollRange, ReverseDragAgreesWithReverseButtons)
{
	Basic::ScrollRange range;
	range.SetPosMax(6);
	range.SetReverse(true);
	range.SetPixelPosition(105, 100, 70, 10);
	CHECK_EQ(5, range.GetScrollPos());
	range.SetPixelPosition(165, 100, 70, 10);
	CHECK_EQ(0, range.GetScrollPos());
}

TEST(ScrollRange, AThumbFillingOrExceedingTheBarCannotBeDragged)
{
	Basic::ScrollRange range;
	range.SetPosMax(6);
	for (const int extent : {10, 9, 0, -1})
	{
		range.SetScrollPos(3);
		range.SetPixelPosition(105, 100, extent, 10);
		CHECK_EQ(0, range.GetScrollPos());
	}
}

TEST(ScrollRange, ContainerCountsAreBoundedBeforeSubtractionOrNarrowing)
{
	Basic::ScrollRange range;
	for (size_t count = 0; count <= 14; ++count)
	{
		range.SetItemCount(count, 13);
		range.ScrollDown(100);
		CHECK_EQ(count <= 13 ? 0 : 1, range.GetScrollPos());
	}
	range.SetItemCount((std::numeric_limits<size_t>::max)(), 1);
	range.ScrollDown((std::numeric_limits<int>::max)());
	CHECK_EQ((std::numeric_limits<int>::max)() - 1, range.GetScrollPos());
	range.SetPositionCount((std::numeric_limits<size_t>::max)());
	range.ScrollDown((std::numeric_limits<int>::max)());
	CHECK_EQ((std::numeric_limits<int>::max)() - 1, range.GetScrollPos());
	range.SetItemCount(20, 0);
	range.ScrollDown();
	CHECK_EQ(0, range.GetScrollPos());
}

TEST(ScrollRange, ExtremePixelCoordinatesDoNotOverflow)
{
	Basic::ScrollRange range;
	range.SetPosMax((std::numeric_limits<int>::max)());
	range.SetPixelPosition((std::numeric_limits<int>::max)(),
		(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)(), 0);
	CHECK_EQ((std::numeric_limits<int>::max)() - 1, range.GetScrollPos());
	range.SetPixelPosition((std::numeric_limits<int>::min)(),
		(std::numeric_limits<int>::max)(), (std::numeric_limits<int>::max)(), 0);
	CHECK_EQ(0, range.GetScrollPos());
	range.SetPixelPosition(100, 0, 30, -1);
	CHECK_EQ(0, range.GetScrollPos());
}

TEST(ScrollRange, ThumbDrawingAndDraggingSharePositionWithoutMutatingIt)
{
	Basic::ScrollRange range;
	range.SetPosMax(6);
	for (const bool reverse : {false, true})
	{
		range.SetReverse(reverse);
		for (int pos = 0; pos < 6; ++pos)
		{
			range.SetScrollPos(pos);
			const int offset = range.GetThumbOffset(70, 10);
			CHECK_EQ(reverse ? (5 - pos) * 12 : pos * 12, offset);
			CHECK_EQ(pos, range.GetScrollPos());
			range.SetPixelPosition(100 + 5 + offset, 100, 70, 10);
			CHECK_EQ(pos, range.GetScrollPos());
		}
	}
	range.SetReverse(false);
	range.SetPosMax((std::numeric_limits<int>::max)());
	range.SetScrollPos((std::numeric_limits<int>::max)());
	CHECK_EQ((std::numeric_limits<int>::max)(),
		range.GetThumbOffset((std::numeric_limits<int>::max)(), 0));
	CHECK_EQ(0, range.GetThumbOffset(10, 10));
	CHECK_EQ(0, range.GetThumbOffset(5, 10));
}
