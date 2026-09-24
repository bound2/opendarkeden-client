#include "test_framework.h"
#include "HelpImageCache.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackend.h"
#include <SDL.h>
#include <SDL_image.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
struct Images {
	std::filesystem::path directory;
	std::vector<std::filesystem::path> files;
	Images() {
		CHECK_EQ(0, spritectl_init());
		for (int i = 0; i < 1000; ++i) {
			auto candidate = std::filesystem::temp_directory_path() / ("darkeden-help-images-" + std::to_string(i));
			if (std::filesystem::create_directory(candidate)) { directory = candidate; break; }
		}
		if (directory.empty()) throw std::runtime_error("cannot create help-image fixture directory");
	}
	~Images() {
		for (const auto& file : files) std::filesystem::remove(file);
		std::filesystem::remove(directory);
	}
	std::string Path(const char* name) {
		files.push_back(directory / name);
		return files.back().string();
	}
	void Solid(const char* name, int width = 3, int height = 2, Uint8 red = 255, Uint8 green = 0, Uint8 blue = 0) {
		Surface surface(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32), SDL_FreeSurface);
		CHECK(surface != nullptr);
		if (!surface) return;
		CHECK_EQ(0, SDL_FillRect(surface.get(), nullptr, SDL_MapRGB(surface->format, red, green, blue)));
		CHECK_EQ(0, SDL_SaveBMP(surface.get(), Path(name).c_str()));
	}
};
WORD Pixel(CSpriteSurface* surface, int x = 0, int y = 0)
{
	CHECK(surface != nullptr);
	if (!surface) return 0;
	const bool locked = surface->Lock();
	CHECK(locked);
	if (!locked) return 0;
	S_SURFACEINFO info{};
	surface->GetSurfaceInfo(&info);
	WORD pixel = 0;
	std::memcpy(&pixel, static_cast<const char*>(info.p_surface) + y * info.pitch + x * sizeof(WORD), sizeof pixel);
	surface->Unlock();
	return pixel;
}
}

TEST(HelpImageCache, MissingCorruptAndEmbeddedNullPathsDoNotPoisonTheCache)
{
	Images images;
	HelpImageCache cache(images.directory.string());
	CHECK(cache.Find("missing.bmp") == nullptr);
	CHECK(cache.Load("") == nullptr);
	CHECK(cache.Load("missing.bmp") == nullptr);
	{
		std::ofstream file(images.Path("bad.jpg"), std::ios::binary);
		file << "not an image";
	}
	CHECK(cache.Load("bad.jpg") == nullptr);
	CHECK(cache.Find("bad.jpg") == nullptr);
	images.Solid("missing.bmp");
	CHECK(cache.Load(std::string_view("missing.bmp\0ignored", 19)) == nullptr);
	CHECK(cache.Find("missing.bmp") == nullptr);
	CHECK(cache.Load("missing.bmp") != nullptr);
}

TEST(HelpImageCache, ConversionPreservesRgb565ChannelsRowsAndPadding)
{
	Images images;
	Surface source(SDL_CreateRGBSurfaceWithFormat(0, 3, 2, 32, SDL_PIXELFORMAT_RGBA32), SDL_FreeSurface);
	CHECK(source != nullptr);
	if (!source) return;
	const Uint8 colors[6][3] = {{255,0,0}, {0,255,0}, {0,0,255}, {255,255,255}, {0,0,0}, {128,128,128}};
	for (int y = 0; y < 2; ++y) for (int x = 0; x < 3; ++x) {
		const auto& c = colors[y * 3 + x];
		auto* row = reinterpret_cast<Uint32*>(static_cast<char*>(source->pixels) + y * source->pitch);
		row[x] = SDL_MapRGB(source->format, c[0], c[1], c[2]);
	}
	CHECK_EQ(0, SDL_SaveBMP(source.get(), images.Path("colors.bmp").c_str()));
	HelpImageCache cache(images.directory.string());
	auto* surface = cache.Load("colors.bmp");
	CHECK(surface != nullptr);
	if (!surface) return;
	CHECK_EQ(3, surface->GetWidth());
	CHECK_EQ(2, surface->GetHeight());
	CHECK_EQ(0xf800, Pixel(surface, 0, 0));
	CHECK_EQ(0x07e0, Pixel(surface, 1, 0));
	CHECK_EQ(0x001f, Pixel(surface, 2, 0));
	CHECK_EQ(0xffff, Pixel(surface, 0, 1));
	CHECK_EQ(0x0000, Pixel(surface, 1, 1));
	CHECK_EQ(0x8410, Pixel(surface, 2, 1));
	CHECK(!surface->IsLocked());
}

