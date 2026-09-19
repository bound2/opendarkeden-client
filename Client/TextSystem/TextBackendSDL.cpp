#include "TextBackend.h"

#include <unordered_map>
#include <list>
#include <memory>
#include <utility>
#include <vector>
#include <string>
#include <stdint.h>
#include <stdio.h>

#include "SpriteLib/SpriteLibBackend.h"

#ifdef USE_SDL_BACKEND
#include <SDL.h>
#include <SDL_ttf.h>
#endif

namespace TextSystem {

struct GlyphKey {
	int fontId;
	uint32_t codepoint;
	uint32_t color;

	bool operator==(const GlyphKey& other) const
	{
		return fontId == other.fontId && codepoint == other.codepoint && color == other.color;
	}
};

struct GlyphKeyHash {
	size_t operator()(const GlyphKey& key) const
	{
		return (static_cast<size_t>(key.fontId) * 1315423911u) ^
			(static_cast<size_t>(key.codepoint) * 2654435761u) ^
			(static_cast<size_t>(key.color) * 97531u);
	}
};

using SpriteOwner = std::unique_ptr<spritectl_sprite_s, decltype(&spritectl_destroy_sprite)>;

struct CachedGlyph {
	Glyph glyph;
	SpriteOwner sprite;
	size_t pixelBytes;
	std::list<GlyphKey>::iterator recent;

	CachedGlyph(GlyphMetrics metrics, SpriteOwner owner, size_t bytes, std::list<GlyphKey>::iterator position)
		: glyph{metrics, owner.get()}, sprite(std::move(owner)), pixelBytes(bytes), recent(position) {}
};

class TextBackendSDL : public TextBackend {
public:
	explicit TextBackendSDL(GlyphCacheLimits limits)
		: m_initialized(false)
		, m_limits(limits)
	{}

	~TextBackendSDL() override
	{
		m_glyphs.clear();

		for (size_t i = 0; i < m_fonts.size(); ++i) {
			if (m_fonts[i]) {
				TTF_CloseFont(m_fonts[i]);
			}
		}
		m_fonts.clear();
	}

	bool Initialize() override
	{
#ifdef USE_SDL_BACKEND
		if (TTF_WasInit() == 0) {
			if (TTF_Init() != 0) {
				fprintf(stderr, "TextBackendSDL: TTF_Init failed: %s\n", TTF_GetError());
				return false;
			}
		}
#endif
		m_initialized = true;
		return true;
	}

	FontHandle AcquireFont(const FontDesc& desc) override
	{
		if (!m_initialized)
			return FontHandle();

		int size = desc.size > 0 ? desc.size : 16;
		auto it = m_sizeToFontId.find(size);
		if (it != m_sizeToFontId.end()) {
			FontHandle handle;
			handle.id = it->second;
			return handle;
		}

		const char* fontPaths[] = {
			"Data/Font/NotoSansCJK-Regular.ttc",
			"Data/Font/NotoSans-Regular.ttf",
			"Data/Font/DejaVuSans.ttf",
			"Data/Font/Hiragino Sans GB.ttc",
			// None of the Data/Font paths above ship with the game data (no
			// font is part of it - see SPRITELIB_BACKEND_README), so the
			// system fonts below are what actually loads. Before they were
			// listed, every AcquireFont() call on Windows walked the Data/Font
			// paths, failed, left TextService::m_initialized false for good,
			// and EnsureInitialized() (called at the top of DrawLine,
			// MeasureText and the rest) turned every text call into a silent
			// no-op: no game text was drawn anywhere, not just in this dialog.
#if defined(_WIN32)
			// Every Windows installation has these. Malgun Gothic covers
			// Hangul, Chinese and Latin together (the client mixes Korean
			// development strings with the Chinese game string tables),
			// Microsoft YaHei specialises in Simplified Chinese, and Arial
			// is the Latin-only last resort.
			"C:\\Windows\\Fonts\\malgun.ttf",
			"C:\\Windows\\Fonts\\msyh.ttc",
			"C:\\Windows\\Fonts\\simsun.ttc",
			"C:\\Windows\\Fonts\\arial.ttf",
#elif defined(PLATFORM_MACOS)
			// Every macOS since 10.8 ships Apple SD Gothic Neo (Hangul and
			// Latin - the client's development strings are Korean), then
			// the Chinese game tables' coverage: Arial Unicode under
			// Supplemental (10.15 and later), Hiragino Sans GB, and
			// PingFang where it is still a file (10.11 to 10.14; later
			// releases keep it in a font asset catalog SDL_ttf cannot
			// open). Helvetica is the Latin-only last resort; it and the
			// first entry are the two a CI runner is certain to have.
			"/System/Library/Fonts/AppleSDGothicNeo.ttc",
			"/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
			"/System/Library/Fonts/Hiragino Sans GB.ttc",
			"/System/Library/Fonts/PingFang.ttc",
			"/System/Library/Fonts/Helvetica.ttc",
#elif defined(PLATFORM_ANDROID)
			// Android: Noto Sans CJK has been the system CJK font since
			// 5.0, though vendor images split it per region or keep the
			// older DroidSansFallback instead; Roboto is on every image
			// and is the Latin-only last resort. Nothing outside
			// /system/fonts is readable from an app.
			"/system/fonts/NotoSansCJK-Regular.ttc",
			"/system/fonts/NotoSansSC-Regular.otf",
			"/system/fonts/NotoSansKR-Regular.otf",
			"/system/fonts/DroidSansFallback.ttf",
			"/system/fonts/Roboto-Regular.ttf",
#elif defined(PLATFORM_IOS)
			// iOS keeps its system fonts in an asset catalog SDL_ttf
			// cannot open, so only the Data/Font entries above can load
			// there: the data tree has to ship a font.
#else
			// Linux: the Noto CJK package where Debian, Ubuntu and Fedora put
			// it, then DejaVu, which nearly every distribution installs and
			// which covers Latin, so the tests and the title screen have a
			// font even where CJK does not. Nothing under /usr/share/fonts is
			// guaranteed; a machine with none of these draws no text, loudly
			// (the "Failed to load font" line below).
			"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
			"/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
			"/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
			"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
			"/usr/share/fonts/dejavu/DejaVuSans.ttf",
			"/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
			NULL
		};

		TTF_Font* font = NULL;
		for (int i = 0; fontPaths[i] != NULL; ++i) {
			font = TTF_OpenFont(fontPaths[i], size);
			if (font) {
				break;
			}
		}

		if (!font) {
			fprintf(stderr, "TextBackendSDL: Failed to load font size %d\n", size);
			return FontHandle();
		}

		int id = static_cast<int>(m_fonts.size());
		m_fonts.push_back(font);
		m_sizeToFontId[size] = id;

		FontHandle handle;
		handle.id = id;
		return handle;
	}

