//----------------------------------------------------------------------
// test_file_dialog_listing.cpp
//----------------------------------------------------------------------
//
// basic/FileDialogListing - the suffix filter and the insertion sort
// lifted out of C_VS_UI_FILE_DIALOG::RefreshFileList, the profile-picture
// file dialog (docs/RESTRUCTURING.md task 3.1, "moved, then fixed").
//
// This file pins the behaviour the dialog has TODAY, defects included,
// so that the move is provably behaviour-preserving. Two of the tests
// below therefore assert something wrong on purpose and say so; the fix
// commit that follows rewrites them into the behaviour the dialog should
// have had.
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
	size_t						uFilterCount;

	SDialogList() : uFilterCount(0) {}

	void	AddDirectory(const char* pName)
	{
		Basic::InsertDialogEntry(vNames, vAttributes,
			std::string("\\") + pName, FILE_ATTRIBUTE_DIRECTORY,
			uFilterCount);
	}

	void	AddFile(const char* pName)
	{
		Basic::InsertDialogEntry(vNames, vAttributes, pName,
			ATTRIBUTES_NORMAL, uFilterCount);
	}
};


//----------------------------------------------------------------------
// The dialog's own filter list, as C_VS_UI_FILE_DIALOG::Start() builds
// it: the type string split on ';', so ".bmp;.jpg" is two entries.
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
// The directory branch is the correct one, and the order it produces is
// what the file branch was meant to extend: every directory first,
// sorted by byte-wise comparison of the '\'-prefixed name. '\' is 0x5C
// and every printable name byte the dialog meets is above it, so a
// directory also sorts ahead of every file.
//----------------------------------------------------------------------
TEST(FileDialogListing, DirectoriesSortAmongThemselvesWhateverTheOrderIn)
{
	SDialogList List;

	List.uFilterCount = 2;

	List.AddDirectory("dir_b");
	List.AddDirectory("dir_a");
	List.AddDirectory("..");

	CHECK_EQ(3, List.vNames.size());
	CHECK_EQ(3, List.vAttributes.size());

	CHECK(List.vNames[0] == "\\..");
	CHECK(List.vNames[1] == "\\dir_a");
	CHECK(List.vNames[2] == "\\dir_b");

	for (size_t i = 0; i < List.vAttributes.size(); i++)
	{
		CHECK(0 != (List.vAttributes[i] & FILE_ATTRIBUTE_DIRECTORY));
	}
}


//----------------------------------------------------------------------
// THIS TEST PINS A DEFECT.
//
// The file branch's insertion loop declares its own `int i`, shadowing
// the `int i` the filter loop above it left at m_filter.size(). The
// append test after the loop therefore reads the FILTER count instead of
// the loop counter, and since entries arrive in ascending order the loop
// never breaks for a file, so a file is appended only when the filter
// count happens to equal the list size at that moment.
//
// With the two image filters and the two directories below, that is true
// exactly once: "a.bmp" lands because the list is two long, and "b.jpg"
// is dropped because it is three long by then. Every further file would
// be dropped too.
//
// The fix commit rewrites this test.
//----------------------------------------------------------------------
TEST(FileDialogListing, FilesAreDroppedUnlessTheFilterCountMatchesTheListSize)
{
	SDialogList List;

	List.uFilterCount = ImageFilters().size();

	List.AddDirectory("..");
	List.AddDirectory("dir");

	List.AddFile("a.bmp");
	List.AddFile("b.jpg");

	CHECK_EQ(3, List.vNames.size());
	CHECK_EQ(3, List.vAttributes.size());

	CHECK(List.vNames[0] == "\\..");
	CHECK(List.vNames[1] == "\\dir");
	CHECK(List.vNames[2] == "a.bmp");

	CHECK_EQ(0, (int)(List.vAttributes[2] & FILE_ATTRIBUTE_DIRECTORY));
}


//----------------------------------------------------------------------
// THIS TEST PINS THE SAME DEFECT FROM ITS OTHER SIDE.
//
// When the loop DOES find a place for the file it inserts and breaks -
// and the stale test still runs, against the list size the insert has
// just grown. When the two agree the entry goes in a second time, so the
// dialog shows one file twice and the parallel attribute vector grows
// with it.
//
// The fix commit rewrites this test.
//----------------------------------------------------------------------
TEST(FileDialogListing, APlacedFileIsInsertedTwiceWhenTheCountMatchesTheGrownSize)
{
	SDialogList List;

	List.uFilterCount = 2;

	List.AddFile("b.txt");

	CHECK_EQ(0, List.vNames.size());

	// An empty list is not the filter count, so the append test failed
	// and nothing went in. Seed the first entry directly instead.
	List.vNames.push_back("b.txt");
	List.vAttributes.push_back(ATTRIBUTES_NORMAL);

	List.AddFile("a.txt");

	CHECK_EQ(3, List.vNames.size());
	CHECK_EQ(3, List.vAttributes.size());

	CHECK(List.vNames[0] == "a.txt");
	CHECK(List.vNames[1] == "b.txt");
	CHECK(List.vNames[2] == "a.txt");
}


//----------------------------------------------------------------------
// The suffix filter, over names at least as long as the suffix - the
// only inputs it is defined for today, because a shorter name indexes
// std::string::operator[] past the end. Those are the inputs the fix
// commit adds.
//----------------------------------------------------------------------
TEST(FileDialogListing, ASuffixMatchesCaseInsensitively)
{
	const std::vector<std::string> vFilters = ImageFilters();

	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.bmp", vFilters));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.jpg", vFilters));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("A.BMP", vFilters));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("MiXeD.JpG", vFilters));

	// The name may be exactly the suffix; nothing indexes out of range.
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive(".bmp", vFilters));

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
