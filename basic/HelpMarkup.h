#pragma once

#include <string>
#include <string_view>

namespace HelpMarkup {

// Read one quoted attribute from a legacy help-tag fragment. Names match
// exactly and case-sensitively, outside quoted values. Both quote styles and
// the legacy optional '=' are accepted. Missing/malformed attributes return
// an empty string. The result owns its bytes; no encoding conversion occurs.
std::string Attribute(std::string_view tag, std::string_view name);

} // namespace HelpMarkup
