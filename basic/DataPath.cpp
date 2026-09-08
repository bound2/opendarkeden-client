/*-----------------------------------------------------------------------------

	DataPath.cpp

	See DataPath.h.

-----------------------------------------------------------------------------*/

#include "DataPath.h"

#include <cctype>
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
//----------------------------------------------------------------------
std::mutex									s_Mutex;
std::unordered_map<std::string, std::vector<std::string> >	s_Listings;

const std::vector<std::string>&	ListingOf(const std::string& sDirectory)
{
	std::lock_guard<std::mutex> Lock(s_Mutex);

	auto iFound = s_Listings.find(sDirectory);

	if (iFound != s_Listings.end())
		return iFound->second;

	std::vector<std::string>& vNames = s_Listings[sDirectory];

	std::error_code Error;

	const std::filesystem::path Directory(sDirectory.empty() ? std::string(".") : sDirectory);

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

bool	EqualsIgnoringCase(const std::string& sA, const std::string& sB)
{
	if (sA.size() != sB.size())
		return false;

	for (size_t i = 0; i < sA.size(); i++)
	{
		if (std::tolower((unsigned char)sA[i]) != std::tolower((unsigned char)sB[i]))
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
#ifdef _WIN32
	return std::string(sPath);
#else
	return ResolveDataPath(sPath);
#endif
}
