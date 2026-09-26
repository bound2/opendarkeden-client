//////////////////////////////////////////////////////////////////////////////
// ProfilePath.h
//
// The files the UI keeps per character: the window layout and skill hotkeys
// (.set) and the mail cache (.mail). They live in UserSet/ beside the game,
// the one directory the browser build persists (web/launcher.mjs mounts it
// on IndexedDB), so the directory separator has to be the platform's, which
// _PROFILE_ROOT is. A literal backslash is a directory separator only on
// Windows; on the other platforms it is a character of a file name in the
// working directory, and in the browser such a file is gone on the next
// visit.
//////////////////////////////////////////////////////////////////////////////
#ifndef __PROFILE_PATH_H__
#define __PROFILE_PATH_H__

#include "VS_UI_filepath.h"

#include <fstream>
#include <string>

namespace ProfilePath
{
	// UserSet/<name><suffix>
	inline std::string Character(const char* name, const char* suffix)
	{
		return std::string(_PROFILE_ROOT) + name + suffix;
	}

	// UserSet/<name>-<world><extension>
	inline std::string CharacterWorld(const char* name, int world, const char* extension)
	{
		return Character(name, ("-" + std::to_string(world) + extension).c_str());
	}

	// UserSet/<name>-<dimension>-<world><extension>: the name the game writes.
	inline std::string CharacterDimensionWorld(const char* name, int dimension, int world, const char* extension)
	{
		return Character(name, ("-" + std::to_string(dimension) + "-" + std::to_string(world) + extension).c_str());
	}

	// The name the game wrote before it used _PROFILE_ROOT: a backslash
	// literal, which native Linux and macOS installs still have as a file
	// of that name in the working directory. On Windows it is the same
	// path as CharacterDimensionWorld.
	inline std::string LegacyCharacterDimensionWorld(const char* name, int dimension, int world, const char* extension)
	{
		return std::string("UserSet\\") + name + "-" + std::to_string(dimension) + "-" + std::to_string(world) + extension;
	}

	// Opens the character's settings file, newest naming first, then the
	// two older namings the game once used, then the misnamed file of
	// earlier builds. Returns whether one opened.
	inline bool OpenCharacterSettings(std::ifstream& file, const char* name, int dimension, int world)
	{
		const std::string names[] = {
			CharacterDimensionWorld(name, dimension, world, ".set"),
			CharacterWorld(name, world, ".set"),
			Character(name, ".set"),
#ifndef PLATFORM_WINDOWS
			LegacyCharacterDimensionWorld(name, dimension, world, ".set"),
#endif
		};
		for (const std::string& path : names)
		{
			file.clear();
			file.open(path, std::ios::binary);
			if (file.is_open())
				return true;
		}
		return false;
	}
}

#endif
