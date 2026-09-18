/*-----------------------------------------------------------------------------

	FileDialogListing.cpp

	See FileDialogListing.h.

	2026.09.05

-----------------------------------------------------------------------------*/

#include "FileDialogListing.h"
#include "StringReduction.h"
#include <cstring>
#include <cassert>

std::vector<std::string> Basic::SplitDialogFilters(const char* type)
{
	int nType, i;
	for (i = 0, nType = 1; i < strlen(type); i++)
		if (type[i] == ';') nType++;
	std::vector<std::string> filters;
	if (type != NULL)
	{
		const char* current = type;
		filters.reserve(nType);
		for (i = 0; i < nType; i++)
		{
			const char* end = i == nType - 1 ? &type[strlen(type)] : strstr(current, ";");
			char name[30] = "";
			memcpy(name, current, end - current);
			name[end - current + 1] = '\0';
			filters.insert(filters.begin() + i, name);
			current = end + 1;
		}
	}
	return filters;
}

void Basic::NormalizeDialogSearchPath(char* path)
{
	if (path[strlen(path) - 1] == '\\') path[strlen(path) - 1] = 0;
	if (path[strlen(path) - 1] == '*') path[strlen(path) - 4] = 0;
	strcat(path, "\\*.*");
}

void Basic::ChangeDialogSearchPath(const char* directory, char* path)
{
	assert(directory);
	assert(path);
	if (std::strcmp(directory, "\\..") == 0)
	{
		path[strlen(path) - 4] = 0;
		int counter = 0;
		while (path[strlen(path) - (++counter)] != '\\');
		path[strlen(path) - counter] = 0;
		strcat(path, "\\");
	}
	else if (strlen(path) + strlen(directory) + 1 <= MAX_PATH && directory[1] != '.')
	{
		path[strlen(path) - 4] = 0;
		if (path[strlen(path) - 1] == '\\') path[strlen(path) - 1] = 0;
		strcat(path, directory);
	}
}

std::string Basic::BuildDialogPathLabel(const std::string& path,
		const std::vector<std::string>& filters)
{
	std::string title = path;
	// Stored paths end in "*.*". Preserve the '*' before suffix filters,
	// or remove the whole search pattern when there are no filters.
	if (title.ends_with("*.*"))
		title.resize(title.size() - (filters.empty() ? 3 : 2));
	bool first = true;
	for (const auto& filter : filters)
	{
		if (!first) title += ';';
		title += filter;
		first = false;
	}
	return title;
}

std::string Basic::ShortenDialogLabel(const std::string& label)
{
	std::string name = label;
	ReduceString(name.data(), 38);
	name.resize(std::strlen(name.c_str()));
	return name;
}

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
