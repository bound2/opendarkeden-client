#include "test_framework.h"
#include "U_edit.h"
#include "src/InputFocusManager.h"
#include <SDL.h>
#include <cstring>

namespace {

struct TextInputSession
{
	TextInputSession()
	{
		SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
		CHECK_EQ(0, SDL_InitSubSystem(SDL_INIT_VIDEO));
		SDL_StopTextInput();
	}
	~TextInputSession()
	{
		InputFocusManager::GetInstance().SetFocusedEditor(nullptr);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
};

} // namespace

TEST(EditorFocus, BrowserReplacementUsesTheCurrentFieldAndCharacterLimit)
{
	TextInputSession session;
	LineEditorVisual editor;
	editor.SetByteLimit(3);
	editor.Acquire();
	auto& focus = InputFocusManager::GetInstance();
	const auto serial = focus.GetFocusSerial();
	CHECK(focus.ReplaceText(serial, "a\xF0\x9F\x98\x80" "b"));
	CHECK(!focus.ReplaceText(serial, "a\xF0\x9F\x98\x80" "bc"));
	CHECK_EQ(3, editor.Size());
	CHECK(std::strcmp(editor.GetString(), "a\xF0\x9F\x98\x80" "b") == 0);
	CHECK(focus.ReplaceText(serial, ""));
	CHECK_EQ(0, editor.Size());
	editor.SetByteLimit(100);
	const std::string longerThanSdlEvent(80, 'x');
	CHECK(focus.ReplaceText(serial, longerThanSdlEvent.c_str()));
	CHECK_EQ(80, editor.Size());
}

TEST(EditorFocus, BrowserReplacementRejectsAStaleFieldEvenAfterRefocus)
{
	TextInputSession session;
	LineEditorVisual first, second;
	first.Acquire();
	auto& focus = InputFocusManager::GetInstance();
	const auto serial = focus.GetFocusSerial();
	second.Acquire();
	CHECK(!focus.ReplaceText(serial, "wrong field"));
	first.Acquire();
	CHECK(!focus.ReplaceText(serial, "old edit"));
	first.Unacquire();
	CHECK(!focus.ReplaceText(focus.GetFocusSerial(), "closed"));
	CHECK_EQ(0, first.Size());
	CHECK_EQ(0, second.Size());
}

TEST(EditorFocus, AcquiringAndReleasingTheFocusedFieldControlsSdlInput)
{
	TextInputSession session;
	LineEditorVisual editor;
	editor.Acquire();
	CHECK(editor.IsAcquire());
	CHECK(SDL_IsTextInputActive());
	CHECK(InputFocusManager::GetInstance().GetFocusedEditor() == &editor);
	editor.Unacquire();
	CHECK(!SDL_IsTextInputActive());
	CHECK(!InputFocusManager::GetInstance().HasFocus());
}

TEST(EditorFocus, SwitchingFieldsDeactivatesTheOldFieldAndPreservesNewInput)
{
	TextInputSession session;
	LineEditorVisual first;
	LineEditorVisual second;
	first.Acquire();
	second.Acquire();
	CHECK(!first.IsAcquire());
	CHECK(!first.m_Editor.IsAcquire());
	CHECK(second.IsAcquire());
	first.Unacquire();
	CHECK(SDL_IsTextInputActive());
	InputFocusManager::GetInstance().HandleTextInput("second");
	CHECK(std::strcmp(first.GetString(), "") == 0);
	CHECK(std::strcmp(second.GetString(), "second") == 0);
}

TEST(EditorFocus, DestroyingTheFocusedEditorStopsTextInput)
{
	TextInputSession session;
	{
		LineEditorVisual editor;
		editor.Acquire();
		SDL_StartTextInput();
	}
	CHECK(!SDL_IsTextInputActive());
	CHECK(!InputFocusManager::GetInstance().HasFocus());
}

TEST(EditorFocus, PreeditWithNoSelectionIsStoredAndCommittedExactlyOnce)
{
	LineEditor editor;
	editor.HandleTextEditing("preedit", 7, 0);
	CHECK(editor.IsComposing());
	CHECK_EQ(0, editor.Size());
	editor.HandleTextInput("result");
	CHECK(std::strcmp(editor.GetString(), "result") == 0);
	CHECK(!editor.IsComposing());
	editor.HandleTextEditing("preview", 0, 2);
	editor.HandleTextInput("!");
	CHECK(std::strcmp(editor.GetString(), "result!") == 0);
}

TEST(EditorFocus, CancellingPreeditOrLosingFocusDoesNotCommitIt)
{
	TextInputSession session;
	LineEditorVisual first;
	LineEditorVisual second;
	first.Acquire();
	auto& focus = InputFocusManager::GetInstance();
	focus.HandleTextEditing("preview", 0, 2);
	focus.HandleTextEditing("", 0, 0);
	CHECK_EQ(0, first.Size());
	CHECK(!first.m_Editor.IsComposing());
	focus.HandleTextEditing("preview", 0, 2);
	second.Acquire();
	CHECK_EQ(0, first.Size());
	CHECK(!first.m_Editor.IsComposing());
	focus.HandleTextInput("x");
	focus.HandleKeyDown(VK_BACK);
	CHECK_EQ(0, second.Size());
}

TEST(EditorLayout, ConfiguredWidthControlsTheFullFieldCheck)
{
	LineEditorVisual editor;
	editor.AddString("abcd");
	editor.SetAbsWidth(24);
	CHECK(editor.ReachSizeOfBox());
	editor.SetAbsWidth(240);
	CHECK(!editor.ReachSizeOfBox());
	editor.SetAbsWidth(-1);
	CHECK_EQ(0, editor.m_AbsWidth);
}
