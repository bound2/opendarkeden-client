/*-----------------------------------------------------------------------------

	DirectoryListing.cpp

	See DirectoryListing.h for the semantics this reproduces from
	_findfirst and the ones it deliberately does not.

	2026.09.05

-----------------------------------------------------------------------------*/

#include "DirectoryListing.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace {

/*-----------------------------------------------------------------------------
  ASCII case fold. Deliberately not toupper(): toupper() answers to the
  active C locale, which would make matching and ordering depend on
  whatever the program set up, and it is undefined for a negative char.
-----------------------------------------------------------------------------*/
inline unsigned char	UpperAscii(char c)
{
	const unsigned char uc = (unsigned char)c;

	return (uc >= 'a' && uc <= 'z') ? (unsigned char)(uc - ('a' - 'A')) : uc;
}

/*-----------------------------------------------------------------------------
  Case-insensitive ordinal ordering, which is how NTFS orders its
  directory index and therefore the order _findnext walked.
-----------------------------------------------------------------------------*/
bool	LessCaseInsensitive(const Basic::SDirectoryEntry& Left, const Basic::SDirectoryEntry& Right)
{
	const std::string&	sLeft = Left.sName;
	const std::string&	sRight = Right.sName;
	const size_t		uShared = (sLeft.size() < sRight.size()) ? sLeft.size() : sRight.size();

	for (size_t i=0; i<uShared; i++)
	{
		const unsigned char cLeft = UpperAscii(sLeft[i]);
		const unsigned char cRight = UpperAscii(sRight[i]);

		if (cLeft != cRight)
		{
			return cLeft < cRight;
		}
	}

	return sLeft.size() < sRight.size();
}

} // anonymous namespace

/*-----------------------------------------------------------------------------
  Wildcard match
-----------------------------------------------------------------------------*/
// Iterative backtracking: pStarPattern/pStarName remember the most recent
// '*' so a failed tail can retry with one more character consumed by it.
// That keeps the worst case quadratic instead of exponential and, unlike
// a recursive matcher, cannot overflow the stack on a hostile pattern.
bool
Basic::MatchesWildcard(const char* pName, const char* pPattern)
{
	if (pName == NULL || pPattern == NULL)
	{
		return false;
	}

	const char*	pStarPattern = NULL;
	const char*	pStarName = NULL;

	while (*pName != '\0')
	{
		if (*pPattern == '*')
		{
			// Remember the star, then try it against zero characters.
			// Tested before the literal comparison below so that a '*'
			// in a NAME is never mistaken for a pattern star's twin.
			pStarPattern = pPattern;
			pStarName = pName;
			pPattern ++;
		}
		else if (*pPattern == '?'
			|| (*pPattern != '\0' && UpperAscii(*pPattern) == UpperAscii(*pName)))
		{
			pPattern ++;
			pName ++;
		}
		else if (pStarPattern != NULL)
		{
			// The tail failed - give the star one more character.
			pStarName ++;
			pPattern = pStarPattern + 1;
			pName = pStarName;
		}
		else
		{
			return false;
		}
	}

	// Trailing stars may still match the empty remainder of the name.
	while (*pPattern == '*')
	{
		pPattern ++;
	}

	return *pPattern == '\0';
}

/*-----------------------------------------------------------------------------
  Directory listing
-----------------------------------------------------------------------------*/
bool
Basic::ListDirectory(const char* pDirectory, const char* pPattern,
		std::vector<SDirectoryEntry>& vEntries, EListFilter eFilter)
{
	vEntries.clear();

	if (pDirectory == NULL || pPattern == NULL)
	{
		return false;
	}

	try
	{
		std::error_code	Error;

		std::filesystem::directory_iterator	iEntry(std::filesystem::path(pDirectory), Error);

		if (Error)
		{
			return false;
		}

		const std::filesystem::directory_iterator	iEnd;

		while (iEntry != iEnd)
		{
			// The error_code overload, so a hostile entry - a dangling
			// symlink, say - throws nothing. is_directory() follows the
			// link, so an entry whose target cannot be classified is
			// left as an ordinary file rather than losing the listing.
			// That is NOT how _findfirst reported it: it read the
			// reparse point's own attributes out of the directory index
			// and set the directory bit on a dangling junction too.
			std::error_code	TypeError;

			const bool	bIsDirectory = iEntry->is_directory(TypeError);

			if (!bIsDirectory || eFilter == LIST_FILES_AND_DIRECTORIES)
			{
				// path::string() converts to the same narrow encoding
				// the ANSI _findfirst reported names in, so a migrated
				// caller keeps building the same paths it always did.
				const std::string	sName = iEntry->path().filename().string();

				if (MatchesWildcard(sName.c_str(), pPattern))
				{
					SDirectoryEntry	Entry;

					Entry.sName = sName;
					Entry.bIsDirectory = bIsDirectory;

					vEntries.push_back(Entry);
				}
			}

			iEntry.increment(Error);

			if (Error)
			{
				vEntries.clear();
				return false;
			}
		}
	}
	catch (...)
	{
		// path::string() is the one call above that can still throw, on a
		// name the narrow encoding cannot represent. A listing is never
		// worth an exception escaping into the caller's loop.
		vEntries.clear();
		return false;
	}

	// stable_sort so that names the ASCII fold cannot tell apart - which a
	// case-sensitive filesystem allows and Windows does not - still come
	// back in a fixed order.
	std::stable_sort(vEntries.begin(), vEntries.end(), LessCaseInsensitive);

	return true;
}
