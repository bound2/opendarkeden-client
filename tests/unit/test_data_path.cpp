//----------------------------------------------------------------------
// test_data_path.cpp
//----------------------------------------------------------------------
//
// Basic::ResolveDataPath and Basic::NormalizeDataPath
// (basic/DataPath.cpp). The game's data paths are written the Windows
// way - backslashes, and a letter case that need not match the disk
// (docs/linux-macos-port-assessment-2026-09-07.md, area F: of the 206
// entries in FileDef.inf, 38 exist only under a different case). On a
// case-sensitive filesystem those opens fail, so the resolver folds the
// separators and, when the path does not exist as spelled, finds each
// component case-insensitively. It never invents a file: a path that
// matches nothing comes back folded and otherwise unchanged, and the
// caller's open fails as loudly as it would have.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "DataPath.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

struct SScratchDirectory
{
	std::filesystem::path	Path;

	SScratchDirectory()
	{
		const long long llStamp =
			std::chrono::steady_clock::now().time_since_epoch().count();

		std::error_code Error;

		Path = std::filesystem::temp_directory_path(Error)
			/ ("datapath_" + std::to_string(llStamp));

		std::filesystem::remove_all(Path, Error);
		std::filesystem::create_directories(Path / "Data" / "Info", Error);
		std::filesystem::create_directories(Path / "Data" / "Image", Error);

		AddFile("Data/Info/FileDef.inf");
		AddFile("Data/Image/Etc.spk");
	}

	~SScratchDirectory()
	{
		std::error_code Error;
		std::filesystem::remove_all(Path, Error);
	}

	void	AddFile(const char* pName) const
	{
		std::ofstream File(Path / pName, std::ios::binary);
		File << "x";
	}

	std::string	Name() const
	{
		return Path.generic_string();
	}
};

bool	Opens(const std::string& sPath)
{
	std::ifstream File(sPath.c_str(), std::ios::binary);
	return File.is_open();
}

} // namespace

//----------------------------------------------------------------------
// A path that exists as spelled, once the separators are folded, is
// returned folded and otherwise untouched.
//----------------------------------------------------------------------
TEST(DataPath, ResolveFoldsBackslashesOnAnExistingPath)
{
	const SScratchDirectory Scratch;

	const std::string sResolved =
		Basic::ResolveDataPath(Scratch.Name() + "\\Data\\Info\\FileDef.inf");

	CHECK(Scratch.Name() + "/Data/Info/FileDef.inf" == sResolved);
	CHECK(Opens(sResolved));
}

//----------------------------------------------------------------------
// A path whose components differ from the disk only in case resolves to
// the disk's spelling, and opens - on every filesystem, including the
// case-sensitive ones where the input would not.
//----------------------------------------------------------------------
TEST(DataPath, ResolveFindsComponentsCaseInsensitively)
{
	const SScratchDirectory Scratch;

	const std::string sResolved =
		Basic::ResolveDataPath(Scratch.Name() + "/data/IMAGE/etc.SPK");

	CHECK(Scratch.Name() + "/Data/Image/Etc.spk" == sResolved);
	CHECK(Opens(sResolved));
}

//----------------------------------------------------------------------
// A path that matches nothing is not invented: it comes back folded and
// otherwise as given, so the caller's open fails where it always did.
//----------------------------------------------------------------------
TEST(DataPath, ResolveLeavesAMissingPathAlone)
{
	const SScratchDirectory Scratch;

	const std::string sResolved =
		Basic::ResolveDataPath(Scratch.Name() + "\\Data\\Image\\Missing.spk");

	CHECK(Scratch.Name() + "/Data/Image/Missing.spk" == sResolved);
	CHECK_EQ(false, Opens(sResolved));
}

//----------------------------------------------------------------------
// A relative path resolves against the working directory the same way.
//----------------------------------------------------------------------
TEST(DataPath, ResolveHandlesARelativePath)
{
	const SScratchDirectory Scratch;

	std::error_code Error;
	const std::filesystem::path Previous = std::filesystem::current_path(Error);
	std::filesystem::current_path(Scratch.Path, Error);

	const std::string sResolved = Basic::ResolveDataPath("data\\info\\filedef.INF");

	std::filesystem::current_path(Previous, Error);

	CHECK(std::string("Data/Info/FileDef.inf") == sResolved);
}

//----------------------------------------------------------------------
// An empty path is returned empty; nothing is listed for it.
//----------------------------------------------------------------------
TEST(DataPath, ResolveOfEmptyIsEmpty)
{
	CHECK(Basic::ResolveDataPath("").empty());
}

//----------------------------------------------------------------------
// NormalizeDataPath is the entry the game calls. On Windows it is the
// identity - the Win32 file APIs take backslashes and ignore case, and
// the live platform's opens are byte-identical to what they were; off
// Windows it is ResolveDataPath.
//----------------------------------------------------------------------
TEST(DataPath, NormalizeIsIdentityOnWindowsAndResolvesElsewhere)
{
	const SScratchDirectory Scratch;

	const std::string sInput = Scratch.Name() + "\\data\\image\\ETC.spk";
	const std::string sNormalized = Basic::NormalizeDataPath(sInput);

#ifdef _WIN32
	CHECK(sInput == sNormalized);
#else
	CHECK(Scratch.Name() + "/Data/Image/Etc.spk" == sNormalized);
#endif
	CHECK(Opens(sNormalized));
}
