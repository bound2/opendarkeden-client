/*-----------------------------------------------------------------------------

	FileDialogListing.cpp

	See FileDialogListing.h.

	2026.09.05

-----------------------------------------------------------------------------*/

#include "FileDialogListing.h"

namespace {

// Deliberately not toupper(), which is undefined for a negative char:
// every CP949 byte outside ASCII is negative here and must be left alone.
inline char	UpperChar(char str)
{
	if(!(str>='a'&&str<='z')) return str;
	else return str-32;
}

} // anonymous namespace

/*-----------------------------------------------------------------------------
  Suffix filter
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