	int GetLineHeight(FontHandle font) const override
	{
		TTF_Font* ttf = GetFont(font);
		if (!ttf)
			return 16;
		return TTF_FontHeight(ttf);
	}

	int GetFontAscent(FontHandle font) const override
	{
		TTF_Font* ttf = GetFont(font);
		if (!ttf)
			return 12;
		return TTF_FontAscent(ttf);
	}

	bool GetGlyphMetrics(FontHandle font, uint32_t codepoint, GlyphMetrics& outMetrics) override
	{
		TTF_Font* ttf = GetFont(font);
		if (!ttf)
			return false;

		int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
		bool ok = false;

		if (codepoint <= 0xFFFF) {
			if (TTF_GlyphMetrics(ttf, static_cast<Uint16>(codepoint), &minx, &maxx, &miny, &maxy, &advance) == 0) {
				ok = true;
			}
		}

		if (!ok) {
			// Fallback: approximate metrics using rendered surface
			std::string utf8 = EncodeUtf8(codepoint);
			SDL_Color white = {255, 255, 255, 255};
			SDL_Surface* surf = TTF_RenderUTF8_Blended(ttf, utf8.c_str(), white);
			if (!surf)
				return false;
			minx = 0;
			maxx = surf->w;
			miny = -TTF_FontAscent(ttf);  // Assume top-aligned
			maxy = TTF_FontDescent(ttf);  // Assume baseline at bottom
			advance = surf->w;
			SDL_FreeSurface(surf);
		}

		outMetrics.width = maxx - minx;
		outMetrics.height = maxy - miny;
		outMetrics.advance = advance;
		outMetrics.bearingX = minx;
		// bearingY is the distance from baseline to top of glyph
		// miny is usually negative (distance above baseline)
		// ascent is the distance from baseline to top of font bounding box
		outMetrics.bearingY = TTF_FontAscent(ttf) + miny;
		return true;
	}

