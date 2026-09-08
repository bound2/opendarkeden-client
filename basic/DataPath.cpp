/*-----------------------------------------------------------------------------

	DataPath.cpp

	See DataPath.h.

-----------------------------------------------------------------------------*/

#include "DataPath.h"
#include "Platform.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace {

//----------------------------------------------------------------------
// One directory's entry names, listed once. The listing is the cost of
// resolving, and the game opens some paths thousands of times (sprite
// packs, sounds), so it is paid once per directory. The file thread
// loads sprites beside the main thread, hence the mutex.
//
// The cache is keyed by the directory's absolute path: the game changes
// its working directory (Client.cpp, ProfileManager.cpp, the updater),
// and a relative key would hand a later resolve the listing of a
// directory it is no longer in. Both objects are function-local statics
// so a resolve from another translation unit's static initialiser finds
// them constructed.
//----------------------------------------------------------------------
std::mutex&	ListingMutex()
{
	static std::mutex s_Mutex;
	return s_Mutex;
}

std::unordered_map<std::string, std::vector<std::string> >&	Listings()
{
	static std::unordered_map<std::string, std::vector<std::string> > s_Listings;
	return s_Listings;
}

const std::vector<std::string>&	ListingOf(const std::string& sDirectory)
{
	std::error_code Error;

	const std::filesystem::path Directory(sDirectory.empty() ? std::string(".") : sDirectory);
	std::filesystem::path Key = std::filesystem::absolute(Directory, Error);

	if (Error)
		Key = Directory;

	const std::string sKey = Key.lexically_normal().generic_string();

	std::lock_guard<std::mutex> Lock(ListingMutex());

	std::unordered_map<std::string, std::vector<std::string> >& mListings = Listings();

	auto iFound = mListings.find(sKey);

	if (iFound != mListings.end())
		return iFound->second;

	std::vector<std::string>& vNames = mListings[sKey];

	std::filesystem::directory_iterator iEntry(Directory, Error);

	if (Error)
		return vNames;

	const std::filesystem::directory_iterator iEnd;

	while (iEntry != iEnd)
	{
		vNames.push_back(iEntry->path().filename().generic_string());

		iEntry.increment(Error);

		if (Error)
			break;
	}

	return vNames;
}

//----------------------------------------------------------------------
// ASCII letters only. The data's names are ASCII; folding through the
// C locale's tolower would let a CP949 lead byte match the wrong entry
// once anything calls setlocale.
//----------------------------------------------------------------------
char	FoldAscii(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

bool	EqualsIgnoringCase(const std::string& sA, const std::string& sB)
{
	if (sA.size() != sB.size())
		return false;

	for (size_t i = 0; i < sA.size(); i++)
	{
		if (FoldAscii(sA[i]) != FoldAscii(sB[i]))
			return false;
	}

	return true;
}

} // namespace

std::string	Basic::ResolveDataPath(std::string_view sPath)
{
	if (sPath.empty())
		return std::string();

	std::string sFolded(sPath);

	for (size_t i = 0; i < sFolded.size(); i++)
	{
		if (sFolded[i] == '\\')
			sFolded[i] = '/';
	}

	// The root ("/", "C:/") is kept as given; the walk is over what
	// follows it.
	const std::filesystem::path Folded(sFolded);

	std::string sResolved = Folded.root_path().generic_string();
	std::string sCurrent  = sResolved;		// the directory the next component is looked up in
	bool bResolving = true;

	for (const std::filesystem::path& Component : Folded.relative_path())
	{
		const std::string sComponent = Component.generic_string();

		if (sComponent.empty())
			continue;

		std::string sChosen = sComponent;

		if (bResolving && sComponent != "." && sComponent != "..")
		{
			const std::vector<std::string>& vNames = ListingOf(sCurrent);

			bool bExact = false;
			const std::string* pCaseMatch = NULL;

			for (size_t i = 0; i < vNames.size(); i++)
			{
				if (vNames[i] == sComponent)
				{
					bExact = true;
					break;
				}

				if (pCaseMatch == NULL && EqualsIgnoringCase(vNames[i], sComponent))
					pCaseMatch = &vNames[i];
			}

			if (bExact)
			{
			}
			else if (pCaseMatch != NULL)
			{
				sChosen = *pCaseMatch;
			}
			else
			{
				// Not in the listing under any case. A name the listing
				// does not show can still exist (a Windows short name, a
				// mount); otherwise the path is simply not there, and the
				// rest is appended as given.
				std::error_code Error;

				const std::filesystem::path Candidate =
					std::filesystem::path(sCurrent.empty() ? std::string(".") : sCurrent) / sComponent;

				if (!std::filesystem::exists(Candidate, Error))
					bResolving = false;
			}
		}

		if (!sResolved.empty() && sResolved.back() != '/')
			sResolved += '/';

		sResolved += sChosen;
		sCurrent   = sResolved;
	}

	// A trailing separator on the input is kept: the caller may be
	// naming a directory.
	if (sFolded.back() == '/' && (sResolved.empty() || sResolved.back() != '/'))
		sResolved += '/';

	return sResolved;
}

std::string	Basic::NormalizeDataPath(std::string_view sPath)
{
#ifdef PLATFORM_WINDOWS
	return std::string(sPath);
#else
	return ResolveDataPath(sPath);
#endif
}
