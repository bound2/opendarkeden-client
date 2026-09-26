#include "InvisibilitySprite.h"

TEST(InvisibilitySprite, WipePreservesBackgroundClippingAndPalette)
{
	CHECK_EQ(0, spritectl_init());
	invisibility_test::CheckFade();
}
