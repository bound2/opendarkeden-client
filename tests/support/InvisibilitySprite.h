#pragma once

#include "test_framework.h"
#include "CIndexSprite565.h"
#include "CSpriteSurface.h"
#include "SpriteGpu.h"
#include <climits>

namespace invisibility_test {
class Sprite : public CIndexSprite565 {
public:
	static FUNCTION_MEMCPYEFFECT Effect() { return s_pMemcpyEffectFunction; }
	static void RestoreEffect(FUNCTION_MEMCPYEFFECT effect) { s_pMemcpyEffectFunction = effect; }
	Sprite()
	{
		// Two rows: a hole, five recolorable pixels, five fixed pixels
		// (including opaque black), another hole and a short fixed run.
		m_Width = 15;
		m_Height = 2;
		m_Pixels = new WORD*[2];
		for (int y = 0; y < 2; ++y)
			m_Pixels[y] = new WORD[19]{2, 1, 5, 10, 10, 10, 10, 10,
				5, 0xf800, 0x07e0, 0, 0x001f, 0xffff, 1, 0, 2, 0xf81f, 0xffe0};
		m_bInit = true;
	}
};

inline void CheckFade()
{
	const auto previousEffect = Sprite::Effect();
	const int previousValue = CSpriteSurface::s_Value1;
	const int previousColor = CIndexSprite::GetUsingColorSet(0);
	CIndexSprite::SetColorSet();
	CIndexSprite::SetEffect(CIndexSprite::EFFECT_WIPE_OUT);
	Sprite sprite;
	CSpriteSurface surface;
	CHECK(surface.Init(17, 5));
	const WORD fixed[] = {0xf800, 0x07e0, 0, 0x001f, 0xffff};
	// Both palette choices reuse the sprite, just like nearby vampires.
	for (int palette : {30, 90}) {
		CIndexSprite::SetUsingColorSetOnly(0, palette);
		for (int value : {-1, 0, 1, 16, 32, 63, 64, 65}) {
			CSpriteSurface::s_Value1 = value;
			for (POINT point : {POINT{1, 1}, POINT{-3, -1}, POINT{10, 4},
				POINT{INT_MIN, 0}, POINT{INT_MAX, 0}}) {
				surface.SetClipNULL();
				surface.FillSurface(0x1234);
				RECT clip{1, 1, 16, 5};
				surface.SetClip(&clip);
				const auto before = SpriteGpu::GetCounters();
				surface.BltIndexSpriteEffect(&point, &sprite);
				const auto after = SpriteGpu::GetCounters();
				CHECK_EQ(before.readbacks, after.readbacks);
				CHECK_EQ(before.textureBytes, after.textureBytes);
				if (value >= 64) {
					CHECK_EQ(before.draws, after.draws);
					CHECK_EQ(before.spriteUploads, after.spriteUploads);
				}
				DWORD pitch = 0;
				const auto* pixels = static_cast<const BYTE*>(surface.Lock(nullptr, &pitch));
				CHECK(pixels != nullptr);
				if (pixels) {
					for (int y = 0; y < 5; ++y) for (int x = 0; x < 17; ++x) {
						const long long sx = static_cast<long long>(x) - point.x;
						const long long sy = static_cast<long long>(y) - point.y;
						WORD expected = 0x1234;
						if (x >= 1 && x < 16 && y >= 1 && sy >= 0 && sy < 2 && value < 64) {
							// Independent expected masks for each five-pixel run.
							const bool endsOnly = value == 32;
							const bool lastOnly = value == 63;
							for (int start : {1, 6}) {
								const long long offset = sx - start;
								const bool kept = offset >= 0 && offset < 5
									&& (lastOnly ? offset == 4 : endsOnly ? offset != 1 && offset != 2
									: value == 16 ? offset != 2 : true);
								if (kept) expected = start == 1 ? CIndexSprite::ColorSet[palette][10] : fixed[offset];
							}
							if (sx == 12 && value < 32) expected = 0xf81f;
							if (sx == 13) expected = 0xffe0;
						}
						CHECK_EQ(expected, reinterpret_cast<const WORD*>(pixels + y * pitch)[x]);
					}
					surface.Unlock();
				}
			}
		}
	}
	Sprite::RestoreEffect(previousEffect);
	CSpriteSurface::s_Value1 = previousValue;
	CIndexSprite::SetUsingColorSetOnly(0, previousColor);
}
}
