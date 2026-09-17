#include "test_framework.h"
#include "CSprite565.h"

namespace {
class InterruptedSprite : public CSprite565 {
public:
	void BeforeRowAllocation()
	{
		m_Width = 4;
		m_Height = 3;
		m_Pixels = nullptr;
	}
	void AfterFirstRowAllocation()
	{
		BeforeRowAllocation();
		m_Pixels = new WORD*[m_Height]{};
		m_Pixels[0] = new WORD[1]{0};
	}
};
}

TEST(SpriteRelease, FailedRowArrayAllocationLeavesAnEmptySprite)
{
	// Reproduce the member state after a loader has read the dimensions but
	// its row-pointer allocation has thrown. No allocation-failure injection.
	InterruptedSprite sprite;
	sprite.BeforeRowAllocation();
	sprite.Release();
	CHECK_EQ(0, sprite.GetWidth());
	CHECK_EQ(0, sprite.GetHeight());
	CHECK(!sprite.IsInit());
	sprite.Release();
}

TEST(SpriteRelease, PartialRowAllocationCanBeReleasedRepeatedly)
{
	InterruptedSprite sprite;
	sprite.AfterFirstRowAllocation();
	sprite.Release();
	CHECK_EQ(0, sprite.GetWidth());
	CHECK_EQ(0, sprite.GetHeight());
	CHECK(!sprite.IsInit());
	sprite.Release();
}
