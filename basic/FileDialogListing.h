/*-----------------------------------------------------------------------------

	FileDialogListing.h

	The suffix filter and the insertion sort C_VS_UI_FILE_DIALOG builds
	its list with. Directory entries carry a leading '\' and sort ahead
	of every file; both groups are sorted byte-wise.

	2026.09.05

-----------------------------------------------------------------------------*/

#ifndef __FILE_DIALOG_LISTING_H__
#define __FILE_DIALOG_LISTING_H__

#include "Platform.h"

#include <string>
#include <vector>

namespace Basic {

/*-----------------------------------------------------------------------------
  Whether sName ends with any of vSuffixes, compared case-insensitively
  over ASCII. An empty suffix list matches nothing; an empty suffix
  string matches everything.
-----------------------------------------------------------------------------*/
bool	MatchesAnySuffixCaseInsensitive(const std::string& sName,
		const std::vector<std::string>& vSuffixes);

/*-----------------------------------------------------------------------------
  Inserts one entry into the dialog's parallel vectors, in the order
  above. A directory arrives with its '\' already on the front of sName
  and with FILE_ATTRIBUTE_DIRECTORY set in dwAttributes.
-----------------------------------------------------------------------------*/
void	InsertDialogEntry(std::vector<std::string>& vNames,
		std::vector<DWORD>& vAttributes, const std::string& sName,
		DWORD dwAttributes);

} // namespace Basic

#endif
