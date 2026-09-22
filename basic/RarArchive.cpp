/*
UnRAR source code may be used in any software to handle
RAR archives without limitations free of charge, but cannot be
used to develop RAR (WinRAR) compatible archiver and to
re-create RAR compression algorithm, which is proprietary.
Distribution of modified UnRAR source code in separate form
or as a part of other software is permitted, provided that
full text of this paragraph, starting from "UnRAR source code"
words, is included in license, or in documentation if license
is not available, and in source code comments of resulting package.
*/
#include "RarArchive.h"
#include "DataPath.h"
#include "TextUtf8.h"
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <memory>
#include <string_view>

// Keep UnRAR's platform types out of Platform.h and every public header.
#ifdef _WIN32
#include <windows.h>
#endif
#include "raros.hpp"
#include "rartypes.hpp"
#include "unicode.hpp"
#include "dll.hpp"

namespace {
constexpr size_t MaxWorkBytes = 256 * 1024 * 1024;
constexpr size_t MaxHeaders = 100000;
constexpr unsigned MaxDictionaryKiB = 64 * 1024;
constexpr size_t MaxNameBytes = 16 * 1024 * 1024;

// UnRAR's error handler and several platform helpers use process-global state.
std::mutex archiveMutex;

char Fold(char value)
{
	if (value == '\\') return '/';
	return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

bool Matches(std::string_view name, std::string_view pattern)
{
	// Iterative wildcard matching with one remembered star; no recursive stack.
	size_t n = 0, p = 0, star = std::string_view::npos, retry = 0;
	while (n < name.size()) {
		if (p < pattern.size() && (pattern[p] == '?' || Fold(pattern[p]) == Fold(name[n]))) {
			++n; ++p;
		} else if (p < pattern.size() && pattern[p] == '*') {
			star = p++; retry = n;
		} else if (star != std::string_view::npos) {
			p = star + 1; n = ++retry;
		} else return false;
	}
	while (p < pattern.size() && pattern[p] == '*') ++p;
	return p == pattern.size();
}

bool SafeMember(const std::string& name)
{
	if (name.empty() || name[0] == '/' || name.find_first_of(":\0", 0, 2) != std::string::npos ||
		!TextSystem::IsValidUtf8(name.data(), name.size())) return false;
	size_t start = 0;
	do {
		const auto end = name.find('/', start);
		const auto part = std::string_view(name).substr(start,
			end == std::string::npos ? end : end - start);
		if (part.empty() || part == "." || part == "..") return false;
		if (end == std::string::npos) return true;
		start = end + 1;
	} while (start <= name.size());
	return false;
}

std::string NormalizeMember(std::string name)
{
	std::replace(name.begin(), name.end(), '\\', '/');
	return name;
}

struct ReadState {
	std::string password;
	std::wstring widePassword;
	std::string* output = nullptr;
	size_t limit = 0;
	size_t workRemaining = MaxWorkBytes;
};

int CALLBACK OnArchiveEvent(UINT message, LPARAM user, LPARAM first, LPARAM second)
{
	auto& state = *reinterpret_cast<ReadState*>(user);
	try {
		switch (message) {
		case UCM_PROCESSDATA: {
			if (second < 0) return -1;
			const size_t count = static_cast<size_t>(second);
			if (count > state.workRemaining) return -1;
			state.workRemaining -= count;
			if (state.output) {
				if (count > state.limit - state.output->size()) return -1;
				state.output->append(reinterpret_cast<const char*>(first), count);
			}
			return 1;
		}
		case UCM_NEEDPASSWORD:
			if (state.password.empty() || second <= 0 || state.password.size() >= static_cast<size_t>(second)) return -1;
			std::memcpy(reinterpret_cast<void*>(first), state.password.c_str(), state.password.size() + 1);
			return 1;
		case UCM_NEEDPASSWORDW:
			if (state.widePassword.empty() || second <= 0 || state.widePassword.size() >= static_cast<size_t>(second)) return -1;
			std::memcpy(reinterpret_cast<void*>(first), state.widePassword.c_str(),
				(state.widePassword.size() + 1) * sizeof(wchar_t));
			return 1;
		default:
			// Never prompt, follow another volume, or accept a larger dictionary.
			return -1;
		}
	} catch (...) {
		// No allocation exception may cross the C callback boundary.
		return -1;
	}
}

struct CloseArchive {
	void operator()(void* archive) const { RARCloseArchive(archive); }
};

bool Walk(const std::string& path, const std::string& password, const std::string& requested,
	size_t limit, std::string* bytes, std::vector<std::string>* names)
{
	if (path.empty() || path.find('\0') != std::string::npos || password.find('\0') != std::string::npos)
		return false;
	ReadState state;
	state.password = password;
	if (!UtfToWide(password.c_str(), state.widePassword)) return false;
	std::string resolved = Basic::NormalizeDataPath(path);
	RAROpenArchiveDataEx open{};
#ifdef _WIN32
	std::wstring widePath;
	if (!TextSystem::IsValidUtf8(resolved.data(), resolved.size()) ||
		!UtfToWide(resolved.c_str(), widePath)) return false;
	open.ArcNameW = widePath.data();
#else
	// On Unix the byte path round-trips through UnRAR's locale mapping even
	// when the caller has not selected a UTF-8 C locale.
	open.ArcName = resolved.data();
#endif
	open.OpenMode = names ? RAR_OM_LIST : RAR_OM_EXTRACT;
	open.Callback = OnArchiveEvent;
	open.UserData = reinterpret_cast<LPARAM>(&state);
	std::unique_ptr<void, CloseArchive> archive(RAROpenArchiveEx(&open));
	if (!archive || open.OpenResult != ERAR_SUCCESS || (open.Flags & ROADF_VOLUME)) return false;
	size_t nameBytes = 0;
	for (size_t count = 0; count < MaxHeaders; ++count) {
		RARHeaderDataEx header{};
		const int result = RARReadHeaderEx(archive.get(), &header);
		if (result == ERAR_END_ARCHIVE) return names != nullptr;
		if (result != ERAR_SUCCESS) return false;
		if (header.Flags & (RHDF_SPLITBEFORE | RHDF_SPLITAFTER)) return false;
		// The DLL's fixed field truncates longer names; never identify a member
		// by a truncated prefix. Resource names do not need this 1023-unit edge.
		if (std::wcslen(header.FileNameW) >= 1023) return false;
		std::string name;
		WideToUtf(std::wstring(header.FileNameW), name);
		name = NormalizeMember(std::move(name));
		// RAR3 Unix symbolic links use mode bits rather than RedirType.
		const bool regular = !(header.Flags & RHDF_DIRECTORY) && header.RedirType == 0 &&
			!(header.HostOS == 3 && (header.FileAttr & 0170000) != 0100000);
		const bool safe = regular && SafeMember(name);
		const bool wanted = safe && (names ? requested.empty() || Matches(name, requested) :
			name.size() == requested.size() && std::equal(name.begin(), name.end(), requested.begin(),
				[](char a, char b) { return Fold(a) == Fold(b); }));
		if (names && wanted) {
			if (name.size() > MaxNameBytes - nameBytes) return false;
			nameBytes += name.size();
			names->push_back(name);
		}
		int operation = RAR_SKIP;
		if (bytes && (wanted || (open.Flags & ROADF_SOLID))) {
			if (header.UnpSizeHigh != 0 || header.UnpSize > RarArchive::MaxMemberBytes ||
				header.UnpSize > state.workRemaining || header.DictSize > MaxDictionaryKiB) return false;
			if (wanted && header.UnpSize > limit) return false;
			state.output = wanted ? bytes : nullptr;
			state.limit = (std::min)(limit, static_cast<size_t>(header.UnpSize));
			operation = RAR_TEST;
		}
		if (RARProcessFileW(archive.get(), operation, nullptr, nullptr) != ERAR_SUCCESS) return false;
		if (bytes && wanted) return bytes->size() == header.UnpSize;
	}
	return false;
}
} // namespace

bool RarArchive::Read(const std::string& archive, const std::string& password,
	const std::string& member, size_t limit, std::string& output)
{
	output.clear();
	std::lock_guard<std::mutex> lock(archiveMutex);
	try {
		const auto name = NormalizeMember(member);
		if (SafeMember(name) && Walk(archive, password, name,
			(std::min)(limit, MaxMemberBytes), &output, nullptr)) return true;
	} catch (...) {}
	output.clear();
	return false;
}

bool RarArchive::List(const std::string& archive, const std::string& password,
	const std::string& filter, std::vector<std::string>& output)
{
	output.clear();
	std::lock_guard<std::mutex> lock(archiveMutex);
	try {
		if (filter.find('\0') == std::string::npos && Walk(archive, password, filter, 0, nullptr, &output)) return true;
	} catch (...) {}
	output.clear();
	return false;
}
