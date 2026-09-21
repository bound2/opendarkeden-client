#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace TextSystem {

struct Utf8WrapOptions {
	bool skipSeamSpace = true;
	bool splitNewlines = true;
};

// Wrap already-decoded text by a byte column, returning owned, complete rows.
// A positive column smaller than one scalar emits that whole scalar so the
// caller always advances. This is a layout budget, NOT a fixed-buffer capacity.
// By default, skip one ASCII space at a wrapped seam and split CR, LF and
// CRLF, including blank rows, without adding a row after a final newline.
// Options can preserve seam spaces and treat line breaks as ordinary bytes.
// Empty text or a zero column produces no rows. Malformed bytes and
// embedded NULs follow Utf8Decode's one-byte progress rule; no decoding/repair.
std::vector<std::string> WrapUtf8Lines(std::string_view text, size_t maxBytes,
	Utf8WrapOptions options = {});

} // namespace TextSystem
