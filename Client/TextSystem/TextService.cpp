#include "TextService.h"

#include <algorithm>
#include <cstring>

#ifdef USE_SDL_BACKEND
#include <SDL.h>
#endif

#ifdef HAVE_REAL_ICONV
#include <iconv.h>
#include <vector>
#endif

namespace TextSystem {

// Forward declaration of SDL backend factory
TextBackend* CreateSDLTextBackend();

static bool IsValidUtf8(const char* data, size_t len)
{
	size_t i = 0;
	while (i < len) {
		unsigned char c = static_cast<unsigned char>(data[i]);
		if (c < 0x80) {
			++i;
			continue;
		}

		int needed = 0;
		if ((c & 0xE0) == 0xC0) {
			if (c < 0xC2)
				return false;
			needed = 1;
		} else if ((c & 0xF0) == 0xE0) {
			needed = 2;
		} else if ((c & 0xF8) == 0xF0) {
			if (c > 0xF4)
				return false;
			needed = 3;
		} else {
			return false;
		}

		if (i + needed >= len)
			return false;
		for (int j = 1; j <= needed; ++j) {
			if ((static_cast<unsigned char>(data[i + j]) & 0xC0) != 0x80)
				return false;
		}
		i += static_cast<size_t>(needed) + 1;
	}
	return true;
}

// Transcodes input from fromEncoding to UTF-8, returning an empty string when
// that code page is unavailable or the bytes are not valid in it. An empty
// result is how NormalizeText below tells a wrong guess from a right one, so
// it must mean "this code page does not explain these bytes" and nothing else.
//
// This has to go through a real iconv rather than SDL_iconv. SDL falls back to
// its own converter whenever it was built without HAVE_ICONV - which is how
// vcpkg installs SDL2 - and that built-in converter only knows UTF-8, UTF-16,
// UCS-2/4, Latin-1 and ASCII. Every legacy CJK code page asked for here fails
// at open, so routing through SDL_iconv quietly made this whole function a
// no-op on Windows: it returned its input unchanged for exactly the CP949 data
// it exists to convert.
static std::string ConvertEncoding(const std::string& input, const char* fromEncoding)
{
	if (input.empty())
		return input;

#ifdef HAVE_REAL_ICONV
	iconv_t cd = iconv_open("UTF-8", fromEncoding);
	if (cd == reinterpret_cast<iconv_t>(-1))
		return std::string();

	size_t inBytes  = input.size();
	size_t outBytes = input.size() * 4 + 4;

	std::vector<char> scratch(outBytes);
	char* inBuf  = const_cast<char*>(input.data());
	char* outBuf = &scratch[0];

	const size_t res = iconv(cd, &inBuf, &inBytes, &outBuf, &outBytes);
	iconv_close(cd);

	// inBytes != 0 means iconv stopped early on a byte it could not map.
	// Treating that as failure is what stops the first code page in the list
	// from claiming input that belongs to a later one.
	if (res == static_cast<size_t>(-1) || inBytes != 0)
		return std::string();

	return std::string(&scratch[0], scratch.size() - outBytes);
#else
	(void)fromEncoding;
	return std::string();
#endif
}

// Public static method for encoding normalization
std::string TextService::NormalizeText(const std::string& text)
{
	if (text.empty())
		return text;

	if (IsValidUtf8(text.c_str(), text.size()))
		return text;

	// Try common encodings: Korean first, then Chinese, then fallback
	const char* encodings[] = {"CP949", "EUC-KR", "GBK", "GB2312", "BIG5", NULL};
	for (int i = 0; encodings[i] != NULL; ++i) {
		std::string converted = ConvertEncoding(text, encodings[i]);
		if (!converted.empty())
			return converted;
	}

	return text;
}

static uint32_t Utf8Decode(const char* s, int maxLen, int* outLen)
{
	if (maxLen < 1) {
		*outLen = 0;
		return 0xFFFD; // Replacement character for empty input
	}

	unsigned char c = static_cast<unsigned char>(*s);

	// ASCII (0x00-0x7F)
	if (c < 0x80) {
		*outLen = 1;
		return c;
	}

	// 2-byte sequence (0xC0-0xDF)
	if ((c >> 5) == 0x6) {
		if (maxLen < 2) {
			*outLen = 1;
			return 0xFFFD; // Truncated sequence
		}
		*outLen = 2;
		return ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[1]) & 0x3F);
	}

	// 3-byte sequence (0xE0-0xEF)
	if ((c >> 4) == 0xE) {
		if (maxLen < 3) {
			*outLen = 1;
			return 0xFFFD; // Truncated sequence
		}
		*outLen = 3;
		return ((c & 0x0F) << 12) |
			((static_cast<unsigned char>(s[1]) & 0x3F) << 6) |
			(static_cast<unsigned char>(s[2]) & 0x3F);
	}

	// 4-byte sequence (0xF0-0xF7)
	if ((c >> 3) == 0x1E) {
		if (maxLen < 4) {
			*outLen = 1;
			return 0xFFFD; // Truncated sequence
		}
		*outLen = 4;
		return ((c & 0x07) << 18) |
			((static_cast<unsigned char>(s[1]) & 0x3F) << 12) |
			((static_cast<unsigned char>(s[2]) & 0x3F) << 6) |
			(static_cast<unsigned char>(s[3]) & 0x3F);
	}

	// Invalid UTF-8 lead byte
	*outLen = 1;
	return 0xFFFD;
}

