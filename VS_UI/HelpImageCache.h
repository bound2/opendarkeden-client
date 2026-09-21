#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>

class CSpriteSurface;

// Owns decoded help images. Returned pointers are borrowed until Clear/destruction.
// Limits bound cached pixels; decoding itself is performed by SDL_image.
class HelpImageCache {
public:
	explicit HelpImageCache(std::string baseDirectory,
		size_t maxPixels = 32 * 1024 * 1024, size_t maxImages = 128);
	~HelpImageCache();
	CSpriteSurface* Load(std::string_view filename);
	CSpriteSurface* Find(std::string_view filename) const;
	void Clear();
private:
	std::string m_directory;
	size_t m_maxPixels, m_maxImages, m_pixels = 0;
	std::map<std::string, std::unique_ptr<CSpriteSurface>, std::less<>> m_images;
};
