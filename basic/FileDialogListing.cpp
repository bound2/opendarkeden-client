/*-----------------------------------------------------------------------------

	FileDialogListing.cpp

	See FileDialogListing.h. Both functions are moved out of
	C_VS_UI_FILE_DIALOG::RefreshFileList as they stood, defects included:
	the move is meant to be invisible to the dialog, and the fix that
	follows it is the commit that changes what the dialog shows.

	2026.09.05

-----------------------------------------------------------------------------*/

#include "FileDialogListing.h"

#include <string.h>

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
-----------------------------------------------------------------------------*/
bool
Basic::MatchesAnySuffixCaseInsensitive(const std::string& sName,
		const std::vector<std::string>& vSuffixes)
{
	char	szfile[20];
	int		i,j;
	bool	findflag=false,fAddFile=false;

	for(i=0;i<vSuffixes.size();i++)
	{
		strcpy(szfile,vSuffixes[i].c_str());

		findflag=false;
		for(j = 0; j < strlen(szfile); j++)
		{
			if(UpperChar(sName[sName.size() - j-1])
				!= UpperChar(szfile[strlen(szfile) - j-1]))
				findflag=true;
		}
		if(!findflag) fAddFile=true;
	}

	return fAddFile;
}

/*-----------------------------------------------------------------------------
  Insertion sort into the dialog's parallel vectors
-----------------------------------------------------------------------------*/
void
Basic::InsertDialogEntry(std::vector<std::string>& vNames,
		std::vector<DWORD>& vAttributes, const std::string& sName,
		DWORD dwAttributes, size_t uFilterCount)
{
	if (dwAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		// A directory goes before the first entry that either sorts
		// after it or is a file, which is what keeps every directory
		// ahead of every file. i is declared in the scope that tests
		// it, so the test below is the loop's own counter and the
		// branch is correct as it stands.
		int i;
		for(i = 0; i < vNames.size(); i++)
		{
			if(vNames[i] > sName || vNames[i][0] != '\\')
			{
				vNames.insert(vNames.begin() + i, sName);
				vAttributes.insert(vAttributes.begin() + i, dwAttributes);
				break;
			}
		}
		if(i == vNames.size())
		{
			vNames.insert(vNames.begin() + i, sName);
			vAttributes.insert(vAttributes.begin() + i, dwAttributes);
		}
	}
	else
	{
		// The file branch, moved with its defect. In the dialog the
		// loop below declared its own `int i` and so shadowed the
		// `int i` the filter loop had left at m_filter.size(); the
		// append test after it therefore read the FILTER count, not
		// the loop counter. The two are spelled apart here - k is the
		// loop's, i is the value the shadowed name really held - so
		// that the defect is legible rather than hidden in a scope.
		//
		// Consequences, both reachable: a file whose place the loop
		// never found is appended only when the filter count happens
		// to equal the list size, and a file the loop DID place is
		// inserted a second time when the count equals the list size
		// it has just grown to.
		const int	i = (int)uFilterCount;

//		vNames.push_back(sName);
//		vAttributes.push_back(dwAttributes);

		for(int k = 0; k < vNames.size(); k++)
		{
			if(vNames[k] > sName && vNames[k][0] != '\\')
			{
				vNames.insert(vNames.begin() + k, sName);
				vAttributes.insert(vAttributes.begin() + k, dwAttributes);
				break;
			}
		}
		if(i == vNames.size())
		{
			vNames.insert(vNames.begin() + i, sName);
			vAttributes.insert(vAttributes.begin() + i, dwAttributes);
		}
	}
}
