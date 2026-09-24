#include "xbrz.h"
#include "SpriteGpu.h"
#include "CSpriteSurface.h"
#include "MixedScene.h"
#include "FrameUpscaler.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <future>
#include <thread>
#include <vector>

int BenchmarkSprites(SDL_Renderer* renderer)
{
	constexpr int Frames = 120, Draws = 512, Size = 64;
	CSpriteSurface surface;
	if (!surface.Init(800, 600)) return 1;
	auto sprite = spritectl_create_sprite_rle(Size, Size);
	if (!sprite) return 1;
	std::vector<uint16_t> line{1, 4, Size - 8};
	for (int x = 4; x < Size - 4; ++x) line.push_back(uint16_t((x << 11) | ((x & 63) << 5) | (x & 31)));
	for (int y = 0; y < Size; ++y) {
		if (spritectl_sprite_set_scanline_rle(sprite, y, line.data(), int(line.size())) != 0) {
			spritectl_destroy_sprite(sprite);
			return 1;
		}
	}
	auto run = [&] {
		const auto start = SDL_GetPerformanceCounter();
		for (int frame = 0; frame < Frames; ++frame) {
			surface.FillSurface(0);
			for (int i = 0; i < Draws; ++i)
				if (spritectl_blt_sprite(surface.GetBackendSurface(), (i * 53 + frame) % 736,
					(i * 31) % 536, sprite, 0, 255) != 0) return -1.0;
			// Synchronize each completed frame so the GPU timing includes its
			// work, not merely command submission. Readback overhead is included.
			if (!SpriteGpu::CpuAccess(surface.GetBackendSurface(), false)) return -1.0;
		}
		return 1000.0 * double(SDL_GetPerformanceCounter() - start) / SDL_GetPerformanceFrequency() / Frames;
	};
	const double cpu = run();
	if (!SpriteGpu::Attach(renderer)) { spritectl_destroy_sprite(sprite); return 1; }
	const double gpu = run();
	const auto stats = SpriteGpu::GetCounters();
	std::printf("Synthetic RLE composition: %d frames, %d cached %dx%d sprites/frame, 800x600\n", Frames, Draws, Size, Size);
	std::printf("CPU %.3f ms/frame; GPU %.3f ms/frame (includes one readback/frame)\n", cpu, gpu);
	std::printf("GPU draws=%llu, sprite uploads=%llu, surface uploads=%llu, readbacks=%llu\n",
		static_cast<unsigned long long>(stats.draws), static_cast<unsigned long long>(stats.spriteUploads),
		static_cast<unsigned long long>(stats.surfaceUploads), static_cast<unsigned long long>(stats.readbacks));
	spritectl_destroy_sprite(sprite);
	SpriteGpu::Detach();
	return cpu >= 0 && gpu >= 0 ? 0 : 1;
}

int BenchmarkXbrz(SDL_Renderer* renderer)
{
	constexpr int Width = 800, Height = 600, Frames = 60;
	if (!SpriteGpu::Attach(renderer) || !SpriteGpuEffects::XbrzAvailable()) return 1;
	CSpriteSurface surface;
	if (!surface.Init(Width, Height)) return 1;
	DWORD pitch = 0;
	auto* bytes = static_cast<BYTE*>(surface.Lock(nullptr, &pitch));
	if (!bytes) return 1;
	const WORD palette[]{0, 0xffff, 0xf800, 0x07e0, 0x001f, 0x8410, 0x09bf, 0x23f7};
	uint32_t random = 0x18479af;
	std::vector<uint32_t> rgb(Width * Height);
	for (int y = 0; y < Height; ++y) {
		auto* row = reinterpret_cast<WORD*>(bytes + y * pitch);
		for (int x = 0; x < Width; ++x) {
			random = random * 1664525u + 1013904223u;
			row[x] = palette[(random >> 16) & 7];
		}
	}
	const int converted = SDL_ConvertPixels(Width, Height, SDL_PIXELFORMAT_RGB565, bytes, int(pitch),
		SDL_PIXELFORMAT_RGB888, rgb.data(), Width * 4);
	surface.Unlock();
	if (converted != 0) return 1;
	const unsigned workers = std::clamp(std::thread::hardware_concurrency(), 1u, 4u);
	std::printf("Synthetic xBRZ: %d changing 800x600 frames, dense palette noise, %u CPU workers\n", Frames, workers);
	for (int factor = 2; factor <= 4; ++factor) {
		std::vector<uint32_t> output(size_t(Width) * Height * factor * factor);
		auto cpuFrame = [&] {
			std::array<std::future<void>, 3> jobs;
			for (unsigned i = 1; i < workers; ++i)
				jobs[i - 1] = std::async(std::launch::async, [&, i] {
					xbrz::scale(factor, rgb.data(), output.data(), Width, Height, xbrz::ColorFormat::RGB,
						xbrz::ScalerCfg(), Height * i / workers, Height * (i + 1) / workers);
				});
			xbrz::scale(factor, rgb.data(), output.data(), Width, Height, xbrz::ColorFormat::RGB,
				xbrz::ScalerCfg(), 0, Height / workers);
			for (unsigned i = 1; i < workers; ++i) jobs[i - 1].get();
		};
		cpuFrame(); // Exclude distance-table initialization and shader warmup.
		auto start = SDL_GetPerformanceCounter();
		for (int frame = 0; frame < Frames; ++frame) { rgb[0] = (frame & 1) ? 0xffffff : 0; cpuFrame(); }
		const double cpu = 1000.0 * double(SDL_GetPerformanceCounter() - start) / SDL_GetPerformanceFrequency() / Frames;
		const SDL_Rect sample{0, 0, 1, 1};
		auto gpuFrame = [&](int frame) {
			if (!SpriteGpu::Fill(surface.GetBackendSurface(), &sample, (frame & 1) ? 0xffff : 0)) return false;
			auto* texture = SpriteGpu::UpscaledTexture(surface.GetBackendSurface(), renderer, factor);
			if (!texture || SDL_SetRenderTarget(renderer, texture) != 0) return false;
			uint32_t pixel = 0;
			// A one-pixel read waits for completed GPU work without downloading
			// the whole frame. Its synchronization cost is included in the result.
			const int read = SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_ARGB8888, &pixel, 4);
			SDL_SetRenderTarget(renderer, nullptr);
			return read == 0;
		};
		if (!gpuFrame(0)) return 1;
		start = SDL_GetPerformanceCounter();
		for (int frame = 0; frame < Frames; ++frame) if (!gpuFrame(frame)) return 1;
		const double gpu = 1000.0 * double(SDL_GetPerformanceCounter() - start) / SDL_GetPerformanceFrequency() / Frames;
		std::printf("%dx: CPU filter %.3f ms/frame; GPU filter %.3f ms/frame (includes completion wait)\n", factor, cpu, gpu);
	}
	return 0;
}

