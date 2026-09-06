/*-----------------------------------------------------------------------------

	FileDialogListing.h

	The list building of C_VS_UI_FILE_DIALOG::RefreshFileList - the
	profile-picture file dialog, live through MODE_PROFILE_SELECT
	(Client/UIMessageManager.cpp) - lifted out of VS_UI so that it has a
	test path (docs/RESTRUCTURING.md task 3.1, "moved, then fixed").

	Nothing here enumerates a directory; basic/DirectoryListing.h does
	that. These two functions are the part that runs over the result: the
	filter that decides whether a file belongs in the dialog at all, and
	the insertion sort that puts it in the two parallel vectors the dialog
	draws from.

	The list the dialog holds is two vectors read by the same index -
	m_vs_file_list, whose directory entries carry a leading '\', and
	m_vs_file_list_attr, which every consumer tests only for
	FILE_ATTRIBUTE_DIRECTORY. Both are kept in step by
	InsertDialogEntry(), which is the only reason they are passed
	together.

	Order: every directory first, sorted by byte-wise std::string
	comparison of the '\'-prefixed name, then every file, sorted the same
	way.

	This header is in basic/ for the reason DebugLog.h and SafeFormat.h
	are: the caller is in VS_UI, no test binary links VS_UI, and basic is
	the one library every target already links.

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
  over ASCII. The suffixes are the dialog's m_filter, which
  C_VS_UI_FILE_DIALOG::Start() fills by splitting its type string on ';'
  - ".bmp;.jpg" gives two entries, ".bmp" and ".jpg".

  Defined for every name and every suffix. A suffix LONGER than the name
  cannot be one of its endings, so it does not match; an empty suffix
  list matches nothing; an empty suffix string matches everything, which
  is what Start("") produces.

  Neither length is bounded here. Start() builds each entry in a
  char[30] and this used to copy it into a char[20] with strcpy(), so a
  filter of 20 characters or more overran a stack buffer, and the
  comparison indexed the name at size() - j - 1, which wraps when the
  name is shorter than the suffix. Both are gone: the suffix is compared
  where it lies and the length test comes first.
-----------------------------------------------------------------------------*/
bool	MatchesAnySuffixCaseInsensitive(const std::string& sName,
		const std::vector<std::string>& vSuffixes);

/*-----------------------------------------------------------------------------
  Inserts one entry into the dialog's parallel vectors, in the order
  above. A directory arrives with its '\' already on the front of sName
  and with FILE_ATTRIBUTE_DIRECTORY set in dwAttributes; a file arrives
  with a bare name.

  One entry goes in per call, at the first position that sorts after it.
  The dialog's own version tested a stale counter after its insertion
  loop instead - see the fix commit - which both dropped files and
  inserted one twice.
-----------------------------------------------------------------------------*/
void	InsertDialogEntry(std::vector<std::string>& vNames,
		std::vector<DWORD>& vAttributes, const std::string& sName,
		DWORD dwAttributes);

} // namespace Basic

#endif
