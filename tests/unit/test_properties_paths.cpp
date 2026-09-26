//----------------------------------------------------------------------
// test_properties_paths.cpp
//----------------------------------------------------------------------
//
// Properties::load has resolved its file name through
// Basic::NormalizeDataPath (basic/DataPath.h) since the Linux port;
// Properties::save now does the same, so a file saved under the game's
// spelling of a path (backslashes, a case the disk need not share) lands
// in the directory load() reads it back from, instead of as one file
// whose name holds the backslashes. On Windows NormalizeDataPath is the
// identity and both spellings name the same file, so this passes there
// as it always did; the Linux and macOS jobs exercise the resolution.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "Properties.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>

TEST(PropertiesPaths, SaveAndLoadAgreeOnTheWindowsSpellingOfAPath)
{
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto directory = std::filesystem::temp_directory_path()
		/ ("darkeden_properties_paths_" + std::to_string(stamp));
	std::filesystem::create_directories(directory / "UserSet");
	struct Cleanup {
		std::filesystem::path directory;
		~Cleanup() {
			std::error_code error;
			std::filesystem::remove_all(directory, error);
		}
	} cleanup{directory};

	std::string requested = (directory / "USERSET" / "Option.inf").generic_string();
	std::replace(requested.begin(), requested.end(), '/', '\\');

	Properties saved;
	saved.setProperty("Volume", "7");
	saved.save(requested);

	CHECK(std::filesystem::exists(directory / "UserSet" / "Option.inf"));

	Properties loaded;
	loaded.load(requested);
	CHECK_EQ(7, loaded.getPropertyInt("Volume"));
}
