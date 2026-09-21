#include "HelpImageCache.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"
#include "DataPath.h"
#include <SDL_image.h>
#include <utility>

HelpImageCache::HelpImageCache(std::string baseDirectory, size_t maxPixels, size_t maxImages)
	: m_directory(std::move(baseDirectory)), m_maxPixels(maxPixels), m_maxImages(maxImages)
{
}

HelpImageCache::~HelpImageCache() = default;

CSpriteSurface* HelpImageCache::Find(std::string_view filename) const
{
	const auto found = m_images.find(filename);
	return found == m_images.end() ? nullptr : found->second.get();
}

CSpriteSurface* HelpImageCache::Load(std::string_view filename)
{
	if (filename.empty() || filename.size() > 32768 || filename.find('\0') != std::string_view::npos)
		return nullptr;
	if (auto* cached = Find(filename)) return cached;
	if (m_images.size() >= m_maxImages || m_pixels >= m_maxPixels) return nullptr;
	std::string path = m_directory;
	if (!path.empty() && path.back() != '/' && path.back() != '\\') path += '/';
	path += filename;
	path = Basic::NormalizeDataPath(path);
	std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> source(IMG_Load(path.c_str()), SDL_FreeSurface);
	if (!source || source->w <= 0 || source->h <= 0 || source->w > 4096 || source->h > 4096)
		return nullptr;
	const size_t width = static_cast<size_t>(source->w), height = static_cast<size_t>(source->h);
	if (width > (m_maxPixels - m_pixels) / height) return nullptr;
	auto surface = std::make_unique<CSpriteSurface>();
	if (!surface->Init(source->w, source->h)) return nullptr;
	// Help artwork is opaque; copy RGB rather than blending against new storage.
	if (SDL_SetSurfaceBlendMode(source.get(), SDL_BLENDMODE_NONE) != 0 ||
		SDL_SetColorKey(source.get(), SDL_FALSE, 0) != 0 ||
		SDL_BlitSurface(source.get(), nullptr, surface->GetBackendSurface()->surface, nullptr) != 0)
		return nullptr;
	auto* result = surface.get();
	m_images.emplace(std::string(filename), std::move(surface));
	m_pixels += width * height;
	return result;
}

void HelpImageCache::Clear()
{
	m_images.clear();
	m_pixels = 0;
}
