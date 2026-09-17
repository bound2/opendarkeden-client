#include "test_framework.h"
#include "Platform.h"
#include "SkinManager.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
struct SkinFile
{
	std::filesystem::path previous = std::filesystem::current_path();
	std::filesystem::path directory;
	SkinFile()
	{
		for (int i = 0; i < 1000; ++i)
		{
			auto candidate = std::filesystem::temp_directory_path() /
				("opendarkeden-skin-test-" + std::to_string(i));
			if (std::filesystem::create_directory(candidate))
			{
				directory = candidate;
				break;
			}
		}
		if (directory.empty()) throw std::runtime_error("cannot create skin fixture directory");
		std::filesystem::create_directories(directory / "Data" / "info");
		std::filesystem::current_path(directory);
	}
	~SkinFile()
	{
		std::filesystem::current_path(previous);
		std::filesystem::remove(directory / "Data" / "info" / "skin-test.inf");
		std::filesystem::remove(directory / "Data" / "info");
		std::filesystem::remove(directory / "Data");
		std::filesystem::remove(directory);
	}
	void Write(const std::string& text)
	{
		std::ofstream file(directory / "Data" / "info" / "skin-test.inf", std::ios::binary);
		file << text;
	}
};
}

TEST(SkinParser, IncompletePointDoesNotAppendCoordinates)
{
	InterfaceInformation info;
	CHECK(info.LoadFromLinePointList("10 -20"));
	CHECK(!info.LoadFromLinePointList("33"));
	CHECK(info.LoadFromLinePointList("40 50"));
	CHECK_EQ(10, info.GetPoint(0).x);
	CHECK_EQ(-20, info.GetPoint(0).y);
	CHECK_EQ(40, info.GetPoint(1).x);
	CHECK_EQ(50, info.GetPoint(1).y);
}

TEST(SkinParser, IncompleteRectangleDoesNotAppendCoordinates)
{
	InterfaceInformation info;
	CHECK(info.LoadFromLineRectList("1 2 3 4"));
	CHECK(!info.LoadFromLineRectList("10 20 30"));
	CHECK(!info.LoadFromLineRectList("not coordinates"));
	CHECK(info.LoadFromLineRectList("-4 -3 -2 -1"));
	CHECK_EQ(-4, info.GetRect(1).left);
	CHECK_EQ(-3, info.GetRect(1).top);
	CHECK_EQ(-2, info.GetRect(1).right);
	CHECK_EQ(-1, info.GetRect(1).bottom);
}

TEST(SkinParser, LoadsPointAndRectangleSections)
{
	SkinFile fixture;
	fixture.Write("; skin fixture\n*INFO POINT_LIST\n10 20\n*END\n"
		"*TITLE RECT_LIST\n1 2 30 40\n*END\n");
	SkinManager manager;
	CHECK(manager.LoadInformation("skin-test.inf"));
	CHECK_EQ(10, manager.Get(SkinManager::INFO).GetPoint(0).x);
	CHECK_EQ(20, manager.Get(SkinManager::INFO).GetPoint(0).y);
	CHECK_EQ(30, manager.Get(SkinManager::TITLE).GetRect(0).right);
	CHECK_EQ(40, manager.Get(SkinManager::TITLE).GetRect(0).bottom);
}

TEST(SkinParser, LongUnknownHeaderDoesNotDamageTheFollowingSection)
{
	SkinFile fixture;
	fixture.Write("*" + std::string(200, 'x') + " POINT_LIST\n*END\n"
		"*INFO POINT_LIST\n70 80\n*END\n");
	SkinManager manager;
	CHECK(manager.LoadInformation("skin-test.inf"));
	CHECK_EQ(70, manager.Get(SkinManager::INFO).GetPoint(0).x);
	CHECK_EQ(80, manager.Get(SkinManager::INFO).GetPoint(0).y);
}

TEST(SkinParser, InvalidNumericInputIsRejected)
{
	InterfaceInformation info;
	CHECK(!info.LoadFromLinePointList(nullptr));
	CHECK(!info.LoadFromLinePointList(""));
	CHECK(!info.LoadFromLinePointList("2147483648 0"));
	CHECK(!info.LoadFromLineRectList(nullptr));
	CHECK(!info.LoadFromLineRectList("0 0 0 -2147483649"));
	CHECK(info.LoadFromLinePointList("-2147483648 2147483647"));
	CHECK_EQ(-2147483647 - 1, info.GetPoint(0).x);
	CHECK_EQ(2147483647, info.GetPoint(0).y);
}

TEST(SkinParser, FailedReloadPreservesThePreviousSkin)
{
	SkinFile fixture;
	SkinManager manager;
	fixture.Write("*INFO POINT_LIST\n70 80\n*END\n");
	CHECK(manager.LoadInformation("skin-test.inf"));
	for (const char* invalid : {
		"*INFO\n", "*INFO POINT_LIST\n12\n*END\n",
		"*TITLE RECT_LIST\n1 2 3\n*END\n" })
	{
		fixture.Write(invalid);
		CHECK(!manager.LoadInformation("skin-test.inf"));
		CHECK_EQ(70, manager.Get(SkinManager::INFO).GetPoint(0).x);
		CHECK_EQ(80, manager.Get(SkinManager::INFO).GetPoint(0).y);
	}
}

TEST(SkinParser, WhitespaceAndCommentsDoNotCreateCoordinateRows)
{
	SkinFile fixture;
	fixture.Write("*INFO POINT_LIST\n \t\n \t; comment\n10 20\n*END\n");
	SkinManager manager;
	const bool loaded = manager.LoadInformation("skin-test.inf");
	CHECK(loaded);
	if (!loaded) return;
	CHECK_EQ(10, manager.Get(SkinManager::INFO).GetPoint(0).x);
	CHECK_EQ(20, manager.Get(SkinManager::INFO).GetPoint(0).y);
}
