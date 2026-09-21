#pragma once

#include <string>
#include <string_view>

namespace ResourceText {

inline constexpr size_t MaxFileBytes = 16 * 1024 * 1024;

// Decode the declared resource page once. A UTF-8 BOM or XML encoding
// declaration takes precedence; conflicting declarations and unsupported
// encodings fail without changing output. Remove the BOM/XML declaration so
// the returned UTF-8 cannot be mistaken for the original page on a later read.
// Damaged payload bytes become U+FFFD. XML syntax is checked by the XML parser.
bool Decode(std::string_view bytes, std::string& output, bool xml = false);

} // namespace ResourceText
