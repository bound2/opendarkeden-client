#ifndef SPRITELIB_SPRITE_SCANLINE_H
#define SPRITELIB_SPRITE_SCANLINE_H

#include <cstddef>
#include <cstdint>
#include <span>

// A line starts with a segment count. Each segment has transparent and
// colored pixel counts, then wordsPerPixel words for each colored pixel.
// Check both the encoded allocation and decoded width before any consumer
// walks the run headers. Trailing padding words are allowed.
inline bool ValidateSpriteScanline(std::span<const uint16_t> line,
	int width, std::size_t wordsPerPixel = 1)
{
	if (line.empty() || width < 0 || wordsPerPixel == 0) return false;
	std::size_t offset = 1, x = 0;
	for (std::size_t run = 0; run < line[0]; ++run) {
		if (line.size() - offset < 2) return false;
		const std::size_t transparent = line[offset++];
		const std::size_t colored = line[offset++];
		if (transparent > std::size_t(width) - x) return false;
		x += transparent;
		if (colored > std::size_t(width) - x || colored > (line.size() - offset) / wordsPerPixel)
			return false;
		x += colored;
		offset += colored * wordsPerPixel;
	}
	return true;
}

// Shadow rows contain only transparent/colored counts, without color words.
inline bool ValidateShadowScanline(std::span<const uint16_t> line, int width)
{
	if (line.empty() || width < 0 || std::size_t(line[0]) * 2 + 1 > line.size()) return false;
	std::size_t remaining = static_cast<std::size_t>(width);
	for (std::size_t run = 0; run < line[0]; ++run) {
		const std::size_t extent = std::size_t(line[1 + run * 2]) + line[2 + run * 2];
		if (extent > remaining) return false;
		remaining -= extent;
	}
	return true;
}

#endif
