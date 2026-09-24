#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Game progress is independent of the rendered cover. Seed once from the
// loaded image, including any white pixels it already contains.
class ScratchCoverage {
	int width = 0, height = 0;
	size_t erased = 0;
	std::vector<uint8_t> uncovered;
public:
	void Reset(int w, int h, const void* pixels, size_t pitch)
	{
		width = (std::max)(w, 0); height = (std::max)(h, 0);
		erased = 0;
		uncovered.assign(size_t(width) * height, 0);
		if (!pixels || pitch < size_t(width) * sizeof(uint16_t)) return;
		for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
			uint16_t pixel;
			std::memcpy(&pixel, static_cast<const uint8_t*>(pixels) + y * pitch + x * sizeof(pixel), sizeof(pixel));
			if (pixel == 0xffff) { uncovered[size_t(y) * width + x] = 1; ++erased; }
		}
	}
	void Erase(int x, int y, int w, int h)
	{
		if (w <= 0 || h <= 0) return;
		const int64_t right = (std::min)(int64_t(width), int64_t(x) + w);
		const int64_t bottom = (std::min)(int64_t(height), int64_t(y) + h);
		for (int64_t row = (std::max)(y, 0); row < bottom; ++row)
			for (int64_t column = (std::max)(x, 0); column < right; ++column) {
				auto& pixel = uncovered[size_t(row) * width + size_t(column)];
				if (!pixel) { pixel = 1; ++erased; }
			}
	}
	int Percent() const { return uncovered.empty() ? 0 : int(100 * erased / uncovered.size()); }
};
