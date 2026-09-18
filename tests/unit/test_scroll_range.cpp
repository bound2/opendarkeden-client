#include "test_framework.h"
#include "ScrollRange.h"

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
