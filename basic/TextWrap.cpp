#include "TextWrap.h"
#include "TextUtf8.h"

namespace TextSystem {

std::vector<std::string> WrapUtf8Lines(std::string_view text, size_t maxBytes)
{
	std::vector<std::string> rows;
	if (maxBytes == 0) return rows;
	while (!text.empty()) {
		const size_t newline = text.find_first_of("\r\n");
		auto line = text.substr(0, newline);
		if (line.empty()) rows.emplace_back();
		while (!line.empty()) {
			size_t cut = Utf8PrefixBytes(line, maxBytes);
			if (cut == 0) {
				int consumed = 0;
				Utf8Decode(line.data(), static_cast<int>(line.size() < 4 ? line.size() : 4), &consumed);
				cut = static_cast<size_t>(consumed);
			}
			rows.emplace_back(line.substr(0, cut));
			line.remove_prefix(cut);
			if (!line.empty() && line.front() == ' ') line.remove_prefix(1);
		}
		if (newline == std::string_view::npos) break;
		size_t consumed = newline + 1;
		if (text[newline] == '\r' && consumed < text.size() && text[consumed] == '\n') ++consumed;
		text.remove_prefix(consumed);
	}
	return rows;
}

} // namespace TextSystem