TEST(HelpImageCache, RepeatedNamesReuseOwnedSurfacesAndClearAllowsReload)
{
	Images images;
	images.Solid("one.bmp");
	HelpImageCache cache(images.directory.string());
	auto* first = cache.Load("one.bmp");
	CHECK_EQ(0xf800, Pixel(first));
	images.Solid("one.bmp", 3, 2, 0, 0, 255);
	CHECK(first == cache.Load("one.bmp"));
	CHECK(first == cache.Find("one.bmp"));
	CHECK_EQ(0xf800, Pixel(first));
	images.Solid("two.bmp", 3, 2, 0, 255, 0);
	CHECK(cache.Load("two.bmp") != first);
	CHECK_EQ(0x07e0, Pixel(cache.Find("two.bmp")));
	CHECK_EQ(0xf800, Pixel(first));
	cache.Clear();
	CHECK(cache.Find("one.bmp") == nullptr);
	CHECK(cache.Find("two.bmp") == nullptr);
	CHECK_EQ(0x001f, Pixel(cache.Load("one.bmp")));
}

TEST(HelpImageCache, PixelCountAndDimensionLimitsPreserveExistingImages)
{
	Images images;
	images.Solid("one.bmp");
	images.Solid("two.bmp", 3, 2, 0, 255, 0);
	HelpImageCache small(images.directory.string(), 5, 2);
	CHECK(small.Load("one.bmp") == nullptr);
	HelpImageCache pixels(images.directory.string(), 6, 2);
	CHECK(pixels.Load("one.bmp") != nullptr);
	CHECK(pixels.Load("two.bmp") == nullptr);
	CHECK_EQ(0xf800, Pixel(pixels.Find("one.bmp")));
	HelpImageCache cache(images.directory.string(), 6, 1);
	auto* first = cache.Load("one.bmp");
	CHECK(first != nullptr);
	CHECK(cache.Load("two.bmp") == nullptr);
	CHECK_EQ(0xf800, Pixel(first));
	cache.Clear();
	CHECK_EQ(0x07e0, Pixel(cache.Load("two.bmp")));
	images.Solid("too-wide.bmp", 4097, 1);
	HelpImageCache dimensions(images.directory.string());
	CHECK(dimensions.Load("too-wide.bmp") == nullptr);
}

TEST(HelpImageCache, LoadsRealJpegDataThroughTheSharedDecoder)
{
	Images images;
	Surface source(SDL_CreateRGBSurfaceWithFormat(0, 8, 8, 32, SDL_PIXELFORMAT_RGBA32), SDL_FreeSurface);
	CHECK(source != nullptr);
	if (!source) return;
	CHECK_EQ(0, SDL_FillRect(source.get(), nullptr, SDL_MapRGB(source->format, 0, 255, 0)));
	CHECK_EQ(0, IMG_SaveJPG(source.get(), images.Path("green.jpg").c_str(), 100));
	HelpImageCache cache(images.directory.string());
	auto* surface = cache.Load("green.jpg");
	CHECK(surface != nullptr);
	if (!surface) return;
	CHECK_EQ(8, surface->GetWidth());
	CHECK_EQ(8, surface->GetHeight());
	const WORD pixel = Pixel(surface, 4, 4);
	CHECK(((pixel >> 5) & 63) >= 60);
	CHECK((pixel >> 11) <= 1);
	CHECK((pixel & 31) <= 1);
}
