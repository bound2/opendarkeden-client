/*-----------------------------------------------------------------------------

	FileDialogListing.cpp

	See FileDialogListing.h. Both functions were moved out of
	C_VS_UI_FILE_DIALOG::RefreshFileList as they stood by the commit
	before this one, so that the two defects they carried could be
	pinned before being fixed here.

	2026.09.05

-----------------------------------------------------------------------------*/

#include "FileDialogListing.h"

namespace {

/*-----------------------------------------------------------------------------
  C_VS_UI_FILE_DIALOG::Upperchar, moved verbatim from
  VS_UI/src/header/VS_UI_ExtraDialog.h. Deliberately not toupper(): it
  answers to the active C locale and is undefined for a negative char,
  and every byte of a CP949 name outside ASCII is negative here. Folding
  only 'a'-'z' leaves those bytes alone, which is what the dialog did.
-----------------------------------------------------------------------------*/
inline char	UpperChar(char str)
{
	if(!(str>='a'&&str<='z')) return str;
	else return str-32;
}

} // anonymous namespace

/*-----------------------------------------------------------------------------
  Suffix filter

  The suffix is read where it lies. The dialog copied it into a
  char[20] with strcpy() first, out of the char[30] Start() fills, so a
  filter of 20 characters or more wrote past the buffer; and it compared
  all strlen(suffix) bytes whatever the name's length, so a name shorter
  than the suffix indexed std::string::operator[] at a wrapped
  size() - j - 1. The length test below is what makes the comparison
  well defined: a suffix longer than the name is not one of its endings.
-----------------------------------------------------------------------------*/
bool
Basic::MatchesAnySuffixCaseInsensitive(const std::string& sName,
		const std::vector<std::string>& vSuffixes)
{
	for(size_t i = 0; i < vSuffixes.size(); i++)
	{
		const std::string&	sSuffix = vSuffixes[i];

		if(sSuffix.size() > sName.size()) continue;

		bool	bMatches = true;

		for(size_t j = 0; bMatches && j < sSuffix.size(); j++)
		{
			if(UpperChar(sName[sName.size() - j - 1])
				!= UpperChar(sSuffix[sSuffix.size() - j - 1]))
				bMatches = false;
		}

		if(bMatches) return true;
	}

	return false;
}

/*-----------------------------------------------------------------------------
  Insertion sort into the dialog's parallel vectors

  One position is found and one entry goes in, whichever branch it takes.
  The dialog wrote the two branches out separately and, in the file one,
  declared the loop's counter inside the for statement, where it shadowed
  an `int i` the filter loop above had left at m_filter.size(); the
  `if(i == m_vs_file_list.size())` that followed therefore compared the
  FILTER count against the list size. The listing arrives in the
  case-folded order basic/DirectoryListing sorts it in, while the loop
  compares byte-wise, so the loop broke for a file only where the two
  orders disagree (a lowercase initial followed by an uppercase one, say)
  and a file was otherwise appended only when the filter count happened
  to equal the list size. With the live ".bmp;.jpg" filter a directory
  typically showed one picture, sometimes a handful in the wrong-looking
  order, and none at all in a directory with no subdirectory. The same
  stale test also ran after an insert that HAD found a place, and
  inserted the file a second time when the count equalled the size the
  insert had just grown to; the dialog's own arrival order can never
  produce that state (a file only ever enters a list already longer
  than the count), so that half was reachable in the function and not
  through the dialog.

  A directory's position is the first entry that sorts after it or is a
  file; a file's is the first FILE that sorts after it, which is what
  keeps every directory ahead of every file even when a file name begins
  below '\' (0x5C).
-----------------------------------------------------------------------------*/
void
Basic::InsertDialogEntry(std::vector<std::string>& vNames,
		std::vector<DWORD>& vAttributes, const std::string& sName,
		DWORD dwAttributes)
{
	const bool	bIsDirectory = (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	size_t		uAt;

	for(uAt = 0; uAt < vNames.size(); uAt++)
	{
		if(bIsDirectory)
		{
			if(vNames[uAt] > sName || vNames[uAt][0] != '\\') break;
		}
		else
		{
			if(vNames[uAt] > sName && vNames[uAt][0] != '\\') break;
		}
	}

	vNames.insert(vNames.begin() + uAt, sName);
	vAttributes.insert(vAttributes.begin() + uAt, dwAttributes);
}
