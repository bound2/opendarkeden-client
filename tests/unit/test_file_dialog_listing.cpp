//----------------------------------------------------------------------
// test_file_dialog_listing.cpp
//----------------------------------------------------------------------
//
// basic/FileDialogListing - the suffix filter and the insertion sort
// C_VS_UI_FILE_DIALOG builds its list with. Nothing here touches the
// filesystem; the enumeration is basic/DirectoryListing.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "FileDialogListing.h"

#include <string>
#include <vector>


namespace {

// What RefreshFileList stores for a plain file. Spelled out, because
// basic/Platform.h defines only FILE_ATTRIBUTE_DIRECTORY.
const DWORD	ATTRIBUTES_NORMAL = 0x00000080;


// The dialog holds two vectors read by one index: the names, with a
// leading '\' on every directory, and the attributes.
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


// The whole list as one string, so a wrong list fails on its contents
// rather than indexing past the end of a short vector.
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


// The attributes read the only way the dialog reads them: 'D' or 'F'.
std::string	Kinds(const std::vector<DWORD>& vAttributes)
{
	std::string	sKinds;

	for (size_t i = 0; i < vAttributes.size(); i++)
	{
		sKinds += (vAttributes[i] & FILE_ATTRIBUTE_DIRECTORY) ? 'D' : 'F';
	}

	return sKinds;
}


// The dialog's own filter list: Start() splits its type string on ';'.
std::vector<std::string>	ImageFilters()
{
	std::vector<std::string> vFilters;

	vFilters.push_back(".bmp");
	vFilters.push_back(".jpg");

	return vFilters;
}

} // anonymous namespace


// The names are compared with the '\' on the front, so ".." leads.
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


// What RefreshFileList walks for one directory, with the live filter.
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


// Plain string order would put "1.bmp" (0x31) ahead of "\dir" (0x5C).
TEST(FileDialogListing, AFileSortingBelowTheDirectoryPrefixStillFollowsIt)
{
	SDialogList	List;

	List.AddDirectory("dir");
	List.AddFile("1.bmp");

	CHECK_EQ(2, List.vNames.size());

	CHECK(Join(List.vNames) == "\\dir|1.bmp");
	CHECK(Kinds(List.vAttributes) == "DF");
}


TEST(FileDialogListing, ASuffixMatchesCaseInsensitively)
{
	const std::vector<std::string>	vFilters = ImageFilters();

	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.bmp", vFilters));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.jpg", vFilters));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("A.BMP", vFilters));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("MiXeD.JpG", vFilters));

	// The name may be exactly the suffix.
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive(".bmp", vFilters));

	// ... and the filter may be the upper-case spelling.
	std::vector<std::string>	vUpper;
	vUpper.push_back(".BMP");

	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.bmp", vUpper));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.BMP", vUpper));

	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("c.txt", vFilters));
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("noext", vFilters));
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("a.bmpx", vFilters));
}


// An empty filter list is Start() never called; an empty filter string
// is Start(""), and is the only way a name with no extension is listed.
TEST(FileDialogListing, AnEmptyFilterListAdmitsNothingAndAnEmptyFilterAdmitsAll)
{
	const std::vector<std::string> vNone;

	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("a.bmp", vNone));

	std::vector<std::string> vEmptyFilter;
	vEmptyFilter.push_back("");

	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("a.bmp", vEmptyFilter));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("noext", vEmptyFilter));
}


// A name shorter than the suffix cannot end with it.
TEST(FileDialogListing, ANameShorterThanTheSuffixIsNotListed)
{
	const std::vector<std::string>	vFilters = ImageFilters();

	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("x", vFilters));
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("mp", vFilters));
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("", vFilters));

	// The empty name against the empty filter is still a match.
	std::vector<std::string>	vEmptyFilter;
	vEmptyFilter.push_back("");

	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive("", vEmptyFilter));
}


// Start() builds each filter entry in a char[30], so an entry of up to
// 29 characters reaches the matcher.
TEST(FileDialogListing, ATwentyFiveCharacterFilterIsMatchedWithoutOverflow)
{
	const std::string	sSuffix = "." + std::string(24, 'a');

	CHECK_EQ(25, sSuffix.size());

	std::vector<std::string>	vLong;
	vLong.push_back(sSuffix);

	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive(
		"picture" + sSuffix, vLong));
	CHECK_EQ(true, Basic::MatchesAnySuffixCaseInsensitive(sSuffix, vLong));

	// Shorter than the filter, and the same length but not an ending.
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive("picture.bmp",
		vLong));
	CHECK_EQ(false, Basic::MatchesAnySuffixCaseInsensitive(
		"." + std::string(23, 'a') + "b", vLong));
}
