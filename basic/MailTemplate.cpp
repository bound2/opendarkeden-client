#include "MailTemplate.h"
#include "ResourceText.h"
#include "TextWrap.h"
#include <charconv>
#include <utility>

namespace MailTemplate {
namespace {
void SkipSpace(std::string_view& text)
{
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
}
bool Number(std::string_view& text, int maximum, int& value)
{
	SkipSpace(text);
	if (text.starts_with('+')) text.remove_prefix(1);
	if (text.empty()) return false;
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
	if (parsed.ec != std::errc{} || value <= 0 || value > maximum) return false;
	text.remove_prefix(static_cast<size_t>(parsed.ptr - text.data()));
	return true;
}
}

bool Parse(std::string_view text, int maxWidth, int maxHeight, Data& output)
{
	if (maxWidth <= 0 || maxHeight <= 0 || text.size() > ResourceText::MaxFileBytes ||
		text.find('\0') != std::string_view::npos) return false;
	std::string rows[4];
	for (auto& row : rows) {
		if (text.empty()) return false;
		auto line = TextSystem::NextUtf8Line(text, text.size(), {.skipSeamSpace = false});
		text.remove_prefix(line.consumed);
		row = std::move(line.text);
	}
	if (text.find_first_not_of(" \t\r\n") != std::string_view::npos) return false;
	Data candidate;
	std::string_view dimensions = rows[0];
	if (!Number(dimensions, maxWidth, candidate.width) || dimensions.empty() ||
		(dimensions.front() != ' ' && dimensions.front() != '\t') ||
		!Number(dimensions, maxHeight, candidate.height)) return false;
	SkipSpace(dimensions);
	if (!dimensions.empty()) return false;
	candidate.sender = std::move(rows[1]);
	candidate.title = std::move(rows[2]);
	candidate.contents = std::move(rows[3]);
	output = std::move(candidate);
	return true;
}
}
