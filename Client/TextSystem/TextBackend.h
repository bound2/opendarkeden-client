#ifndef TEXTSYSTEM_TEXTBACKEND_H
#define TEXTSYSTEM_TEXTBACKEND_H

#include <cstddef>
#include "TextTypes.h"
#include "RenderTarget.h"

namespace TextSystem {

struct Glyph {
	GlyphMetrics metrics;
	void* handle;
};

struct GlyphCacheLimits {
	size_t entries = 4096;
	size_t pixelBytes = 8 * 1024 * 1024;
};

struct GlyphCacheStats {
	size_t entries = 0;
	size_t pixelBytes = 0;
	// Successful cache fills, including recreations after eviction.
	size_t rasterizations = 0;
};

class TextBackend {
public:
	virtual ~TextBackend() {}
	virtual bool Initialize() = 0;
	virtual FontHandle AcquireFont(const FontDesc& desc) = 0;
	virtual int GetLineHeight(FontHandle font) const = 0;
	virtual int GetFontAscent(FontHandle font) const = 0;
	virtual bool GetGlyphMetrics(FontHandle font, uint32_t codepoint, GlyphMetrics& outMetrics) = 0;
	// The returned glyph is borrowed until a subsequent cache miss or destruction.
	virtual const Glyph* GetGlyph(FontHandle font, uint32_t codepoint, const Color& color) = 0;
	virtual void DrawGlyph(RenderTarget& target, const Glyph& glyph, int x, int y, uint8_t alpha) = 0;
	virtual GlyphCacheStats GetGlyphCacheStats() const = 0;
};

// A glyph larger than the pixel budget is rejected. Limits also bound the
// metadata for tiny glyphs; a hit keeps its sprite until it is least recently used.
TextBackend* CreateSDLTextBackend(GlyphCacheLimits limits = {});

} // namespace TextSystem

#endif // TEXTSYSTEM_TEXTBACKEND_H
