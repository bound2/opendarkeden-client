//----------------------------------------------------------------------
// test_bundled_assets.cpp
//----------------------------------------------------------------------
//
// basic/BundledAssets: the manifest contract and the first-launch copy
// of the packaged data tree (docs/android-port-2026-09-19.md, "Bundled
// assets"). The source root is a scratch directory here, where on
// Android it is the APK's assets; SDL_RWFromFile is the one path in
// either case, so what is measured is the same code.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "BundledAssets.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>


namespace {

//----------------------------------------------------------------------
// A throwaway directory that removes itself, so a failed CHECK - which
// does not abort the test - still leaves nothing behind in the user's
// temporary directory. Two trees under it: src/ (the "package") and
// dst/ (the "internal storage").
//----------------------------------------------------------------------
struct SScratch
{
	std::filesystem::path	Path;

	SScratch()
	{
		const long long llStamp =
			std::chrono::steady_clock::now().time_since_epoch().count();

		std::error_code Error;
		Path = std::filesystem::temp_directory_path(Error)
			/ ("bundledassets_" + std::to_string(llStamp));
		std::filesystem::remove_all(Path, Error);
		std::filesystem::create_directories(Path / "src", Error);
	}

	~SScratch()
	{
		std::error_code Error;
		std::filesystem::remove_all(Path, Error);
	}

	std::string	Src() const { return (Path / "src").string(); }
	std::string	Dst() const { return (Path / "dst").string(); }

	void	Write(const std::string& sRelative, const std::string& sContent) const
	{
		const std::filesystem::path File = Path / sRelative;
		std::error_code Error;
		std::filesystem::create_directories(File.parent_path(), Error);
		std::ofstream Out(File, std::ios::binary);
		Out << sContent;
	}

	std::string	Read(const std::string& sRelative) const
	{
		std::ifstream In(Path / sRelative, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
	}

	bool	Exists(const std::string& sRelative) const
	{
		std::error_code Error;
		return std::filesystem::exists(Path / sRelative, Error);
	}
};

const char* MANIFEST = "darkeden-assets.manifest";

// A three-file tree with a space in one name and an empty directory,
// the shapes the real tree has.
void	WriteTree(const SScratch& S, const char* pVersion)
{
	S.Write("src/Data/Info/FileDef.inf", "filedef");
	S.Write("src/Data/Font/Sans Serif.ttf", "0123456789");
	S.Write("src/Data/Image/a.spk", "");
	S.Write(std::string("src/") + MANIFEST,
		std::string("darkeden-assets 1\n")
		+ "version " + pVersion + "\n"
		+ "dir UserSet\n"
		+ "dir Data/Image\n"
		+ "file 7 Data/Info/FileDef.inf\n"
		+ "file 10 Data/Font/Sans Serif.ttf\n"
		+ "file 0 Data/Image/a.spk\n");
}

size_t g_nProgressCalls = 0;
size_t g_nProgressTotal = 0;

void	CountProgress(size_t /*nDone*/, size_t nTotal, const char* /*pPath*/, void* /*pUser*/)
{
	g_nProgressCalls++;
	g_nProgressTotal = nTotal;
}

} // namespace


TEST(BundledAssets, InstallsTheTreeAndWritesTheMarkerLast)
{
	SScratch S;
	WriteTree(S, "assets-v2");
	g_nProgressCalls = 0;

	const Basic::SBundledAssetsResult R =
		Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst(), CountProgress, nullptr);

	CHECK(std::string("") == R.sError);
	CHECK_EQ(true, R.bInstalled);
	CHECK_EQ(false, R.bAlreadyCurrent);
	CHECK(std::string("assets-v2") == R.sVersion);
	CHECK_EQ(size_t(3), R.nFiles);
	CHECK_EQ(size_t(3), g_nProgressCalls);
	CHECK_EQ(size_t(3), g_nProgressTotal);
	CHECK(std::string("filedef") == S.Read("dst/Data/Info/FileDef.inf"));
	CHECK(std::string("0123456789") == S.Read("dst/Data/Font/Sans Serif.ttf"));
	CHECK_EQ(true, S.Exists("dst/Data/Image/a.spk"));
	CHECK_EQ(true, S.Exists("dst/UserSet"));
	CHECK(std::string("assets-v2\n") == S.Read(std::string("dst/") + Basic::BUNDLED_ASSETS_MARKER));
}

TEST(BundledAssets, ASecondLaunchWithTheSameVersionCopiesNothing)
{
	SScratch S;
	WriteTree(S, "assets-v2");
	Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst());

	// The user's edit survives: nothing is rewritten.
	S.Write("dst/Data/Info/FileDef.inf", "edited");
	g_nProgressCalls = 0;

	const Basic::SBundledAssetsResult R =
		Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst(), CountProgress, nullptr);

	CHECK(std::string("") == R.sError);
	CHECK_EQ(true, R.bInstalled);
	CHECK_EQ(true, R.bAlreadyCurrent);
	CHECK_EQ(size_t(0), R.nFiles);
	CHECK_EQ(size_t(0), g_nProgressCalls);
	CHECK(std::string("edited") == S.Read("dst/Data/Info/FileDef.inf"));
}

