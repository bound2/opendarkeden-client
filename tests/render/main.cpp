#define SDL_MAIN_HANDLED
#include "test_framework.h"
#include "SpriteLibBackendSDL.h"
#include "SpriteGpu.h"
#include <cstring>
#include <cstdio>

SDL_Renderer* testSpriteRenderer = nullptr;
int BenchmarkSprites(SDL_Renderer* renderer);
int BenchmarkXbrz(SDL_Renderer* renderer);
int BenchmarkMixed(SDL_Renderer* renderer);

int main(int argc, char** argv)
{
	const bool benchmark = argc == 2 && std::strcmp(argv[1], "--benchmark") == 0;
	const bool benchmarkXbrz = argc == 2 && std::strcmp(argv[1], "--benchmark-xbrz") == 0;
	const bool benchmarkMixed = argc == 2 && std::strcmp(argv[1], "--benchmark-mixed") == 0;
	const bool foreign = argc == 2 && std::strcmp(argv[1], "--foreign-shaders") == 0;
	const bool shaders =
#ifdef __EMSCRIPTEN__
		true ||
#endif
		foreign || benchmarkXbrz || benchmarkMixed || (argc == 2 && std::strcmp(argv[1], "--shaders") == 0);
	const bool accelerated = shaders || benchmark || (argc == 2 && std::strcmp(argv[1], "--accelerated") == 0);
	SDL_SetMainReady();
	if (shaders) SDL_SetHint(SDL_HINT_RENDER_DRIVER,
#ifdef __EMSCRIPTEN__
		"opengles2"
#else
		"opengl"
#endif
	);
	if (!accelerated) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
	if (spritectl_init() != 0) return 1;
	SDL_Window* window = nullptr;
#ifdef _WIN32
	HWND nativeWindow = nullptr;
	if (foreign) {
		SDL_SetHint(SDL_HINT_VIDEO_FOREIGN_WINDOW_OPENGL, "1");
		nativeWindow = CreateWindowExW(0, L"STATIC", L"Sprite renderer native window test",
			WS_POPUP, 0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
		if (nativeWindow) window = SDL_CreateWindowFrom(nativeWindow);
	} else
#else
	if (foreign) { std::fprintf(stderr, "Foreign-window test requires Windows\n"); return 1; }
#endif
	if (accelerated) window = SDL_CreateWindow("Sprite renderer test", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
	SDL_Surface* screen = accelerated ? nullptr : SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32, SDL_PIXELFORMAT_ARGB8888);
	testSpriteRenderer = accelerated && window
		? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE)
		: (screen ? SDL_CreateSoftwareRenderer(screen) : nullptr);
	int result = 1;
	if (testSpriteRenderer) {
		SDL_RendererInfo info{};
		SDL_GetRendererInfo(testSpriteRenderer, &info);
		std::printf("Sprite test device: %s; accelerated=%d\n", info.name, !!(info.flags & SDL_RENDERER_ACCELERATED));
		if (shaders && (!SpriteGpu::Attach(testSpriteRenderer) || !SpriteGpuEffects::Active())) {
			std::fprintf(stderr, "Required shader backend is unavailable: %s\n", SDL_GetError());
		} else result = benchmarkMixed ? BenchmarkMixed(testSpriteRenderer)
			: benchmarkXbrz ? BenchmarkXbrz(testSpriteRenderer)
			: benchmark ? BenchmarkSprites(testSpriteRenderer) : testfw::RunAll();
	} else std::fprintf(stderr, "Cannot create required sprite test renderer: %s\n", SDL_GetError());
	spritectl_release_present_resources(testSpriteRenderer);
	SDL_DestroyRenderer(testSpriteRenderer);
	SDL_DestroyWindow(window);
#ifdef _WIN32
	if (nativeWindow) DestroyWindow(nativeWindow);
#endif
	SDL_FreeSurface(screen);
	spritectl_shutdown();
	return result;
}
