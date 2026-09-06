//----------------------------------------------------------------------
// test_file_dialog_listing.cpp
//----------------------------------------------------------------------
//
// basic/FileDialogListing - the suffix filter and the insertion sort
// lifted out of C_VS_UI_FILE_DIALOG::RefreshFileList, the profile-picture
// file dialog (docs/RESTRUCTURING.md task 3.1, "moved, then fixed").
//
// The move commit before this one pinned the behaviour the dialog had,
// two defects included. This is that file rewritten: the two tests that
// asserted a defect now assert what the dialog is supposed to show, and
// two more cover the inputs the old filter could not survive at all.
//
// Nothing here touches the filesystem: the enumeration is
// basic/DirectoryListing, pinned by test_directory_listing.cpp, and
// these two functions only run over its result.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "FileDialogListing.h"

#include <string>
#include <vector>


namespace {

//----------------------------------------------------------------------
// What RefreshFileList stores for a plain file. basic/Platform.h defines
// FILE_ATTRIBUTE_DIRECTORY on every platform, because the library needs
// it, and leaves the rest of the Win32 attribute set to <windows.h>, so
// the value is spelled out here rather than depended on.
//----------------------------------------------------------------------
const DWORD	ATTRIBUTES_NORMAL = 0x00000080;


//----------------------------------------------------------------------
// The dialog holds two vectors read by one index: the names, with a
// leading '\' on every directory, and the attributes, of which only
// FILE_ATTRIBUTE_DIRECTORY is ever read back.
//----------------------------------------------------------------------
struct SDialogList
{
	std::vector<std::string>	vNames;
	std::vector<DWORD>			vAttributes;

	void	AddDirectory(const char* pName)
	{
		Basic::InsertDialogEntry(vNames, vAttributes,
			std::string("\\") + pName, FILE_ATTRIBUTE_DIRECTORY);
	}

	void	AddFile(const char* pName)
	{
		Basic::InsertDialogEntry(vNames, vAttributes, pName,
			ATTRIBUTES_NORMAL);
	}
};


//----------------------------------------------------------------------
// The whole list as one string. A list that came out wrong then fails on
// its contents rather than indexing past the end of a short vector,
// which matters because the behaviour under test is entries going
// missing.
//----------------------------------------------------------------------
std::string	Join(const std::vector<std::string>& vNames)
{
	std::string	sJoined;

	for (size_t i = 0; i < vNames.size(); i++)
	{
		if (i > 0) sJoined += "|";
		sJoined += vNames[i];
	}

	return sJoined;
}


//----------------------------------------------------------------------
// The attribute vector read the only way the dialog reads it: one 'D'
// or 'F' per entry.
//----------------------------------------------------------------------
std::string	Kinds(const std::vector<DWORD>& vAttributes)
{
	std::string	sKinds;

	for (size_t i = 0; i < vAttributes.size(); i++)
	{
		sKinds += (vAttributes[i] & FILE_ATTRIBUTE_DIRECTORY) ? 'D' : 'F';
	}

	return sKinds;
}


//----------------------------------------------------------------------
// The dialog's own filter list, as C_VS_UI_FILE_DIALOG::Start() builds
// it: the type string split on ';', so ".bmp;.jpg" - the one live call,
// in VS_UI/src/VS_UI_GameCommon.cpp - is two entries.
//----------------------------------------------------------------------
std::vector<std::string>	ImageFilters()
{
	std::vector<std::string> vFilters;

	vFilters.push_back(".bmp");
	vFilters.push_back(".jpg");

	return vFilters;
}

} // anonymous namespace


//----------------------------------------------------------------------
// Directories sort among themselves whatever order they arrive in. '\'
// is 0x5C and the names are compared with it on the front, so ".." leads
// the list the way the dialog needs it to.
//----------------------------------------------------------------------
TEST(FileDialogListing, DirectoriesSortAmongThemselvesWhateverTheOrderIn)
{
	SDialogList	List;

	List.AddDirectory("dir_b");
	List.AddDirectory("dir_a");
	List.AddDirectory("..");

	CHECK_EQ(3, List.vNames.size());
	CHECK_EQ(3, List.vAttributes.size());

	CHECK(Join(List.vNames) == "\\..|\\dir_a|\\dir_b");
	CHECK(Kinds(List.vAttributes) == "DDD");
}


//----------------------------------------------------------------------
// The whole of what the dialog is for: every matching file is listed,
// after the directories, and a file the filter rejects is not.
//
// This is the material RefreshFileList walks for one directory - the
// synthesised "..", the subdirectories and files ListDirectory returns,
// the filter, the insert - with the live ".bmp;.jpg" filter. The real
// walk interleaves directories and files in name order; this fixture
// feeds the directories first, and the out-of-order arrivals are
// covered by AFileIsPlacedAmongTheFilesExactlyOnce below.
//
// Before the fix the insertion loop's append test read m_filter.size()
// instead of its own counter, so exactly one file went in here - the one
// that arrived while the list happened to be two entries long - and
// every file after it was dropped.
//----------------------------------------------------------------------
TEST(FileDialogListing, EveryMatchingFileIsListedAfterTheDirectories)
{
	static const char* const	pEntries[] = { "a.bmp", "b.jpg", "c.txt" };

	const std::vector<std::string>	vFilters = ImageFilters();

	SDialogList	List;

	List.AddDirectory("..");
	List.AddDirectory("dir");

	for (size_t i = 0; i < sizeof(pEntries) / sizeof(pEntries[0]); i++)
	{
		if (!Basic::MatchesAnySuffixCaseInsensitive(pEntries[i], vFilters))
			continue;

		List.AddFile(pEntries[i]);
	}

	CHECK_EQ(4, List.vNames.size());
	CHECK_EQ(4, List.vAttributes.size());

	CHECK(Join(List.vNames) == "\\..|\\dir|a.bmp|b.jpg");
	CHECK(Kinds(List.vAttributes) == "DDFF");
}


