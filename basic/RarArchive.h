#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace RarArchive {

// Read-only, single-volume RAR/RPK resources. Names are UTF-8, separators are
// folded, and ASCII letters compare case-insensitively as in the game's paths.
// No archive member is written to disk. Links and absolute/traversing member
// names are excluded. Failures clear output, including partial decompression.
inline constexpr size_t MaxMemberBytes = 64 * 1024 * 1024;
bool Read(const std::string& archive, const std::string& password,
	const std::string& member, size_t limit, std::string& output);

// Owned regular-member names in archive order. Empty filter means all files;
// '*' and '?' match normalized names, with ASCII case-insensitive comparison.
bool List(const std::string& archive, const std::string& password,
	const std::string& filter, std::vector<std::string>& output);

} // namespace RarArchive
