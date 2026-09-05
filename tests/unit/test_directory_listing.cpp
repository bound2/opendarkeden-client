//----------------------------------------------------------------------
// test_directory_listing.cpp
//----------------------------------------------------------------------
//
// basic/DirectoryListing - the std::filesystem replacement for the
// _findfirst / _findnext walks in ProfileManager and Client.cpp
// (docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md, priority 6).
//
// The migrated callers are executable-side and have no test path, so the
// contract they were migrated against is pinned here instead: the match
// set, the case-insensitive ordinal order an NTFS _findnext walk produced,
// the treatment of subdirectories, and the promise that an absent
// directory is a false return rather than an exception.
//
// The expectations below were checked against FindFirstFileW on this
// machine before they were written down - see the semantics table in
// basic/DirectoryListing.h.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "DirectoryListing.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>


namespace {

//----------------------------------------------------------------------
// A throwaway directory that removes itself, so a failed CHECK - which
// does not abort the test - still leaves nothing behind in the user's
// temporary directory.
//----------------------------------------------------------------------
struct SScratchDirectory
{
	std::filesystem::path	Path;

	SScratchDirectory()
	{
		const long long llStamp =
			std::chrono::steady_clock::now().time_since_epoch().count();

		std::error_code Error;

		Path = std::filesystem::temp_directory_path(Error)
			/ ("dirlisting_" + std::to_string(llStamp));

		std::filesystem::remove_all(Path, Error);
		std::filesystem::create_directories(Path, Error);
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

	void	AddDirectory(const char* pName) const
	{
		std::error_code Error;
		std::filesystem::create_directory(Path / pName, Error);
	}

	std::string	Name() const
	{
		return Path.string();
	}
};

//----------------------------------------------------------------------
// Renders a listing as "a|b|c" so one CHECK covers both the match set and
// its order.
//----------------------------------------------------------------------
std::string	Render(const std::vector<Basic::SDirectoryEntry>& vEntries)
{
	std::string sResult;

	for (size_t i=0; i<vEntries.size(); i++)
	{
		if (i > 0)
		{
			sResult += "|";
		}

		sResult += vEntries[i].sName;
	}

	return sResult;
}

//----------------------------------------------------------------------
// The fixture every listing test shares: mixed case, several dots, a name
// with no dot at all, a subdirectory, and a long name whose 8.3 alias
// would match ".spk" patterns on NTFS but whose real name does not.
//----------------------------------------------------------------------
void	Populate(const SScratchDirectory& Scratch)
{
	Scratch.AddFile("a.b.spk");
	Scratch.AddFile("ab");
	Scratch.AddFile("abc");
	Scratch.AddFile("noextension");
	Scratch.AddFile("Upper.SPK");
	Scratch.AddFile("x.spk");
	Scratch.AddFile("x.spk.tmp");
	Scratch.AddFile("longprofilename.spkbackup");
	Scratch.AddDirectory("subdirectoryname");
}

} // anonymous namespace


//----------------------------------------------------------------------
// Wildcard semantics, asserted without touching the filesystem.
//----------------------------------------------------------------------
TEST(DirectoryListing, StarMatchesZeroOrMoreCharactersIncludingDots)
{
	CHECK_EQ(true, Basic::MatchesWildcard("x.spk", "*"));
	CHECK_EQ(true, Basic::MatchesWildcard("x.spk", "*.spk*"));
	CHECK_EQ(true, Basic::MatchesWildcard("x.spk.tmp", "*.spk*"));
	CHECK_EQ(true, Basic::MatchesWildcard("a.b.spk", "*.spk*"));
	CHECK_EQ(true, Basic::MatchesWildcard("x.spk", "x.spk*"));
	CHECK_EQ(false, Basic::MatchesWildcard("x.bmp", "*.spk*"));
}


//----------------------------------------------------------------------
// "*.*" is not "a name containing a dot": Win32 returns every entry for
// it, dotless names included, which is why the migrated profile walk asks
// for "*" instead.
//----------------------------------------------------------------------
TEST(DirectoryListing, StarDotStarDoesNotMatchADotlessNameButStarDoes)
{
	CHECK_EQ(false, Basic::MatchesWildcard("noextension", "*.*"));
	CHECK_EQ(true, Basic::MatchesWildcard("noextension", "*"));
}


TEST(DirectoryListing, QuestionMarkMatchesExactlyOneCharacter)
{
	CHECK_EQ(true, Basic::MatchesWildcard("abc", "ab?"));
	CHECK_EQ(true, Basic::MatchesWildcard("abc", "???"));

	// Win32's DOS_QM would let a trailing '?' match zero characters and
	// find "ab" here. That is a documented deviation.
	CHECK_EQ(false, Basic::MatchesWildcard("ab", "ab?"));
	CHECK_EQ(false, Basic::MatchesWildcard("abcd", "ab?"));
}


TEST(DirectoryListing, MatchingIsCaseInsensitiveOverAscii)
{
	CHECK_EQ(true, Basic::MatchesWildcard("Upper.SPK", "*.spk"));
	CHECK_EQ(true, Basic::MatchesWildcard("upper.spk", "*.SPK"));
	CHECK_EQ(true, Basic::MatchesWildcard("LOG42.TXT", "Log*.txt"));
}


//----------------------------------------------------------------------
// An empty pattern matches only an empty name, and no directory entry has
// one - so it lists nothing rather than everything.
//----------------------------------------------------------------------
TEST(DirectoryListing, EmptyPatternMatchesOnlyTheEmptyName)
{
	CHECK_EQ(true, Basic::MatchesWildcard("", ""));
	CHECK_EQ(false, Basic::MatchesWildcard("x.spk", ""));
	CHECK_EQ(true, Basic::MatchesWildcard("", "*"));
}


TEST(DirectoryListing, NullNameOrPatternMatchesNothing)
{
	CHECK_EQ(false, Basic::MatchesWildcard(NULL, "*"));
	CHECK_EQ(false, Basic::MatchesWildcard("x.spk", NULL));
	CHECK_EQ(false, Basic::MatchesWildcard(NULL, NULL));
}


//----------------------------------------------------------------------
// The order is the one NTFS keeps its directory index in - a
// case-insensitive ordinal sort - so a migrated caller is handed the
// entries in the order _findnext produced them.
//----------------------------------------------------------------------
TEST(DirectoryListing, ListsFilesInCaseInsensitiveOrdinalOrder)
{
	const SScratchDirectory Scratch;
	Populate(Scratch);

	std::vector<Basic::SDirectoryEntry> vEntries;

	CHECK_EQ(true, Basic::ListDirectory(Scratch.Name().c_str(), "*", vEntries));

	CHECK(Render(vEntries) ==
		"a.b.spk|ab|abc|longprofilename.spkbackup|noextension|Upper.SPK|x.spk|x.spk.tmp");
}


//----------------------------------------------------------------------
// The two patterns DeleteProfiles walks, and the one CheckLogFile walks.
//----------------------------------------------------------------------
TEST(DirectoryListing, MigratedPatternsSelectTheSameFilesFindFirstFileDid)
{
	const SScratchDirectory Scratch;
	Populate(Scratch);
	Scratch.AddFile("q-spk");
	Scratch.AddFile("zz-spk.tmp");

	std::vector<Basic::SDirectoryEntry> vEntries;

	CHECK_EQ(true, Basic::ListDirectory(Scratch.Name().c_str(), "*.spk*", vEntries));
	CHECK(Render(vEntries) ==
		"a.b.spk|longprofilename.spkbackup|Upper.SPK|x.spk|x.spk.tmp");

	CHECK_EQ(true, Basic::ListDirectory(Scratch.Name().c_str(), "*-spk*", vEntries));
	CHECK(Render(vEntries) == "q-spk|zz-spk.tmp");

	CHECK_EQ(true, Basic::ListDirectory(Scratch.Name().c_str(), "Log*.txt", vEntries));
	CHECK_EQ(0, vEntries.size());
}


//----------------------------------------------------------------------
// "longprofilename.spkbackup" has the 8.3 alias LONGPR~1.SPK on an NTFS
// volume with short-name generation on, and FindFirstFile would return it
// for "*.spk". This helper matches the real name only.
//----------------------------------------------------------------------
TEST(DirectoryListing, ShortNameAliasesAreNotMatched)
{
	const SScratchDirectory Scratch;
	Populate(Scratch);

	std::vector<Basic::SDirectoryEntry> vEntries;

	CHECK_EQ(true, Basic::ListDirectory(Scratch.Name().c_str(), "*.spk", vEntries));
	CHECK(Render(vEntries) == "a.b.spk|Upper.SPK|x.spk");
}


//----------------------------------------------------------------------
// Subdirectories are opt-in, and "." and ".." are never produced - the
// only entries the legacy walk returned that this one cannot.
//----------------------------------------------------------------------
TEST(DirectoryListing, SubdirectoriesAreExcludedUnlessAskedFor)
{
	const SScratchDirectory Scratch;
	Populate(Scratch);

	std::vector<Basic::SDirectoryEntry> vFiles;
	std::vector<Basic::SDirectoryEntry> vAll;

	CHECK_EQ(true, Basic::ListDirectory(Scratch.Name().c_str(), "s*", vFiles));
	CHECK_EQ(0, vFiles.size());

	CHECK_EQ(true, Basic::ListDirectory(Scratch.Name().c_str(), "s*", vAll,
			Basic::LIST_FILES_AND_DIRECTORIES));
	CHECK(Render(vAll) == "subdirectoryname");
	CHECK_EQ(1, vAll.size());
	CHECK_EQ(true, vAll[0].bIsDirectory);

	CHECK_EQ(true, Basic::ListDirectory(Scratch.Name().c_str(), "*", vAll,
			Basic::LIST_FILES_AND_DIRECTORIES));

	for (size_t i=0; i<vAll.size(); i++)
	{
		CHECK(vAll[i].sName != ".");
		CHECK(vAll[i].sName != "..");
	}
}


//----------------------------------------------------------------------
// The file dialog (C_VS_UI_FILE_DIALOG::RefreshFileList) is the one
// caller that hands over a directory with its trailing separator still
// on - "c:\a\b\" - because it strips only the "*.*" off its search
// pattern. The listing, its order and the bare names must not change.
//----------------------------------------------------------------------
TEST(DirectoryListing, ATrailingSeparatorOnTheDirectoryChangesNothing)
{
	const SScratchDirectory Scratch;
	Populate(Scratch);

	std::vector<Basic::SDirectoryEntry> vBare;
	std::vector<Basic::SDirectoryEntry> vSlashed;

	const std::string sSlashed = Scratch.Name() + "\\";

	CHECK_EQ(true, Basic::ListDirectory(Scratch.Name().c_str(), "*", vBare,
			Basic::LIST_FILES_AND_DIRECTORIES));
	CHECK_EQ(true, Basic::ListDirectory(sSlashed.c_str(), "*", vSlashed,
			Basic::LIST_FILES_AND_DIRECTORIES));

	CHECK(Render(vBare) == Render(vSlashed));
	CHECK_EQ(9, vSlashed.size());

	for (size_t i=0; i<vSlashed.size(); i++)
	{
		CHECK(vSlashed[i].sName.find('\\') == std::string::npos);
		CHECK(vSlashed[i].sName.find('/') == std::string::npos);
	}
}


//----------------------------------------------------------------------
// An empty but readable directory is a success with nothing in it; a
// directory that is not there at all is a failure. Neither throws, which
// is what lets a caller keep the plain if-guard the _findfirst walk had.
//----------------------------------------------------------------------
TEST(DirectoryListing, EmptyDirectoryListsNothingAndSucceeds)
{
	const SScratchDirectory Scratch;

	std::vector<Basic::SDirectoryEntry> vEntries;

	CHECK_EQ(true, Basic::ListDirectory(Scratch.Name().c_str(), "*", vEntries));
	CHECK_EQ(0, vEntries.size());
}


TEST(DirectoryListing, MissingDirectoryFailsWithoutThrowing)
{
	const SScratchDirectory Scratch;
	Populate(Scratch);

	const std::string sMissing = (Scratch.Path / "no_such_subdirectory").string();

	std::vector<Basic::SDirectoryEntry> vEntries;

	CHECK_EQ(true, Basic::ListDirectory(Scratch.Name().c_str(), "*", vEntries));
	CHECK(vEntries.size() > 0);

	// The previous result must be cleared even on failure, so a caller
	// that ignores the return value cannot process a stale listing.
	CHECK_EQ(false, Basic::ListDirectory(sMissing.c_str(), "*", vEntries));
	CHECK_EQ(0, vEntries.size());

	CHECK_EQ(false, Basic::ListDirectory(NULL, "*", vEntries));
	CHECK_EQ(false, Basic::ListDirectory(Scratch.Name().c_str(), NULL, vEntries));
}


//----------------------------------------------------------------------
// A regular file is not a directory, so passing one where a directory
// belongs must fail rather than yield the file itself.
//----------------------------------------------------------------------
TEST(DirectoryListing, AFileWhereADirectoryBelongsFails)
{
	const SScratchDirectory Scratch;
	Populate(Scratch);

	const std::string sFile = (Scratch.Path / "x.spk").string();

	std::vector<Basic::SDirectoryEntry> vEntries;

	CHECK_EQ(false, Basic::ListDirectory(sFile.c_str(), "*", vEntries));
	CHECK_EQ(0, vEntries.size());
}
