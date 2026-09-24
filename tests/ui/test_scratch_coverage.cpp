#include "test_framework.h"
#include "ScratchCoverage.h"
#include <climits>

TEST(ScratchCoverage, SeededPixelsClippedStrokesAndOverlapsPreserveProgress)
{
	ScratchCoverage cover;
	CHECK_EQ(0, cover.Percent());
	const uint16_t pixels[3][5]{{0xffff, 0, 0, 0, 0xffff}, {0, 0xffff, 0, 0, 0xffff}, {0, 0, 0, 0, 0xffff}};
	cover.Reset(4, 3, pixels, sizeof(pixels[0])); // Last column is padding.
	CHECK_EQ(16, cover.Percent());
	cover.Erase(-1, -1, 3, 3);
	CHECK_EQ(33, cover.Percent());
	cover.Erase(-1, -1, 3, 3);
	CHECK_EQ(33, cover.Percent());
	cover.Erase(INT_MAX, INT_MAX, INT_MAX, INT_MAX);
	CHECK_EQ(33, cover.Percent());
	cover.Erase(2, 1, 10, 10);
	CHECK_EQ(66, cover.Percent());
	cover.Erase(0, 0, 4, 3);
	CHECK_EQ(100, cover.Percent());
	cover.Reset(1, 1, nullptr, 0);
	CHECK_EQ(0, cover.Percent());
	cover.Erase(0, 0, 1, 1);
	CHECK_EQ(100, cover.Percent());
}
