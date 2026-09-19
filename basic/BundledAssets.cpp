/*-----------------------------------------------------------------------------

	BundledAssets.cpp

	See BundledAssets.h.

-----------------------------------------------------------------------------*/

#include "BundledAssets.h"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace Basic {

const char* const BUNDLED_ASSETS_MARKER = "darkeden-assets.version";

namespace {

struct SManifestEntry
{
	bool		bDirectory;
	std::string	sPath;
	unsigned long long	ullSize;
};

std::string	JoinSource(const std::string& sRoot, const std::string& sPath)
{
	if (sRoot.empty())
		return sPath;
	return sRoot + "/" + sPath;
}

// Reads a whole source file through SDL_RWFromFile (the one way into an
// Android asset). Returns false when it cannot be opened.
bool	ReadSource(const std::string& sSource, std::string& sOut)
{
	SDL_RWops* pOps = SDL_RWFromFile(sSource.c_str(), "rb");
	if (pOps == NULL)
		return false;

	char szBuffer[16384];
	for (;;)
	{
		const size_t nRead = SDL_RWread(pOps, szBuffer, 1, sizeof(szBuffer));
		if (nRead == 0)
			break;
		sOut.append(szBuffer, nRead);
	}
	SDL_RWclose(pOps);
	return true;
}

// The marker's content, trimmed of its line ending; empty when absent.
std::string	ReadMarker(const std::filesystem::path& Marker)
{
	std::ifstream In(Marker, std::ios::binary);
	std::string sId;
	if (In)
		std::getline(In, sId);
	while (!sId.empty() && (sId.back() == '\r' || sId.back() == '\n' || sId.back() == ' '))
		sId.pop_back();
	return sId;
}

// A path is rejected when it could reach outside the destination: an
// absolute path, a drive letter, or a ".." component.
bool	IsSafeRelativePath(const std::string& sPath)
{
	if (sPath.empty() || sPath[0] == '/' || sPath[0] == '\\')
		return false;
	if (sPath.size() >= 2 && sPath[1] == ':')
		return false;

	size_t nStart = 0;
	while (nStart <= sPath.size())
	{
		size_t nEnd = sPath.find_first_of("/\\", nStart);
		if (nEnd == std::string::npos)
			nEnd = sPath.size();
		if (sPath.compare(nStart, nEnd - nStart, "..") == 0)
			return false;
		nStart = nEnd + 1;
	}
	return true;
}

// Parses the manifest text. Returns false with sError set on the first
// malformed line.
bool	ParseManifest(const std::string& sText, std::string& sVersion,
	std::vector<SManifestEntry>& vEntries, std::string& sError)
{
	size_t nPos = 0;
	size_t nLine = 0;
	bool bHeader = false;

	while (nPos < sText.size())
	{
		size_t nEnd = sText.find('\n', nPos);
		if (nEnd == std::string::npos)
			nEnd = sText.size();
		std::string sLine = sText.substr(nPos, nEnd - nPos);
		nPos = nEnd + 1;
		nLine++;

		if (!sLine.empty() && sLine.back() == '\r')
			sLine.pop_back();
		if (sLine.empty())
			continue;

		if (!bHeader)
		{
			if (sLine != "darkeden-assets 1")
			{
				sError = "manifest line 1: not a darkeden-assets 1 manifest";
				return false;
			}
			bHeader = true;
			continue;
		}

		if (sLine.compare(0, 8, "version ") == 0)
		{
			sVersion = sLine.substr(8);
			if (sVersion.empty())
			{
				sError = "manifest line " + std::to_string(nLine) + ": empty version";
				return false;
			}
			continue;
		}

		if (sLine.compare(0, 4, "dir ") == 0)
		{
			SManifestEntry Entry;
			Entry.bDirectory = true;
			Entry.sPath = sLine.substr(4);
			Entry.ullSize = 0;
			if (!IsSafeRelativePath(Entry.sPath))
			{
				sError = "manifest line " + std::to_string(nLine) + ": unsafe path " + Entry.sPath;
				return false;
			}
			vEntries.push_back(Entry);
			continue;
		}

		if (sLine.compare(0, 5, "file ") == 0)
		{
			const size_t nSpace = sLine.find(' ', 5);
			if (nSpace == std::string::npos || nSpace == 5 || nSpace + 1 >= sLine.size())
			{
				sError = "manifest line " + std::to_string(nLine) + ": expected 'file <size> <path>'";
				return false;
			}
			const std::string sSize = sLine.substr(5, nSpace - 5);
			char* pEnd = NULL;
			const unsigned long long ullSize = strtoull(sSize.c_str(), &pEnd, 10);
			if (pEnd == NULL || *pEnd != '\0' || sSize.find_first_not_of("0123456789") != std::string::npos)
			{
				sError = "manifest line " + std::to_string(nLine) + ": bad size " + sSize;
				return false;
			}
			SManifestEntry Entry;
			Entry.bDirectory = false;
			Entry.sPath = sLine.substr(nSpace + 1);
			Entry.ullSize = ullSize;
			if (!IsSafeRelativePath(Entry.sPath))
			{
				sError = "manifest line " + std::to_string(nLine) + ": unsafe path " + Entry.sPath;
				return false;
			}
			vEntries.push_back(Entry);
			continue;
		}

		sError = "manifest line " + std::to_string(nLine) + ": unknown entry";
		return false;
	}

	if (!bHeader)
	{
		sError = "manifest is empty";
		return false;
	}
	if (sVersion.empty())
	{
		sError = "manifest names no version";
		return false;
	}
	return true;
}

// Copies one source into Dest, verifying the byte count against the
// manifest. Returns false with sError set.
bool	CopyFile(const std::string& sSource, const std::filesystem::path& Dest,
	unsigned long long ullExpected, std::string& sError)
{
	SDL_RWops* pOps = SDL_RWFromFile(sSource.c_str(), "rb");
	if (pOps == NULL)
	{
		sError = "cannot open " + sSource + ": " + SDL_GetError();
		return false;
	}

	FILE* pOut = fopen(Dest.string().c_str(), "wb");
	if (pOut == NULL)
	{
		SDL_RWclose(pOps);
		sError = "cannot create " + Dest.string();
		return false;
	}

	static char szBuffer[65536];
	unsigned long long ullWritten = 0;
	bool bOk = true;
	for (;;)
	{
		const size_t nRead = SDL_RWread(pOps, szBuffer, 1, sizeof(szBuffer));
		if (nRead == 0)
			break;
		if (fwrite(szBuffer, 1, nRead, pOut) != nRead)
		{
			sError = "short write to " + Dest.string();
			bOk = false;
			break;
		}
		ullWritten += nRead;
	}
	SDL_RWclose(pOps);
	if (fclose(pOut) != 0 && bOk)
	{
		sError = "cannot close " + Dest.string();
		bOk = false;
	}

	if (bOk && ullWritten != ullExpected)
	{
		sError = Dest.string() + ": wrote " + std::to_string(ullWritten)
			+ " bytes, manifest says " + std::to_string(ullExpected);
		bOk = false;
	}
	return bOk;
}

} // namespace

