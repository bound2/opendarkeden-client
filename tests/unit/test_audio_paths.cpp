#include "test_framework.h"
#include "DXLibBackend.h"

#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

TEST(AudioPaths, LoadsWindowsSpellingForSoundAndMusic)
{
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto directory = std::filesystem::temp_directory_path()
		/ ("darkeden_audio_paths_" + std::to_string(stamp));
	std::filesystem::create_directories(directory / "Sound");
	struct Cleanup {
		std::filesystem::path directory;
		std::string driver;
		~Cleanup() {
			dxlib_music_release();
			dxlib_sound_release();
			SDL_setenv("SDL_AUDIODRIVER", driver.c_str(), 1);
			std::error_code error;
			std::filesystem::remove_all(directory, error);
		}
	} cleanup{directory, SDL_getenv("SDL_AUDIODRIVER") ? SDL_getenv("SDL_AUDIODRIVER") : ""};

	// A valid mono PCM WAV, so the assertion covers the actual SDL loaders.
	const unsigned char wav[] = {
		'R', 'I', 'F', 'F', 40, 0, 0, 0, 'W', 'A', 'V', 'E',
		'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
		0x44, 0xac, 0, 0, 0x44, 0xac, 0, 0, 1, 0, 8, 0,
		'd', 'a', 't', 'a', 4, 0, 0, 0, 128, 128, 128, 128
	};
	std::ofstream file(directory / "Sound" / "World_Crow.wav", std::ios::binary);
	file.write(reinterpret_cast<const char*>(wav), sizeof(wav));
	file.close();
	std::string requested = (directory / "SOUND" / "WORLD_CROW.WAV").generic_string();
	std::replace(requested.begin(), requested.end(), '/', '\\');

	SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
	const int initialized = dxlib_sound_init(nullptr);
	CHECK_EQ(0, initialized);
	if (initialized != 0) {
		return;
	}
	auto sound = dxlib_sound_load_wav(requested.c_str());
	CHECK(sound != nullptr);
	dxlib_sound_free(sound);
	CHECK_EQ(0, dxlib_music_init(nullptr));
	CHECK_EQ(0, dxlib_music_load(requested.c_str()));
}
