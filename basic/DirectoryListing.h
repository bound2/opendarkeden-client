/*-----------------------------------------------------------------------------

	DirectoryListing.h

	Directory enumeration on std::filesystem, replacing the _findfirst /
	_findnext / _findclose walks that were spread through the client. The
	caller gets a snapshot vector and owns no search handle.

	Kept from _findfirst / FindFirstFileA:

	  * '*' matches zero or more characters, dots included; '?' exactly one.
	  * Matching is case-insensitive.
	  * Result order is the NTFS index order: case-insensitive ordinal.
	  * Directories are enumerated when the caller asks for them.

	Deliberately not kept:

	  * 8.3 short names are not matched against.
	  * DOS_DOT: '.' is a literal here, so "*.*" loses dotless names.
	  * "." and ".." are never in the result.
	  * DOS_QM: a trailing '?' does not match zero characters.
	  * Folding and ordering are byte-wise and ASCII only, not UTF-16.
	  * A snapshot, not a live walk.
	  * All or nothing on a mid-walk error: false, with an empty result.

	Nothing here throws; every filesystem call goes through its
	std::error_code overload.

	2026.09.05

-----------------------------------------------------------------------------*/

#ifndef __DIRECTORY_LISTING_H__
#define __DIRECTORY_LISTING_H__

#include <string>
#include <vector>

namespace Basic {

/*-----------------------------------------------------------------------------
  One enumerated entry. The name carries no directory part, and is in the
  narrow encoding the rest of the client uses for paths.
-----------------------------------------------------------------------------*/
struct SDirectoryEntry
{
	std::string	sName;
	bool		bIsDirectory;
};

/*-----------------------------------------------------------------------------
  Whether subdirectories join the result. Ask for
  LIST_FILES_AND_DIRECTORIES only when the loop body could act on a
  directory.
-----------------------------------------------------------------------------*/
enum EListFilter
{
	LIST_FILES_ONLY,
	LIST_FILES_AND_DIRECTORIES
};

/*-----------------------------------------------------------------------------
  DOS-style wildcard match, case-insensitive over ASCII. A NULL name or
  pattern matches nothing. An empty pattern matches only an empty name,
  which no directory entry has.
-----------------------------------------------------------------------------*/
bool	MatchesWildcard(const char* pName, const char* pPattern);

/*-----------------------------------------------------------------------------
  Lists the entries of pDirectory whose name matches pPattern, sorted as
  described above. Returns false - with vEntries empty - when the
  directory does not exist or cannot be enumerated; returns true with an
  empty vector when it is readable but nothing matches.
-----------------------------------------------------------------------------*/
bool	ListDirectory(const char* pDirectory, const char* pPattern,
		std::vector<SDirectoryEntry>& vEntries,
		EListFilter eFilter = LIST_FILES_ONLY);

} // namespace Basic

#endif
