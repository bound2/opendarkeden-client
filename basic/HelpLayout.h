#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace HelpLayout {

enum class Alignment { Left, Right, Center, LeftBlock, RightBlock };
struct ImageSize { int width = 0, height = 0; };
struct TextRun {
	std::string text;
	std::uint32_t color;
	size_t row;
};
struct Image {
	std::string filename;
	Alignment alignment;
	size_t row;
	int width, height;
};
struct Document {
	std::vector<TextRun> text;
	std::vector<Image> images;
	size_t rowCount = 0;
};
struct Options {
	size_t columns = 60;
	int glyphWidth = 6, lineHeight = 20;
	// The legacy Chinese layout inserts two spaces per left-image column.
	unsigned indentScale = 1;
	size_t maxRows = 65536, maxRuns = 65536, maxImages = 1024;
	size_t maxBytes = 16 * 1024 * 1024;
};
using ImageLookup = std::function<ImageSize(std::string_view)>;

// Input is normalized UTF-8. Markup occupies complete physical lines. Missing
// images/invalid image attributes are skipped; text remains available. Layout
// failures leave output unchanged. The lookup may cache images separately.
bool Parse(std::string_view text, const Options& options, const ImageLookup& lookup,
	Document& output);

} // namespace HelpLayout