TextService::TextService()
	: m_backend(NULL)
	, m_initialized(false)
	, m_defaultFont()
{
}

TextService::~TextService()
{
	delete m_backend;
	m_backend = NULL;
}

TextService& TextService::Get()
{
	static TextService s_instance;
	return s_instance;
}

bool TextService::EnsureInitialized()
{
	if (m_initialized)
		return true;

	m_backend = CreateSDLTextBackend();
	if (!m_backend)
		return false;

	if (!m_backend->Initialize())
		return false;

	FontDesc defaultDesc;
	defaultDesc.size = 16;
	m_defaultFont = m_backend->AcquireFont(defaultDesc);
	m_initialized = m_defaultFont.IsValid();
	return m_initialized;
}

FontHandle TextService::GetDefaultFont()
{
	EnsureInitialized();
	return m_defaultFont;
}

FontHandle TextService::GetFont(int size)
{
	if (!EnsureInitialized())
		return FontHandle();

	FontDesc desc;
	desc.size = size;
	FontHandle handle = m_backend->AcquireFont(desc);
	if (!handle.IsValid())
		return m_defaultFont;
	return handle;
}

TextStyle TextService::GetDefaultStyle()
{
	TextStyle style;
	style.font = GetDefaultFont();
	style.align = TextAlign::Left;
	style.lineSpacing = 0;
	style.color = ColorFromRGB(0xFFFFFF);
	return style;
}

int TextService::GetLineHeight(const TextStyle& style) const
{
	if (!m_initialized || !m_backend)
		return 16;
	return m_backend->GetLineHeight(style.font);
}

int TextService::MeasureLineWidth(const std::string& text, FontHandle font)
{
	if (!EnsureInitialized())
		return 0;

	int width = 0;
	const char* p = text.c_str();
	int remaining = static_cast<int>(text.size());
	while (*p && remaining > 0) {
		int len = 0;
		uint32_t codepoint = Utf8Decode(p, remaining, &len);
		if (len == 0) break; // Safety: no data left
		p += len;
		remaining -= len;

		if (codepoint == '\n')
			break;

		GlyphMetrics metrics;
		if (m_backend->GetGlyphMetrics(font, codepoint, metrics)) {
			width += metrics.advance;
		}
	}
	return width;
}

Metrics TextService::MeasureText(const std::string& text, const TextStyle& style, int maxWidth)
{
	Metrics out = {0, 0, GetLineHeight(style)};
	std::vector<std::string> lines = WrapText(text, style, maxWidth);

	int maxLineWidth = 0;
	for (size_t i = 0; i < lines.size(); ++i) {
		int w = MeasureLineWidth(lines[i], style.font);
		if (w > maxLineWidth)
			maxLineWidth = w;
	}

	out.width = maxLineWidth;
	if (!lines.empty()) {
		out.height = static_cast<int>(lines.size()) * (out.lineHeight + style.lineSpacing) - style.lineSpacing;
	}
	return out;
}

