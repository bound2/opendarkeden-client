#include "test_framework.h"
#include "U_edit.h"
#include <cstring>

TEST(LineEditor, EditsCharactersAndKeepsTheCursorInRange)
{
	LineEditor editor;
	editor.AddString("abc");
	CHECK_EQ(3, editor.Size());
	editor.SetCursor(1);
	editor.InsertChar('X');
	CHECK(std::strcmp(editor.GetString(), "aXbc") == 0);
	editor.Backspace();
	CHECK(std::strcmp(editor.GetString(), "abc") == 0);
	editor.DeleteChar(1);
	CHECK(std::strcmp(editor.GetString(), "ac") == 0);
	editor.EndCursor();
	CHECK_EQ(2, editor.GetCursor());
	editor.EraseAll();
	CHECK_EQ(0, editor.GetCursor());
	CHECK_EQ(0, editor.Size());
}
