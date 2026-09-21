#include "TextWrap.h"
#include "TextUtf8.h"
#include <utility>

namespace TextSystem {

namespace {
size_t LineBreakBytes(std::string_view text, Utf8WrapOptions options)
{
	if (text.empty()) return 0;
	if (options.splitNewlines) {
		if (text.front() == '\r') return text.size() > 1 && text[1] == '\n' ? 2 : 1;
		if (text.front() == '\n') return 1;
	}
	if (options.splitEscapedNewlines && text.size() > 1 && text[0] == '\\' && text[1] == 'n')
		return 2;
	return 0;
}
}

Utf8Line NextUtf8Line(std::string_view text, size_t maxBytes, Utf8WrapOptions options)
{
	Utf8Line row;
	if (text.empty() || maxBytes == 0) return row;
	if (options.trimLeadingSpaces) {
		while (row.consumed < text.size() && text[row.consumed] == ' ') ++row.consumed;
	}
	const auto remaining = text.substr(row.consumed);
	size_t newline = std::string_view::npos;
	if (options.splitNewlines || options.splitEscapedNewlines) {
		// Stop at the first break before decoding a prefix. Otherwise a huge
		// column with many short lines would repeatedly scan the whole tail.
		for (size_t i = 0; i < remaining.size() && i <= maxBytes; ++i) {
			if (LineBreakBytes(remaining.substr(i), options) != 0) {
				newline = i;
				break;
			}
		}
	}
	const auto content = remaining.substr(0, newline);
	size_t cut = Utf8PrefixBytes(content, maxBytes);
	if (cut == 0 && !content.empty()) {
		int consumed = 0;
		Utf8Decode(content.data(), static_cast<int>(content.size() < 4 ? content.size() : 4), &consumed);
		cut = static_cast<size_t>(consumed);
	}
	row.text = remaining.substr(0, cut);
	row.consumed += cut;
	if (options.skipSeamSpace && row.consumed < text.size() && text[row.consumed] == ' ')
		++row.consumed;
	row.consumed += LineBreakBytes(text.substr(row.consumed), options);
	return row;
}

std::vector<std::string> WrapUtf8Lines(std::string_view text, size_t maxBytes,
	Utf8WrapOptions options)
{
	std::vector<std::string> rows;
	if (maxBytes == 0) return rows;
	while (!text.empty()) {
		auto row = NextUtf8Line(text, maxBytes, options);
		text.remove_prefix(row.consumed);
		rows.push_back(std::move(row.text));
	}
	return rows;
}

} // namespace TextSystem
