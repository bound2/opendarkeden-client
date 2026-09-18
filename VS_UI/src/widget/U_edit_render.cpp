#include "U_edit.h"
#include "../hangul/Ci.h"
#include "../InputFocusManager.h"
#include "../header/UISafeText.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_POSIX
#include <SDL.h>
#include "../../../Client/TextSystem/TextService.h"
#include "../../../Client/TextSystem/RenderTargetSpriteSurface.h"
#include "../../../Client/SpriteLib/CSpriteSurface.h"
#endif

// Forward declare FL2 functions (defined in hangul/FL2.cpp)
extern void g_Print(int x, int y, const char* sz_str, void* p_print_info);
extern int g_GetStringWidth(const char* sz_str, void* hfont);
extern int g_GetStringHeight(const char* sz_str, void* hfont);

// External reference to global CI (for cursor blink state)
extern CI* gC_ci;

// External reference to back buffer surface (for spritectl blt)
#ifdef PLATFORM_POSIX
// External reference to SDL renderer
extern SDL_Renderer* g_pSDLRenderer;
extern CSpriteSurface* g_pBack;
extern CSpriteSurface* g_pLast;  // UI renders to g_pLast, not g_pBack!
#endif

void LineEditorVisual::Show() const
{
	// Get the text to display (as UTF-8)
	const char* textToDisplay = m_Editor.GetBuffer();
	std::string displayText;

	// Password mode has always shown one asterisk per UTF-8 byte. Dynamic
	// storage preserves that count for the editor's full 4,092-byte result.
	if (m_bPasswordMode) {
		displayText = UISafeText::MakePasswordMask(textToDisplay);
		textToDisplay = displayText.c_str();
	}

#ifdef PLATFORM_POSIX
	// Use TextService for unified rendering
	extern CSpriteSurface* g_pLast;

	// Build text style from PrintInfo
	TextSystem::TextStyle style;
	style.font = TextSystem::TextService::Get().GetFont(14);  // Default font size
	style.align = TextSystem::TextAlign::Left;
	style.lineSpacing = 0;
	style.color = TextSystem::ColorFromRGB(m_PrintInfo.text_color);
	style.color.a = 255;

	// Create render target
	TextSystem::SpriteSurfaceRenderTarget target(g_pLast);

	// Render text
	// Note: TextService::DrawLine expects baseline position, but it adds GetFontAscent() internally
	// So we pass m_Y directly as the baseline position
	TextSystem::TextService::Get().DrawLine(target, textToDisplay, m_X, m_Y, m_MaxWidth, style);

	// Draw cursor if editor is acquired and cursor blink is on
	if (m_Editor.m_bAcquired && gC_ci != NULL && gC_ci->GetCursorBlink()) {
		// Calculate cursor X position using TextService
		int cursorX = m_X;
		if (m_Editor.m_CursorPos > 0) {
			const char* fullText = textToDisplay;
			const std::string cursorPrefix = UISafeText::Utf8Prefix(
				fullText, (size_t)m_Editor.m_CursorPos);

			// Measure text width using TextService
			TextSystem::Metrics metrics = TextSystem::TextService::Get().MeasureText(cursorPrefix, style, 0);
			cursorX = m_X + metrics.width;
		}

		// Draw cursor
		PrintInfo cursorPI = m_PrintInfo;
		cursorPI.text_color = m_CursorColor;

		if (m_Editor.m_ComposingLen > 0) {
			// During IME composition, show underline-style cursor
			g_Print(cursorX, m_Y + 2, "_", &cursorPI);
		} else {
			// Normal block cursor
			g_Print(cursorX, m_Y - 1, "▊", &cursorPI);
		}
	}
#else
	// Use the field's font and color for both text and caret. The default
	// 16px font is too tall for the chat input and disagrees with its history.
	PrintInfo printInfo = m_PrintInfo;
	const std::string prefix = UISafeText::Utf8Prefix(
		m_Editor.GetBuffer(), (size_t)m_Editor.m_CursorPos);
	const size_t cursorByte = prefix.size();
	std::string visibleText = textToDisplay;
	size_t visibleCursor = cursorByte;
	const int caretWidth = g_GetStringWidth("|", printInfo.hfont);
	const int width = m_AbsWidth > caretWidth ? m_AbsWidth - caretWidth : 0;

	// Scroll by complete UTF-8 characters until the caret fits, then trim
	// the right edge. This affects display only; the editor keeps all input.
	while (visibleCursor > 0 &&
		g_GetStringWidth(visibleText.substr(0, visibleCursor).c_str(), printInfo.hfont) > width) {
		const size_t first = UISafeText::Utf8Prefix(visibleText.c_str(), 1).size();
		visibleText.erase(0, first);
		visibleCursor -= first;
	}
	size_t end = visibleCursor;
	while (end < visibleText.size()) {
		const size_t next = end + UISafeText::Utf8Prefix(visibleText.c_str() + end, 1).size();
		if (g_GetStringWidth(visibleText.substr(0, next).c_str(), printInfo.hfont) > width)
			break;
		end = next;
	}
	visibleText.resize(end);
	g_Print(m_X, m_Y, visibleText.c_str(), &printInfo);

	if (m_Editor.m_bAcquired && gC_ci != NULL && gC_ci->GetCursorBlink()) {
		const int cursorX = m_X + g_GetStringWidth(
			visibleText.substr(0, visibleCursor).c_str(), printInfo.hfont);
		PrintInfo cursorPI = printInfo;
		cursorPI.text_color = m_CursorColor;
		g_Print(cursorX, m_Y, "|", &cursorPI);
	}
#endif
}