	const Glyph* GetGlyph(FontHandle font, uint32_t codepoint, const Color& color) override
	{
		TTF_Font* ttf = GetFont(font);
		if (!ttf)
			return NULL;

		uint32_t packedColor = (static_cast<uint32_t>(color.r) << 16) |
			(static_cast<uint32_t>(color.g) << 8) |
			static_cast<uint32_t>(color.b);

		GlyphKey key;
		key.fontId = font.id;
		key.codepoint = codepoint;
		key.color = packedColor;

		auto it = m_glyphs.find(key);
		if (it != m_glyphs.end()) {
			m_recent.splice(m_recent.begin(), m_recent, it->second.recent);
			return &it->second.glyph;
		}

		// Get glyph metrics first
		int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
		bool hasMetrics = false;

		if (codepoint <= 0xFFFF) {
			if (TTF_GlyphMetrics(ttf, static_cast<Uint16>(codepoint), &minx, &maxx, &miny, &maxy, &advance) == 0) {
				hasMetrics = true;
			}
		}

		// Render the glyph
		std::string utf8 = EncodeUtf8(codepoint);
		// Coverage lives in the sprite; text opacity is applied at draw time.
		SDL_Color sdlColor = {color.r, color.g, color.b, 255};
		std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(
			TTF_RenderUTF8_Blended(ttf, utf8.c_str(), sdlColor), SDL_FreeSurface);
		if (!surface)
			return NULL;

		if (surface->format->format != SDL_PIXELFORMAT_RGBA32) {
			surface.reset(SDL_ConvertSurfaceFormat(surface.get(), SDL_PIXELFORMAT_RGBA32, 0));
			if (!surface)
				return NULL;
		}

		GlyphMetrics metrics;

		if (hasMetrics) {
			int ascent = TTF_FontAscent(ttf);
			metrics.width = maxx - minx;
			metrics.height = maxy - miny;
			metrics.advance = advance;
			metrics.bearingX = minx;
			// bearingY is distance from baseline to TOP of rendered glyph surface
			// The rendered surface includes the full glyph, so its top is at baseline - miny
			// Therefore bearingY should be: ascent - (surface_top_relative_to_baseline)
			// Since miny is negative (above baseline), surface_top = baseline - miny = baseline + |miny|
			// So bearingY = ascent + miny
			metrics.bearingY = ascent + miny;
		} else {
			// Fallback: approximate metrics from surface
			int ascent = TTF_FontAscent(ttf);
			metrics.width = surface->w;
			metrics.height = surface->h;
			metrics.advance = surface->w;
			metrics.bearingX = 0;
			metrics.bearingY = ascent;  // Assume top-aligned
		}
		const size_t dataSize = static_cast<size_t>(surface->pitch) * surface->h;
		if (m_limits.entries == 0 || dataSize > m_limits.pixelBytes)
			return NULL;
		SpriteOwner sprite(spritectl_create_sprite(
			surface->w,
			surface->h,
			SPRITECTL_FORMAT_RGBA32,
			surface->pixels,
			dataSize), spritectl_destroy_sprite);
		if (!sprite)
			return NULL;

		while (m_glyphs.size() >= m_limits.entries || dataSize > m_limits.pixelBytes - m_pixelBytes) {
			const auto cold = m_glyphs.find(m_recent.back());
			m_pixelBytes -= cold->second.pixelBytes;
			m_glyphs.erase(cold); // Owning sprite is released here.
			m_recent.pop_back();
		}
		m_recent.push_front(key);
		try {
			it = m_glyphs.try_emplace(key, metrics, std::move(sprite), dataSize, m_recent.begin()).first;
		} catch (...) {
			m_recent.pop_front();
			throw;
		}
		m_pixelBytes += dataSize;
		++m_rasterizations;
		return &it->second.glyph;
	}

	GlyphCacheStats GetGlyphCacheStats() const override
	{
		return {m_glyphs.size(), m_pixelBytes, m_rasterizations};
	}

	void DrawGlyph(RenderTarget& target, const Glyph& glyph, int x, int y, uint8_t alpha) override
	{
		void* native = target.GetNative(NativeTargetType::SpriteCtlSurface);
		if (!native || !glyph.handle)
			return;

		spritectl_surface_t surface = reinterpret_cast<spritectl_surface_t>(native);
		spritectl_sprite_t sprite = reinterpret_cast<spritectl_sprite_t>(glyph.handle);

		spritectl_blt_sprite(surface, x, y, sprite, SPRITECTL_BLT_ALPHA, alpha);
	}

private:
	TTF_Font* GetFont(FontHandle handle) const
	{
		if (handle.id < 0 || handle.id >= static_cast<int>(m_fonts.size()))
			return NULL;
		return m_fonts[handle.id];
	}

	std::string EncodeUtf8(uint32_t codepoint) const
	{
		char buf[5] = {0};
		if (codepoint < 0x80) {
			buf[0] = static_cast<char>(codepoint);
			buf[1] = 0;
			return std::string(buf);
		}
		if (codepoint < 0x800) {
			buf[0] = static_cast<char>(0xC0 | (codepoint >> 6));
			buf[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
			buf[2] = 0;
			return std::string(buf);
		}
		if (codepoint < 0x10000) {
			buf[0] = static_cast<char>(0xE0 | (codepoint >> 12));
			buf[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
			buf[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
			buf[3] = 0;
			return std::string(buf);
		}
		buf[0] = static_cast<char>(0xF0 | (codepoint >> 18));
		buf[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
		buf[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		buf[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
		buf[4] = 0;
		return std::string(buf);
	}

private:
	bool m_initialized;
	GlyphCacheLimits m_limits;
	size_t m_pixelBytes = 0;
	size_t m_rasterizations = 0;
	std::vector<TTF_Font*> m_fonts;
	std::unordered_map<int, int> m_sizeToFontId;
	std::list<GlyphKey> m_recent;
	std::unordered_map<GlyphKey, CachedGlyph, GlyphKeyHash> m_glyphs;
};

TextBackend* CreateSDLTextBackend(GlyphCacheLimits limits)
{
	return new TextBackendSDL(limits);
}

} // namespace TextSystem
