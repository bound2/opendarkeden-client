#include "InputFocusManager.h"
#include "widget/U_edit.h"
#include "header/UISafeText.h"
#include <SDL.h>
#include <algorithm>

InputFocusManager::InputFocusManager()
	: m_focusedEditor(NULL)
{
}

InputFocusManager& InputFocusManager::GetInstance()
{
	static InputFocusManager instance;
	return instance;
}

// Global accessor for DXLibBackendSDL (cannot include headers)
InputFocusManager& g_GetInputFocusManager()
{
	return InputFocusManager::GetInstance();
}

void InputFocusManager::SetFocusedEditor(LineEditorVisual* editor)
{
	if (m_focusedEditor == editor)
		return;
	if (m_focusedEditor != nullptr) {
		m_focusedEditor->m_Editor.Unacquire();
		m_focusedEditor->m_bAcquired = false;
	}
	m_focusedEditor = editor;
	++m_focusSerial;
	if (editor != nullptr) {
		editor->m_Editor.Acquire();
		editor->m_bAcquired = true;
		SDL_StartTextInput();
	} else {
		SDL_StopTextInput();
	}
}

LineEditorVisual* InputFocusManager::GetFocusedEditor() const
{
	return m_focusedEditor;
}

void InputFocusManager::HandleTextInput(const char* text)
{
	if (!m_focusedEditor) {
		// No editor has focus, ignore input
		return;
	}

	// Route text input to focused editor
	m_focusedEditor->m_Editor.HandleTextInput(text);
}

void InputFocusManager::HandleTextEditing(const char* text, int start, int length)
{
	if (!m_focusedEditor) {
		// No editor has focus, ignore editing events
		return;
	}

	// Route text editing to focused editor
	m_focusedEditor->m_Editor.HandleTextEditing(text, start, length);
}

void InputFocusManager::HandleKeyDown(unsigned int vk_code)
{
	if (!m_focusedEditor) {
		// No editor has focus, ignore key events
		return;
	}

	// Route key down to focused editor
	// WM_KEYDOWN = 0x0100 (Windows message value)
	m_focusedEditor->m_Editor.KeyboardControl(0x0100, vk_code, 0);
}

bool InputFocusManager::ReplaceText(uint32_t serial, const char* text)
{
	if (!m_focusedEditor || serial != m_focusSerial || !text) return false;
	uint32_t characters[LineEditor::MAX_TEXT];
	const auto length = UISafeText::Utf8ToUtf32(text, characters, LineEditor::MAX_TEXT);
	auto& editor = m_focusedEditor->m_Editor;
	const int limit = std::clamp(editor.m_Limit, 0, LineEditor::MAX_TEXT - 1);
	if (length > static_cast<size_t>(limit)) return false;
	editor.EraseAll();
	editor.InsertText(characters, static_cast<int>(length));
	return true;
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
extern "C" {
EMSCRIPTEN_KEEPALIVE uint32_t darkeden_text_target()
{
	auto& focus = InputFocusManager::GetInstance();
	return focus.HasFocus() ? focus.GetFocusSerial() : 0;
}
EMSCRIPTEN_KEEPALIVE const char* darkeden_text_value()
{
	auto* editor = InputFocusManager::GetInstance().GetFocusedEditor();
	return editor ? editor->GetString() : "";
}
EMSCRIPTEN_KEEPALIVE int darkeden_text_password()
{
	auto* editor = InputFocusManager::GetInstance().GetFocusedEditor();
	return editor && editor->m_bPasswordMode;
}
EMSCRIPTEN_KEEPALIVE int darkeden_text_limit()
{
	auto* editor = InputFocusManager::GetInstance().GetFocusedEditor();
	return editor ? editor->m_Editor.m_Limit : 0;
}
EMSCRIPTEN_KEEPALIVE int darkeden_text_replace(uint32_t serial, const char* text)
{
	return InputFocusManager::GetInstance().ReplaceText(serial, text);
}
}
#endif
