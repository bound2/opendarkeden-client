#pragma once

#include "TextEncoding.h"
#include <string>
#include <string_view>

namespace ResourceText {

inline constexpr size_t MaxFileBytes = 16 * 1024 * 1024;

// Decode the declared resource page once. A UTF-8 BOM or XML encoding
// declaration takes precedence; conflicting declarations and unsupported
// encodings fail without changing output. Remove the BOM/XML declaration so
// the returned UTF-8 cannot be mistaken for the original page on a later read.
// Display resources replace damaged bytes with U+FFFD by default. Callers that
// interpret actions can require strict rejection. XML syntax is checked later.
bool Decode(std::string_view bytes, std::string& output, bool xml = false,
	TextEncoding::InvalidInput invalid = TextEncoding::InvalidInput::Replace);

} // namespace ResourceText