TEST(BundledAssets, ANewVersionOverwritesAndRewritesTheMarker)
{
	SScratch S;
	WriteTree(S, "assets-v2");
	Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst());

	S.Write("src/Data/Info/FileDef.inf", "v3");
	S.Write(std::string("src/") + MANIFEST,
		"darkeden-assets 1\nversion assets-v3\nfile 2 Data/Info/FileDef.inf\n");

	const Basic::SBundledAssetsResult R =
		Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst());

	CHECK(std::string("") == R.sError);
	CHECK_EQ(true, R.bInstalled);
	CHECK_EQ(false, R.bAlreadyCurrent);
	CHECK_EQ(size_t(1), R.nFiles);
	CHECK(std::string("v3") == S.Read("dst/Data/Info/FileDef.inf"));
	CHECK(std::string("assets-v3\n") == S.Read(std::string("dst/") + Basic::BUNDLED_ASSETS_MARKER));
	// Files the new manifest does not name are left alone.
	CHECK_EQ(true, S.Exists("dst/Data/Font/Sans Serif.ttf"));
}

TEST(BundledAssets, ASizeMismatchIsAnErrorAndLeavesNoMarker)
{
	SScratch S;
	WriteTree(S, "assets-v2");
	S.Write("src/Data/Info/FileDef.inf", "truncated!");	// 10 bytes, manifest says 7

	const Basic::SBundledAssetsResult R =
		Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst());

	CHECK_EQ(false, R.bInstalled);
	CHECK_EQ(false, R.sError.empty());
	CHECK_EQ(true, R.sError.find("manifest says 7") != std::string::npos);
	CHECK_EQ(false, S.Exists(std::string("dst/") + Basic::BUNDLED_ASSETS_MARKER));

	// Repaired, the next launch copies the whole tree again.
	S.Write("src/Data/Info/FileDef.inf", "filedef");
	const Basic::SBundledAssetsResult R2 =
		Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst());
	CHECK(std::string("") == R2.sError);
	CHECK_EQ(true, R2.bInstalled);
	CHECK_EQ(size_t(3), R2.nFiles);
}

TEST(BundledAssets, AnInterruptedCopyIsRedoneBecauseTheOldMarkerIsRemovedFirst)
{
	SScratch S;
	WriteTree(S, "assets-v2");
	Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst());

	// Version bump whose second file is missing from the package: the
	// copy fails part way. The v2 marker must not survive it.
	S.Write(std::string("src/") + MANIFEST,
		"darkeden-assets 1\nversion assets-v3\nfile 7 Data/Info/FileDef.inf\nfile 1 Data/Missing.inf\n");

	const Basic::SBundledAssetsResult R =
		Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst());

	CHECK_EQ(false, R.bInstalled);
	CHECK_EQ(true, R.sError.find("Data/Missing.inf") != std::string::npos);
	CHECK_EQ(false, S.Exists(std::string("dst/") + Basic::BUNDLED_ASSETS_MARKER));
}

TEST(BundledAssets, AMissingManifestIsAnError)
{
	SScratch S;

	const Basic::SBundledAssetsResult R =
		Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst());

	CHECK_EQ(false, R.bInstalled);
	CHECK_EQ(true, R.sError.find("cannot open the manifest") != std::string::npos);
	CHECK_EQ(false, S.Exists("dst"));
}

TEST(BundledAssets, MalformedManifestsAreRejectedBeforeAnythingIsWritten)
{
	SScratch S;
	S.Write("src/Data/a", "x");

	const char* apBad[] = {
		"",								// empty
		"not a manifest\nversion v\n",					// wrong header
		"darkeden-assets 1\nfile 1 Data/a\n",				// no version
		"darkeden-assets 1\nversion v\nfile x Data/a\n",		// bad size
		"darkeden-assets 1\nversion v\nfile 1\n",			// no path
		"darkeden-assets 1\nversion v\nfile 1 ../escape\n",		// escapes the destination
		"darkeden-assets 1\nversion v\ndir /abs\n",			// absolute
		"darkeden-assets 1\nversion v\nlink 1 Data/a\n",		// unknown entry
	};

	for (size_t i = 0; i < sizeof(apBad) / sizeof(apBad[0]); i++)
	{
		S.Write(std::string("src/") + MANIFEST, apBad[i]);
		const Basic::SBundledAssetsResult R =
			Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst());
		CHECK_EQ(false, R.bInstalled);
		CHECK_EQ(false, R.sError.empty());
		CHECK_EQ(false, S.Exists("dst"));
	}
}

TEST(BundledAssets, CRLFManifestsAndBlankLinesAreAccepted)
{
	SScratch S;
	S.Write("src/Data/a", "x");
	S.Write(std::string("src/") + MANIFEST,
		"darkeden-assets 1\r\n\r\nversion assets-v2\r\nfile 1 Data/a\r\n\r\n");

	const Basic::SBundledAssetsResult R =
		Basic::InstallBundledAssets(S.Src(), MANIFEST, S.Dst());

	CHECK(std::string("") == R.sError);
	CHECK_EQ(true, R.bInstalled);
	CHECK(std::string("assets-v2") == R.sVersion);
	CHECK(std::string("x") == S.Read("dst/Data/a"));
}
