#include "test_framework.h"
#include "Platform.h"
#include "ProfileManager.h"
#include "ShrineInfoManager.h"
#include "Properties.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace {
struct FileDefinitionScope {
	Properties definitions;
	Properties* previous = g_pFileDef;
	FileDefinitionScope() { g_pFileDef = &definitions; }
	~FileDefinitionScope() { g_pFileDef = previous; }
};
struct TowerFile {
	static constexpr const char* path = "ui_metadata_towers.txt";
	explicit TowerFile(const std::string& text) {
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out.write(text.data(), static_cast<std::streamsize>(text.size()));
	}
	~TowerFile() { std::error_code error; std::filesystem::remove(path, error); }
};
}

TEST(UiMetadata, ProfileMapOwnsNamesAndReplacesFilenames)
{
	ProfileManager profiles;
	CHECK(!profiles.HasProfile("Slayer"));
	CHECK(profiles.GetFilename("Slayer") == nullptr);
	std::string name = "Slayer", filename = "first.spk";
	profiles.AddProfile(name.c_str(), filename.c_str());
	name = "changed";
	filename = "changed";
	CHECK(profiles.HasProfile("Slayer"));
	CHECK(std::string(profiles.GetFilename("Slayer")) == "first.spk");
	profiles.AddProfile("Slayer", "second.spk");
	CHECK(std::string(profiles.GetFilename("Slayer")) == "second.spk");
	CHECK(profiles.RemoveProfile("Slayer"));
	CHECK(!profiles.RemoveProfile("Slayer"));
	profiles.AddProfile("Vampire", "third.spk");
	profiles.Release();
	CHECK(!profiles.HasProfile("Vampire"));
}

TEST(UiMetadata, ShrineFileAdapterUsesTheRealResourceReaderAndParser)
{
	FileDefinitionScope scope;
	scope.definitions.setProperty("FILE_INFO_DATA", "ui_metadata_absent.rpk");
	scope.definitions.setProperty("FILE_REGEN_TOWER_INFO", TowerFile::path);
	RegenTowerInfoManager towers;
	{
		TowerFile file("; tower metadata\n*2\n0 72 18 21\n1 73 5 250\n");
		CHECK(towers.LoadRegenTowerInfo());
		CHECK_EQ(2, towers.GetSize());
		CHECK_EQ(72, towers.Get(0).zoneID);
		CHECK_EQ(18, towers.Get(0).x);
		CHECK_EQ(21, towers.Get(0).y);
		CHECK_EQ(73, towers.Get(1).zoneID);
		CHECK_EQ(250, towers.Get(1).y);
	}
	// Failed refreshes must leave the last complete table available.
	CHECK(!towers.LoadRegenTowerInfo());
	CHECK_EQ(2, towers.GetSize());
	{
		std::string text = "*1\n0 71 1 2";
		text.push_back('\0');
		text += "hidden\n";
		TowerFile file(text);
		CHECK(!towers.LoadRegenTowerInfo());
		CHECK_EQ(72, towers.Get(0).zoneID);
	}
}

#include "StringCell.h"

TEST(UiText, StringCellPreservesAliasedInputAndNullSemantics)
{
	StringCell cell;
	CHECK(cell.GetString() == nullptr);
	cell.SetString("alpha beta");
	cell.SetString(cell.GetString());
	CHECK(std::string(cell.GetString()) == "alpha beta");
	cell.SetString(cell.GetString() + 6);
	CHECK(std::string(cell.GetString()) == "beta");
	cell.SetString(nullptr);
	CHECK(std::string(cell.GetString()) == "beta");
	cell.Release();
	CHECK(cell.GetString() == nullptr);
}