//----------------------------------------------------------------------
// A file goes in once, at its place among the files. The dialog's
// version ran its stale append test after the loop had already inserted,
// so a file whose place was found was inserted a second time whenever
// the filter count equalled the size the insert had just grown to.
//----------------------------------------------------------------------
TEST(FileDialogListing, AFileIsPlacedAmongTheFilesExactlyOnce)
{
	SDialogList	List;

	List.AddDirectory("..");

	List.AddFile("b.bmp");
	List.AddFile("a.bmp");
	List.AddFile("c.bmp");

	CHECK_EQ(4, List.vNames.size());
	CHECK_EQ(4, List.vAttributes.size());

	CHECK(Join(List.vNames) == "\\..|a.bmp|b.bmp|c.bmp");
	CHECK(Kinds(List.vAttributes) == "DFFF");
}


//----------------------------------------------------------------------
// A file whose name sorts below '\' still goes after the directories.
// That is what the '\' test in the file branch is for; plain string
// order would put "1.bmp" (0x31) ahead of "\dir".
//----------------------------------------------------------------------
TEST(FileDialogListing, AFileSortingBelowTheDirectoryPrefixStillFollowsIt)
{
	SDialogList	List;

	List.AddDirectory("dir");
	List.AddFile("1.bmp");

	CHECK_EQ(2, List.vNames.size());

	CHECK(Join(List.vNames) == "\\dir|1.bmp");
	CHECK(Kinds(List.vAttributes) == "DF");
}


//----------------------------------------------------------------------
// The suffix match folds ASCII case on both sides, because the name
// comes from the filesystem and the filter from the call site.
//----------------------------------------------------------------------
TEST(FileDialogListing, ASuffixMatchesCaseInsensitively)
{
	const std::vector<std::string>	vFilters = ImageFilters();

	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.bmp", vFilters));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.jpg", vFilters));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("A.BMP", vFilters));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("MiXeD.JpG", vFilters));

	// The name may be exactly the suffix.
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive(".bmp", vFilters));

	// ... and the filter may be the upper-case spelling, since Start()
	// takes its type string from whatever the call site wrote.
	std::vector<std::string>	vUpper;
	vUpper.push_back(".BMP");

	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.bmp", vUpper));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.BMP", vUpper));

	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("c.txt", vFilters));
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("noext", vFilters));
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("a.bmpx", vFilters));
}


//----------------------------------------------------------------------
// An empty filter list admits nothing, which is how the dialog behaves
// when Start() was never called. An empty filter STRING admits
// everything, which is what Start("") produces - a single empty entry -
// and is the only way the dialog lists a name with no extension.
//----------------------------------------------------------------------
TEST(FileDialogListing, AnEmptyFilterListAdmitsNothingAndAnEmptyFilterAdmitsAll)
{
	const std::vector<std::string> vNone;

	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("a.bmp", vNone));

	std::vector<std::string> vEmptyFilter;
	vEmptyFilter.push_back("");

	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.bmp", vEmptyFilter));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("noext", vEmptyFilter));
}


//----------------------------------------------------------------------
// A name shorter than the suffix cannot end with it. Before the fix
// this indexed the name at a wrapped size() - j - 1, so the answer was
// not merely wrong - the read was out of range, and in a Debug build
// std::string::operator[] takes the process down over it.
//
// The observable contract is what is asserted, per CLAUDE.md; the ASan
// tree is where the invalid access would have aborted.
//----------------------------------------------------------------------
TEST(FileDialogListing, ANameShorterThanTheSuffixIsNotListed)
{
	const std::vector<std::string>	vFilters = ImageFilters();

	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("x", vFilters));
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("mp", vFilters));
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("", vFilters));

	// The empty name against the empty filter is still a match: nothing
	// is longer than nothing.
	std::vector<std::string>	vEmptyFilter;
	vEmptyFilter.push_back("");

	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("", vEmptyFilter));
}


//----------------------------------------------------------------------
// A long filter is compared where it lies. C_VS_UI_FILE_DIALOG::Start()
// builds each entry in a char[30], so an entry of up to 29 characters
// reaches the matcher; the dialog's version strcpy'd it into a char[20]
// first, which wrote past the buffer for anything from 20 characters up.
//----------------------------------------------------------------------
TEST(FileDialogListing, ATwentyFiveCharacterFilterIsMatchedWithoutOverflow)
{
	const std::string	sSuffix = "." + std::string(24, 'a');

	CHECK_EQ(25, sSuffix.size());

	std::vector<std::string>	vLong;
	vLong.push_back(sSuffix);

	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive(
		"picture" + sSuffix, vLong));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive(sSuffix, vLong));

	// Shorter than the filter, and a name of the same length that does
	// not end with it.
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("picture.bmp",
		vLong));
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive(
		"." + std::string(23, 'a') + "b", vLong));
}
