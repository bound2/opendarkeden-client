#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace TextSystem {

struct Utf8WrapOptions {
	bool skipSeamSpace = true;
	bool splitNewlines = true;
	bool trimLeadingSpaces = false;
	bool splitEscapedNewlines = false;
};

struct Utf8Line {
	std::string text;
	size_t consumed = 0;
};

// One owned row plus the exact input consumption, including skipped spaces
// and line breaks. Empty input or a zero column consumes nothing; otherwise
// consumption is positive and within the supplied span. Callers can change
// the column between rows. Searches are bounded by this row's byte budget.
// The same scalar/layout rules as WrapUtf8Lines apply, including emitting one
// complete scalar when a positive column is too small to hold it.
Utf8Line NextUtf8Line(std::string_view text, size_t maxBytes,
	Utf8WrapOptions options = {});

// Wrap already-decoded text by a byte column, returning owned, complete rows.
// A positive column smaller than one scalar emits that whole scalar so the
// caller always advances. This is a layout budget, NOT a fixed-buffer capacity.
// By default, skip one ASCII space at a wrapped seam and split CR, LF and
// CRLF, including blank rows, without adding a row after a final newline.
// Options can preserve seam spaces, trim leading spaces, treat line breaks
// as ordinary bytes, or recognize the two-byte backslash-n marker.
// Empty text or a zero column produces no rows. Malformed bytes and
// embedded NULs follow Utf8Decode's one-byte progress rule; no decoding/repair.
std::vector<std::string> WrapUtf8Lines(std::string_view text, size_t maxBytes,
	Utf8WrapOptions options = {});

} // namespace TextSystem
