#pragma once

#include "CSpriteSurface.h"
#include "CSprite565.h"
#include "CSpritePal.h"
#include "CAlphaSpritePal.h"
#include "CFilter.h"
#include <vector>

// Shared synthetic scene for the completed-frame regression and benchmark.
// Includes cached terrain, moving sprites, palette effects, world light and UI.
struct MixedScene {
	CSpriteSurface frame, terrain;
	CSprite565 sprite;
	CSpritePal indexed;
	CAlphaSpritePal alpha;
	MPalette palette;
	CFilter lights;
	int widths[64]{}, heights[64]{};
	spritectl_sprite_t glyph = nullptr;
	bool Init()
	{
		if (!frame.Init(800, 600) || !terrain.Init(800, 600)) return false;
		std::vector<WORD> pixels(64 * 64);
		std::vector<BYTE> indices(pixels.size()), opacity(pixels.size());
		for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) {
			const int i = y * 64 + x;
			pixels[i] = WORD((x * 523 + y * 149) & 0xffff);
			indices[i] = BYTE(1 + (i % 254));
			opacity[i] = BYTE((x + y) % 33);
		}
		sprite.SetPixelNoColorkey(pixels.data(), 128, 64, 64);
		indexed.SetPixel(indices.data(), 64, 64, 64);
		alpha.SetPixel(indices.data(), 64, opacity.data(), 64, 64, 64);
		palette.Init(255);
		for (int i = 0; i < 255; ++i) palette[BYTE(i)] = WORD(i * 251);
		lights.Init(64, 64);
		for (int i = 0; i < 64; ++i) {
			widths[i] = 12 + i % 2;
			heights[i] = i % 8 == 0 || i % 8 == 3 || i % 8 == 6 ? 10 : 9;
			for (int x = 0; x < 64; ++x) lights.SetFilter(WORD(x), WORD(i), BYTE(12 + (x + i) % 24));
		}
		terrain.FillSurface(0x1943);
		for (int y = 0; y < 600; y += 64) for (int x = 0; x < 800; x += 64) {
			POINT point{x, y}; terrain.BltSprite(&point, &sprite);
		}
		uint32_t rgba[96];
		for (int i = 0; i < 96; ++i) rgba[i] = i % 3 ? 0xffe0c080 : 0;
		glyph = spritectl_create_sprite(8, 12, SPRITECTL_FORMAT_RGBA32, rgba, sizeof(rgba));
		CSpriteSurface::InitEffectTable();
		return glyph != nullptr;
	}
	~MixedScene() { spritectl_destroy_sprite(glyph); }
	void Draw(int number)
	{
		const auto oldEffect = CSpriteSurface::s_pMemcpyEffectFunction;
		const auto oldPalEffect = CSpriteSurface::s_pMemcpyPalEffectFunction;
		const int oldValue = CSpriteSurface::s_Value1;
		CSpriteSurface::s_pMemcpyEffectFunction = CSpriteSurface::memcpyEffectGrayScale;
		CSpriteSurface::s_pMemcpyPalEffectFunction = CSpriteSurface::memcpyPalEffectScreen;
		POINT origin{0, 0}; RECT full{0, 0, 800, 600};
		frame.Blt(&origin, &terrain, &full);
		frame.Lock();
		for (int i = 0; i < 160; ++i) {
			POINT p{(i * 53 + number * 3) % 736, (i * 31 + number) % 536};
			if (i < 112) frame.BltSprite(&p, &sprite);
			else if (i < 128) frame.BltSpritePalEffect(&p, &indexed, palette);
			else if (i < 144) frame.BltAlphaSpritePal(&p, &alpha, palette);
			else frame.BltSpriteDarkness(&p, &sprite, BYTE(i % 3));
		}
		frame.ApplyLightGrid(lights, widths, heights);
		RECT panel{10, 430, 450, 590}; frame.GammaBox565(&panel, 16);
		for (int i = 0; i < 10; ++i) {
			POINT p{20 + i * 70, 460}; RECT clip{0, 13, 64, 47};
			frame.BltSpriteColorClip(&p, &sprite, &clip, BYTE(i % 3));
			p.y += 60; frame.BltSpriteEffectClip(&p, &sprite, &clip);
		}
		for (int i = 0; i < 80; ++i) {
			frame.HLine(15 + i * 9, 15 + i % 7, 1, 0xffff);
			spritectl_blt_sprite(frame.GetBackendSurface(), 20 + i % 40 * 9, 440 + i / 40 * 16, glyph, 0, 255);
		}
		frame.Unlock();
		CSpriteSurface::s_pMemcpyEffectFunction = oldEffect;
		CSpriteSurface::s_pMemcpyPalEffectFunction = oldPalEffect;
		CSpriteSurface::s_Value1 = oldValue;
	}
};
