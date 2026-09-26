// The per-character settings and mail files are named through ProfilePath so
// that they land in UserSet/ on every platform. The browser build persists
// only that directory, and a literal backslash in the name kept the file
// out of it, which lost the skill hotkeys on every reload.
#include "test_framework.h"
#include "Platform.h"
#include "ProfilePath.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::string Contents(std::ifstream& file)
{
	std::string text;
	std::getline(file, text);
	return text;
}

// Runs a body inside a fresh temporary working directory and restores the old one.
template <typename Body>
void InTemporaryDirectory(Body body)
{
	const std::filesystem::path previous = std::filesystem::current_path();
	const std::filesystem::path root = std::filesystem::temp_directory_path() / "darkeden-profile-path-test";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root / "UserSet");
	std::filesystem::current_path(root);
	body();
	std::filesystem::current_path(previous);
	std::filesystem::remove_all(root);
}

void WriteText(const std::string& path, const char* text)
{
	std::ofstream file(path, std::ios::binary);
	file << text << '\n';
}

} // namespace

TEST(ProfilePath, CharacterFilesLiveUnderTheProfileRoot)
{
	const std::string root = _PROFILE_ROOT;
	CHECK(ProfilePath::Character("Ann", ".set") == root + "Ann.set");
	CHECK(ProfilePath::CharacterWorld("Ann", 2, ".set") == root + "Ann-2.set");
	CHECK(ProfilePath::CharacterDimensionWorld("Ann", 1, 2, ".set") == root + "Ann-1-2.set");
	CHECK(ProfilePath::CharacterDimensionWorld("Ann", 1, 2, ".mail") == root + "Ann-1-2.mail");
#ifdef PLATFORM_WINDOWS
	CHECK(root == "UserSet\\");
#else
	CHECK(root == "UserSet/");
	CHECK(ProfilePath::CharacterDimensionWorld("Ann", 1, 2, ".set").find('\\') == std::string::npos);
#endif
}

TEST(ProfilePath, OpensTheNewestNamingFirst)
{
	InTemporaryDirectory([] {
		std::ifstream file;
		CHECK(!ProfilePath::OpenCharacterSettings(file, "Ann", 1, 2));

		WriteText(ProfilePath::Character("Ann", ".set"), "oldest");
		CHECK(ProfilePath::OpenCharacterSettings(file, "Ann", 1, 2));
		CHECK(Contents(file) == "oldest");
		file.close();

		WriteText(ProfilePath::CharacterWorld("Ann", 2, ".set"), "older");
		CHECK(ProfilePath::OpenCharacterSettings(file, "Ann", 1, 2));
		CHECK(Contents(file) == "older");
		file.close();

		WriteText(ProfilePath::CharacterDimensionWorld("Ann", 1, 2, ".set"), "current");
		CHECK(ProfilePath::OpenCharacterSettings(file, "Ann", 1, 2));
		CHECK(Contents(file) == "current");
		file.close();
	});
}

#ifndef PLATFORM_WINDOWS
TEST(ProfilePath, FallsBackToTheMisnamedFileOfEarlierBuilds)
{
	InTemporaryDirectory([] {
		WriteText(ProfilePath::LegacyCharacterDimensionWorld("Ann", 1, 2, ".set"), "legacy");
		std::ifstream file;
		CHECK(ProfilePath::OpenCharacterSettings(file, "Ann", 1, 2));
		CHECK(Contents(file) == "legacy");
		file.close();

		WriteText(ProfilePath::CharacterDimensionWorld("Ann", 1, 2, ".set"), "current");
		CHECK(ProfilePath::OpenCharacterSettings(file, "Ann", 1, 2));
		CHECK(Contents(file) == "current");
	});
}
#endif
