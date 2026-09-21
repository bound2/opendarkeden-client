#include "HelpLayout.h"
#include "HelpMarkup.h"
#include "TextWrap.h"

#include <algorithm>
#include <charconv>
#include <utility>

namespace HelpLayout {
namespace {
std::string_view Trim(std::string_view text)
{
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
	while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
	return text;
}

std::uint32_t Color(std::string_view text)
{
	if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
	std::uint32_t color = 0xffffff;
	if (text.empty()) return color;
	const auto read = std::from_chars(text.data(), text.data() + text.size(), color, 16);
	return read.ec == std::errc{} && read.ptr == text.data() + text.size() && color <= 0xffffff
		? color : 0xffffff;
}

bool Position(std::string_view name, Alignment& result)
{
	if (name == "L") result = Alignment::Left;
	else if (name == "R") result = Alignment::Right;
	else if (name == "C") result = Alignment::Center;
	else if (name == "LT") result = Alignment::LeftBlock;
	else if (name == "RT") result = Alignment::RightBlock;
	else return false;
	return true;
}

bool EmptyImageTag(std::string_view tag)
{
	return tag.starts_with("<file") && tag.size() > 5 &&
		std::string_view(" \t=>\"'").find(tag[5]) != std::string_view::npos;
}
}

bool Parse(std::string_view text, const Options& options, const ImageLookup& lookup,
	Document& output)
{
	if (options.columns == 0 || options.glyphWidth <= 0 || options.lineHeight <= 0 ||
		(options.indentScale != 1 && options.indentScale != 2) ||
		text.size() > options.maxBytes || text.find('\0') != std::string_view::npos) return false;
	Document candidate;
	size_t row = 0, bytes = 0, lastRow = 0, lastWidth = 0;
	size_t imageTop = 0, imageBottom = 0, leftInset = 0, rightInset = 0;
	std::uint32_t color = 0xffffff;
	bool hasLast = false, append = false;
	while (!text.empty()) {
		const auto physical = TextSystem::NextUtf8Line(text, text.size(), {.skipSeamSpace = false});
		text.remove_prefix(physical.consumed);
		const auto tag = Trim(physical.text);
		if (tag.size() >= 2 && tag.front() == '<' && tag.back() == '>') {
			if (tag.starts_with("<#")) {
				color = Color(HelpMarkup::Attribute(tag, "color"));
				append = HelpMarkup::Attribute(tag, "a") == "y";
				continue;
			}
			auto filename = HelpMarkup::Attribute(tag, "file");
			if (!filename.empty() || EmptyImageTag(tag)) {
				Alignment alignment;
				if (filename.empty() || !Position(HelpMarkup::Attribute(tag, "pos"), alignment) || !lookup) continue;
				filename += ".jpg";
				const auto dimensions = lookup(filename);
				if (dimensions.width <= 0 || dimensions.height <= 0) continue;
				row = (std::max)(row, imageBottom);
				const auto height = (static_cast<size_t>(dimensions.height) - 1) /
					static_cast<size_t>(options.lineHeight) + 1;
				if (candidate.images.size() >= options.maxImages || row > options.maxRows ||
					height > options.maxRows - row || filename.size() > options.maxBytes - bytes) return false;
				bytes += filename.size();
				candidate.images.push_back({std::move(filename), alignment, row, dimensions.width, dimensions.height});
				imageTop = row;
				imageBottom = row + height;
				const auto inset = (static_cast<size_t>(dimensions.width) - 1) /
					static_cast<size_t>(options.glyphWidth) + 1;
				leftInset = alignment == Alignment::Left ? inset * options.indentScale : 0;
				rightInset = alignment == Alignment::Right ? inset : 0;
				if (alignment != Alignment::Left && alignment != Alignment::Right) row = imageBottom;
				hasLast = append = false;
				continue;
			}
		}
		std::string_view remaining = physical.text;
		bool reuse = append && hasLast;
		if (reuse) row = lastRow;
		append = false;
		while (true) {
			if (row >= options.maxRows) return false;
			const bool besideImage = row >= imageTop && row < imageBottom;
			const size_t left = besideImage ? leftInset : 0;
			const size_t right = besideImage ? rightInset : 0;
			if (right >= options.columns || left >= options.columns - right) {
				row = imageBottom;
				reuse = false;
				continue;
			}
			const size_t indent = reuse ? (std::max)(left, lastWidth) : left;
			if (indent >= options.columns - right) {
				++row;
				reuse = false;
				continue;
			}
			const size_t available = options.columns - right - indent;
			const auto line = TextSystem::NextUtf8Line(remaining, available,
				{.skipSeamSpace = false, .splitNewlines = false});
			// A scalar may exceed a tiny column. Consume it on a fresh text row,
			// rather than placing it over an image or the previous appended run.
			if (line.text.size() > available && (indent != 0 || right != 0)) {
				row = besideImage && line.text.size() > options.columns - right - left ? imageBottom : row + 1;
				reuse = false;
				continue;
			}
			if (candidate.text.size() >= options.maxRuns || indent > options.maxBytes - bytes ||
				line.text.size() > options.maxBytes - bytes - indent) return false;
			std::string value(indent, ' ');
			value += line.text;
			lastWidth = value.size();
			bytes += value.size();
			lastRow = row;
			hasLast = true;
			candidate.text.push_back({std::move(value), color, row++});
			remaining.remove_prefix(line.consumed);
			reuse = false;
			if (remaining.empty()) break;
		}
	}
	candidate.rowCount = (std::max)(row, imageBottom);
	output = std::move(candidate);
	return true;
}

} // namespace HelpLayout
