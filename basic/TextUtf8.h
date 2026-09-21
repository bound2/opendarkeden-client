#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace TextSystem {

// Invalid input consumes one byte and produces U+FFFD, so a malformed
// sequence never hides valid text after it. Empty input consumes nothing.
inline uint32_t Utf8Decode(const char* s, int maxLen, int* outLen, bool* valid = nullptr)
{
	if (valid) *valid = false;
	*outLen = maxLen > 0 ? 1 : 0;
	if (maxLen < 1)
		return 0xFFFD;

	const auto first = static_cast<unsigned char>(s[0]);
	int length = 0;
	uint32_t scalar = 0;
	uint32_t minimum = 0;
	if (first < 0x80) {
		length = 1; scalar = first;
	} else if (first >= 0xC2 && first <= 0xDF) {
		length = 2; scalar = first & 0x1F; minimum = 0x80;
	} else if (first >= 0xE0 && first <= 0xEF) {
		length = 3; scalar = first & 0x0F; minimum = 0x800;
	} else if (first >= 0xF0 && first <= 0xF4) {
		length = 4; scalar = first & 0x07; minimum = 0x10000;
	} else {
		return 0xFFFD;
	}

	if (length > maxLen)
		return 0xFFFD;
	for (int i = 1; i < length; ++i) {
		const auto byte = static_cast<unsigned char>(s[i]);
		if ((byte & 0xC0) != 0x80)
			return 0xFFFD;
		scalar = (scalar << 6) | (byte & 0x3F);
	}
	if (scalar < minimum || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
		return 0xFFFD;

	*outLen = length;
	if (valid) *valid = true;
	return scalar;
}

// Validation and rendering use exactly the same scalar-value rules.
inline bool IsValidUtf8(const char* data, size_t len)
{
	size_t offset = 0;
	while (offset < len) {
		const size_t remaining = len - offset;
		int consumed = 0;
		bool valid = false;
		Utf8Decode(data + offset, static_cast<int>(remaining < 4 ? remaining : 4), &consumed, &valid);
		if (!valid)
			return false;
		offset += consumed;
	}
	return true;
}

// Largest prefix within a byte budget that keeps every complete UTF-8 scalar.
// Malformed input follows Utf8Decode's one-byte progress rule; this helper
// neither validates nor transcodes the input. Embedded NUL is an ordinary byte.
inline size_t Utf8PrefixBytes(std::string_view text, size_t maxBytes)
{
	const size_t limit = maxBytes < text.size() ? maxBytes : text.size();
	size_t offset = 0;
	while (offset < limit) {
		const size_t remaining = text.size() - offset;
		int consumed = 0;
		Utf8Decode(text.data() + offset, static_cast<int>(remaining < 4 ? remaining : 4), &consumed);
		if (static_cast<size_t>(consumed) > limit - offset) break;
		offset += static_cast<size_t>(consumed);
	}
	return offset;
}

} // namespace TextSystem
