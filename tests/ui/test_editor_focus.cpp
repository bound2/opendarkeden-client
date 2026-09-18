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
