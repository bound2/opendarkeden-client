#include "test_framework.h"
#include "U_edit.h"
#include <cstring>
#include <string>

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

TEST(LineEditor, Utf8ResultsRemainOwnedAcrossEditorsAndEdits)
{
	LineEditor first;
	LineEditor second;
	first.AddString("first");
	second.AddString("second");
	const auto firstText = first.GetBuffer();
	const auto secondText = second.GetBuffer();
	CHECK(std::string(firstText) == "first");
	CHECK(std::string(secondText) == "second");
	first.EraseAll();
	first.AddString("replacement");
	first.GetBuffer();
	CHECK(std::string(firstText) == "first");
}

TEST(LineEditor, LegacyTextViewsBelongToTheirEditor)
{
	LineEditor first;
	LineEditor second;
	first.AddString("first");
	second.AddString("second");
	const char* firstText = first.GetString();
	const char* secondText = second.GetString();
	CHECK(std::strcmp(firstText, "first") == 0);
	CHECK(std::strcmp(secondText, "second") == 0);
}

TEST(LineEditor, WideResultsRemainOwnedAcrossEditorsAndEdits)
{
	LineEditorVisual first;
	LineEditorVisual second;
	first.AddString("first");
	second.AddString("second");
	const auto firstText = first.GetStringWide();
	const auto secondText = second.GetStringWide();
	const std::basic_string<char_t> expectedFirst = {'f', 'i', 'r', 's', 't'};
	const std::basic_string<char_t> expectedSecond = {'s', 'e', 'c', 'o', 'n', 'd'};
	CHECK(std::basic_string<char_t>(firstText) == expectedFirst);
	CHECK(std::basic_string<char_t>(secondText) == expectedSecond);
	first.EraseAll();
	first.GetStringWide();
	CHECK(std::basic_string<char_t>(firstText) == expectedFirst);
}

TEST(LineEditor, WideResultsHoldEverySupplementaryScalarIncludingTheMaximum)
{
	LineEditorVisual editor;
	for (int i = 0; i < LineEditor::MAX_TEXT - 1; ++i)
		editor.m_Editor.InsertChar(0x10FFFF);
	const std::basic_string<char_t> wide = editor.GetStringWide();
	CHECK_EQ((LineEditor::MAX_TEXT - 1) * 2, wide.size());
	CHECK_EQ(0xDBFF, wide[0]);
	CHECK_EQ(0xDFFF, wide[1]);
	CHECK_EQ(0, wide.c_str()[wide.size()]);
}

TEST(LineEditor, InvalidScalarsBecomeReplacementCharactersInBothResults)
{
	LineEditorVisual editor;
	editor.m_Editor.InsertChar(0xD800);
	editor.m_Editor.InsertChar(0xDFFF);
	editor.m_Editor.InsertChar(0x110000);
	CHECK(editor.m_Editor.GetBuffer() == "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");
	const auto wide = editor.GetStringWide();
	CHECK_EQ(3, wide.size());
	for (char_t unit : wide)
		CHECK_EQ(0xFFFD, unit);
}
