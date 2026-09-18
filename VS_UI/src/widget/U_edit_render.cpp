#include "U_edit.h"
#include "../header/UISafeText.h"

// The compatibility printer and width function both use TextService on SDL.
extern CI* gC_ci;

void LineEditorVisual::Show() const
{
	// Get the text to display (as UTF-8)
	const std::string editorText = m_Editor.GetBuffer();
	const char* textToDisplay = editorText.c_str();
	std::string displayText;

	// Password mode has always shown one asterisk per UTF-8 byte. Dynamic
	// storage preserves that count for the editor's full 4,092-byte result.
	if (m_bPasswordMode) {
		displayText = UISafeText::MakePasswordMask(textToDisplay);
		textToDisplay = displayText.c_str();
	}
	// Use the field's font and color for both text and caret. The default
	// 16px font is too tall for the chat input and disagrees with its history.
	PrintInfo printInfo = m_PrintInfo;
	const std::string prefix = UISafeText::Utf8Prefix(
		editorText.c_str(), (size_t)m_Editor.m_CursorPos);
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
}
