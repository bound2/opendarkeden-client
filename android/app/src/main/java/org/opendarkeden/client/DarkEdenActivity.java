package org.opendarkeden.client;

import org.libsdl.app.SDLActivity;

/**
 * The game's activity: SDL's, with the libraries it loads named. SDLActivity
 * loads each with System.loadLibrary in this order and then calls the
 * SDL_main it finds in the last one, which is the main() of
 * Client/SDLMain.cpp under the name <SDL_main.h> gives it on Android.
 * The three satellites are listed because the CMake build links them as
 * shared libraries (tools/android/build-deps.sh); the loader would find
 * them through libmain.so's DT_NEEDED entries as well, but SDL's own
 * convention is to name them, and a missing one then fails here with
 * its name rather than in dlopen with libmain's.
 */
public class DarkEdenActivity extends SDLActivity {

	@Override
	protected String[] getLibraries() {
		return new String[] {
			"SDL2",
			"SDL2_image",
			"SDL2_ttf",
			"SDL2_mixer",
			"main"
		};
	}
}
