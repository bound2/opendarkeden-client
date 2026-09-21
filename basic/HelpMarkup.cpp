#include "HelpMarkup.h"

namespace HelpMarkup {
namespace {
bool IsNameByte(char value)
{
	return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
		(value >= '0' && value <= '9') || value == '_' || value == '-' || value == ':';
}

bool IsSpace(char value)
{
	return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}
}

std::string Attribute(std::string_view tag, std::string_view name)
{
	if (name.empty()) return {};
	for (const char byte : name)
		if (!IsNameByte(byte)) return {};

	size_t pos = 0;
	while (pos < tag.size()) {
		if (tag[pos] == '\'' || tag[pos] == '"') {
			const auto end = tag.find(tag[pos], pos + 1);
			if (end == std::string_view::npos) return {};
			pos = end + 1;
			continue;
		}
		if (!IsNameByte(tag[pos])) {
			++pos;
			continue;
		}
		const auto begin = pos;
		while (pos < tag.size() && IsNameByte(tag[pos])) ++pos;
		const bool matches = tag.substr(begin, pos - begin) == name;
		while (pos < tag.size() && IsSpace(tag[pos])) ++pos;
		if (pos < tag.size() && tag[pos] == '=') ++pos;
		while (pos < tag.size() && IsSpace(tag[pos])) ++pos;
		if (pos < tag.size() && (tag[pos] == '\'' || tag[pos] == '"')) {
			const auto end = tag.find(tag[pos], pos + 1);
			if (end == std::string_view::npos) return {};
			if (matches) return std::string(tag.substr(pos + 1, end - pos - 1));
			pos = end + 1;
		} else if (matches) {
			return {};
		}
	}
	return {};
}

} // namespace HelpMarkup
