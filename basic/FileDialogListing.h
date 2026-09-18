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

// Semicolon-separated suffixes: null means none; empty suffixes match all.
std::vector<std::string> SplitDialogFilters(const char* type);
// Search state always ends with a separator and "*.*"; empty input means '.'.
void NormalizeDialogSearchPath(std::string& path);
// Directory entries have the UI's leading '\\' marker. '..' stays inside
// drive, native, and network-share roots; other entries are single names.
void ChangeDialogSearchPath(const char* directory, std::string& path);
std::string DialogDirectoryPath(std::string path);

struct DialogDirectories
{
	std::vector<std::string> paths;
	size_t current = 0;
};

// Always supplies a path and valid current index, even without drive letters.
DialogDirectories MakeDialogDirectories(DWORD driveMask, const std::string& currentPath);

// Replace a trailing search pattern with the configured suffix display;
// paths without that pattern remain intact, including empty/short input.
std::string BuildDialogPathLabel(const std::string& path,
		const std::vector<std::string>& filters);
// Return an owned label of at most 38 bytes, with the legacy ellipsis.
std::string ShortenDialogLabel(const std::string& label);

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