std::vector<std::string> TextService::WrapText(const std::string& text, const TextStyle& style, int maxWidth)
{
	std::string normalized = NormalizeText(text);

	std::vector<std::string> lines;
	if (!EnsureInitialized()) {
		lines.push_back(normalized);
		return lines;
	}

	std::string line;
	int lineWidth = 0;
	int lastBreakIndex = -1;
	int lastBreakSkip = 0;

	const char* p = normalized.c_str();
	int remaining = static_cast<int>(normalized.size());
	while (*p && remaining > 0) {
		int len = 0;
		uint32_t codepoint = Utf8Decode(p, remaining, &len);
		if (len == 0) break; // Safety: no data left

		if (codepoint == '\n') {
			lines.push_back(line);
			line.clear();
			lineWidth = 0;
			lastBreakIndex = -1;
			lastBreakSkip = 0;
			p += len;
			remaining -= len;
			continue;
		}

		GlyphMetrics metrics;
		if (!m_backend->GetGlyphMetrics(style.font, codepoint, metrics)) {
			p += len;
			remaining -= len;
			continue;
		}

		if (codepoint == ' ') {
			lastBreakIndex = static_cast<int>(line.size());
			lastBreakSkip = len;
		}

		if (maxWidth > 0 && lineWidth + metrics.advance > maxWidth && !line.empty()) {
			// lastBreakIndex may have just been set to line.size() on this very
			// iteration (codepoint == ' ' above, before the space is appended
			// below), so lastBreakIndex + lastBreakSkip can exceed line.size()
			// here - substr() would throw std::out_of_range in that case, so
			// treat it the same as "no break point recorded".
			if (lastBreakIndex >= 0 && lastBreakIndex + lastBreakSkip <= static_cast<int>(line.size())) {
				lines.push_back(line.substr(0, lastBreakIndex));
				line = line.substr(lastBreakIndex + lastBreakSkip);
				lineWidth = MeasureLineWidth(line, style.font);
				lastBreakIndex = -1;
				lastBreakSkip = 0;
			} else {
				lines.push_back(line);
				line.clear();
				lineWidth = 0;
				lastBreakIndex = -1;
				lastBreakSkip = 0;
			}
		}

		line.append(p, len);
		lineWidth += metrics.advance;
		p += len;
		remaining -= len;
	}

	if (!line.empty())
		lines.push_back(line);

	if (lines.empty())
		lines.push_back(std::string());

	return lines;
}

void TextService::DrawLine(RenderTarget& target, const std::string& text,
					 int x, int y, int maxWidth, const TextStyle& style)
{
	if (!EnsureInitialized())
		return;

	std::string normalized = NormalizeText(text);
	int lineWidth = MeasureLineWidth(normalized, style.font);
	int drawX = x;
	if (style.align == TextAlign::Center) {
		drawX = x + (maxWidth - lineWidth) / 2;
	} else if (style.align == TextAlign::Right) {
		drawX = x + (maxWidth - lineWidth);
	}

	const char* p = normalized.c_str();
	int remaining = static_cast<int>(normalized.size());
	int penX = drawX;

	while (*p && remaining > 0) {
		int len = 0;
		uint32_t codepoint = Utf8Decode(p, remaining, &len);
		if (len == 0) break; // Safety: no data left
		p += len;
		remaining -= len;

		GlyphMetrics metrics;
		if (!m_backend->GetGlyphMetrics(style.font, codepoint, metrics))
			continue;

		const Glyph* glyph = m_backend->GetGlyph(style.font, codepoint, style.color);
		if (glyph) {
			// SDL_ttf renders each UTF-8 glyph on a full line-height surface.
			// Its baseline is already positioned inside that surface. Applying
			// ink bearings again raises descenders (g, p, y) and shifts capitals.
			// Only compensate for padding SDL_ttf adds for a negative left bearing.
			m_backend->DrawGlyph(target, *glyph,
				penX + (std::min)(0, metrics.bearingX), y, style.color.a);
		}

		penX += metrics.advance;
	}
}

void TextService::DrawLines(RenderTarget& target, const std::vector<std::string>& lines,
					   int x, int y, int maxWidth, int maxLines, int startLine,
					   const TextStyle& style, int lineHeight)
{
	if (!EnsureInitialized())
		return;

	int count = static_cast<int>(lines.size());
	if (startLine < 0)
		startLine = 0;
	if (startLine >= count)
		return;

	int endLine = count;
	if (maxLines > 0)
		endLine = (std::min)(count, startLine + maxLines);

	int drawY = y;
	for (int i = startLine; i < endLine; ++i) {
		DrawLine(target, lines[i], x, drawY, maxWidth, style);
		drawY += lineHeight + style.lineSpacing;
	}
}

// RenderText is not here: it is the one member of this class that draws
// through g_pLast, the back buffer the executable owns, which made this
// library unlinkable without that global - a test binary had to invent
// it. Client/TextServiceScreen.cpp defines it now, on the executable
// side of the same split MItem/MItemUse took (docs/RESTRUCTURING.md
// task 5.3). Everything else in this file draws through a RenderTarget
// the caller supplies, which is what kept the rest of it clean.

} // namespace TextSystem