SBundledAssetsResult InstallBundledAssets(
	const std::string& sSourceRoot,
	const std::string& sManifestName,
	const std::string& sDestDir,
	BundledAssetsProgressFn pfnProgress,
	void* pUser)
{
	SBundledAssetsResult Result;

	std::string sText;
	const std::string sManifestSource = JoinSource(sSourceRoot, sManifestName);
	if (!ReadSource(sManifestSource, sText))
	{
		Result.sError = "cannot open the manifest " + sManifestSource + ": " + SDL_GetError();
		return Result;
	}

	std::vector<SManifestEntry> vEntries;
	if (!ParseManifest(sText, Result.sVersion, vEntries, Result.sError))
		return Result;

	const std::filesystem::path Dest(sDestDir);
	const std::filesystem::path Marker = Dest / BUNDLED_ASSETS_MARKER;

	if (ReadMarker(Marker) == Result.sVersion)
	{
		Result.bInstalled = true;
		Result.bAlreadyCurrent = true;
		return Result;
	}

	std::error_code Error;
	std::filesystem::create_directories(Dest, Error);
	if (Error)
	{
		Result.sError = "cannot create " + sDestDir + ": " + Error.message();
		return Result;
	}

	// A stale marker from an older version is removed first, so a copy
	// that stops half way leaves no claim behind.
	std::filesystem::remove(Marker, Error);

	size_t nTotalFiles = 0;
	for (size_t i = 0; i < vEntries.size(); i++)
		if (!vEntries[i].bDirectory)
			nTotalFiles++;

	size_t nDone = 0;
	for (size_t i = 0; i < vEntries.size(); i++)
	{
		const SManifestEntry& Entry = vEntries[i];
		const std::filesystem::path Target = Dest / Entry.sPath;

		if (Entry.bDirectory)
		{
			Error.clear();
			const bool bCreated = std::filesystem::create_directories(Target, Error);
			if (Error)
			{
				Result.sError = "cannot create " + Target.string() + ": " + Error.message();
				return Result;
			}
			if (bCreated)
				Result.nDirectories++;
			continue;
		}

		if (pfnProgress != nullptr)
			pfnProgress(nDone, nTotalFiles, Entry.sPath.c_str(), pUser);

		Error.clear();
		std::filesystem::create_directories(Target.parent_path(), Error);
		if (Error)
		{
			Result.sError = "cannot create " + Target.parent_path().string() + ": " + Error.message();
			return Result;
		}

		if (!CopyFile(JoinSource(sSourceRoot, Entry.sPath), Target, Entry.ullSize, Result.sError))
			return Result;

		Result.nFiles++;
		nDone++;
	}

	{
		std::ofstream Out(Marker, std::ios::binary | std::ios::trunc);
		Out << Result.sVersion << "\n";
		if (!Out)
		{
			Result.sError = "cannot write " + Marker.string();
			return Result;
		}
	}

	Result.bInstalled = true;
	return Result;
}

} // namespace Basic