int BenchmarkMixed(SDL_Renderer* renderer)
{
	constexpr int Frames = 120;
	SpriteGpu::Detach();
	MixedScene scene;
	if (!scene.Init()) return 1;
	FrameUpscaler cpuUpscaler;
	const SDL_Rect display{0, 0, 1600, 1200}, sample{0, 0, 1, 1};
	auto draw = [&](int number, bool gpu, bool upscale) {
		scene.Draw(number);
		if (gpu) {
			auto* texture = upscale ? SpriteGpu::UpscaledTexture(scene.frame.GetBackendSurface(), renderer, 2)
				: SpriteGpu::PresentationTexture(scene.frame.GetBackendSurface(), renderer);
			if (!texture || SDL_SetRenderTarget(renderer, texture) != 0) return false;
			Uint32 pixel;
			const int result = SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_ARGB8888, &pixel, 4);
			SDL_SetRenderTarget(renderer, nullptr);
			return result == 0;
		}
		if (upscale) {
			if (!cpuUpscaler.Draw(scene.frame.GetBackendSurface()->surface, renderer, display)) return false;
			Uint32 pixel;
			if (SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_ARGB8888, &pixel, 4) != 0) return false;
		}
		return true;
	};
	std::printf("Synthetic mixed scene: %d changing 800x600 frames; terrain, 160 sprites/effects, 64x64 lighting, clipped UI, glyphs\n", Frames);
	for (bool upscale : {false, true}) {
		double times[2];
		for (int gpu = 0; gpu < 2; ++gpu) {
			SpriteGpu::Detach();
			if (gpu && (!SpriteGpu::Attach(renderer) || !SpriteGpuEffects::XbrzAvailable())) return 1;
			if (!draw(0, gpu != 0, upscale)) return 1;
			const auto before = SpriteGpu::GetCounters();
			const auto start = SDL_GetPerformanceCounter();
			for (int i = 0; i < Frames; ++i) if (!draw(i + 1, gpu != 0, upscale)) return 1;
			times[gpu] = 1000.0 * double(SDL_GetPerformanceCounter() - start) / SDL_GetPerformanceFrequency() / Frames;
			if (gpu) {
				const auto after = SpriteGpu::GetCounters();
				std::printf("GPU steady frames: sprite uploads=%llu, surface uploads=%llu, composition readbacks=%llu\n",
					static_cast<unsigned long long>(after.spriteUploads - before.spriteUploads),
					static_cast<unsigned long long>(after.surfaceUploads - before.surfaceUploads),
					static_cast<unsigned long long>(after.readbacks - before.readbacks));
				if (after.readbacks != before.readbacks || after.surfaceUploads != before.surfaceUploads) return 1;
			}
		}
		std::printf("%s: CPU %.3f ms/frame; GPU %.3f ms/frame (includes GPU completion wait)\n",
			upscale ? "Composition + 2x xBRZ" : "Composition", times[0], times[1]);
	}
	SpriteGpu::Detach();
	return 0;
}
