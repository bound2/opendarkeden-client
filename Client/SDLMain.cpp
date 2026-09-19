/*-----------------------------------------------------------------------------

	SDLMain.cpp

	The entry point on Linux, macOS, Android and iOS: main() hands the
	launch argument to ClientMain (Client.cpp), which is WinMain's body
	and runs on every platform - the command line, the working directory,
	the file definitions, the language, InitApp, the frame loop and the
	shutdown. On Android and iOS <SDL_main.h> spells main() as SDL_main,
	the function SDL's Java activity (android/) or its UIKit delegate
	calls once the platform has a window to give it; the file is the
	same.

	This file used to be a hand-copied partial of that body, 572 lines
	with about twenty globals re-declared at the wrong types (bool for a
	BOOL, int for a LONG), its own window creation and its own frame
	loop that presented the back buffer beside the one CSDLGraphics::Flip
	already does. It had never had a green build to diverge from. The SDL
	window is CSDLGraphics::Init's off Windows now, and the SDL event pump
	is inside ClientMain's loop (docs/linux-macos-port-assessment-2026-09-07.md,
	area D).

	2025.01.18 - rewritten from Client.cpp WinMain
	2026-09-08 - reduced to the bootstrap

-----------------------------------------------------------------------------*/

#include "Client_PCH.h"

#ifndef PLATFORM_WINDOWS

#include "ClientMain.h"
#include "BundledAssets.h"

#include <SDL.h>
#include <SDL_main.h>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <string>

#ifdef PLATFORM_ANDROID
#include <android/log.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

//----------------------------------------------------------------------
// The logcat bridge
//
// An Android app has no console: what the game writes to stdout and
// stderr - every fprintf(stderr, ...) in the startup path, DebugLog's
// console echo, SDL's own complaints - goes to /dev/null unless it is
// handed to the system log. The bridge replaces both descriptors with
// the write end of a pipe and drains the read end on a thread, one
// line at a time, into logcat under the tag "DarkEden"; a startup
// failure is then one `adb logcat -s DarkEden` away instead of
// invisible. The thread is detached and lives as long as the process,
// which is what the descriptors do too.
//----------------------------------------------------------------------
static void* LogcatPump(void* pArg)
{
	FILE* pIn = fdopen((int)(intptr_t)pArg, "r");
	if (pIn == NULL)
		return NULL;

	char szLine[1024];
	while (fgets(szLine, sizeof(szLine), pIn) != NULL)
	{
		size_t nLen = strlen(szLine);
		if (nLen > 0 && szLine[nLen - 1] == '\n')
			szLine[nLen - 1] = '\0';
		__android_log_write(ANDROID_LOG_INFO, "DarkEden", szLine);
	}
	fclose(pIn);
	return NULL;
}

static void RedirectStdioToLogcat()
{
	int fds[2];
	if (pipe(fds) != 0)
		return;

	// Line-buffered stdout and unbuffered stderr, as a terminal would
	// give them; a pipe would otherwise fully buffer both and a crash
	// would take the last lines with it.
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	dup2(fds[1], STDOUT_FILENO);
	dup2(fds[1], STDERR_FILENO);
	close(fds[1]);

	pthread_t thread;
	if (pthread_create(&thread, NULL, LogcatPump, (void*)(intptr_t)fds[0]) == 0)
		pthread_detach(thread);
	else
		close(fds[0]);
}
#endif // PLATFORM_ANDROID

//----------------------------------------------------------------------
// AppLifecycleWatch
//
// Android and iOS raise SDL_APP_* events for the app lifecycle, and
// raise them on the OS's thread the moment it happens - the game's
// loop, blocked in SDL_PollEvent while Android holds a paused app
// (SDL_HINT_ANDROID_BLOCK_ON_PAUSE, on by default), may not run again
// before the process is suspended or killed, which is why SDL tells
// mobile apps to take them in an event watch and not from the queue.
// Backgrounding stops the present (CSDLGraphicsFlip.cpp); a terminate
// becomes the SDL_QUIT the pump already turns into a clean shutdown,
// pushed rather than acted on here because SDL_PushEvent is the one
// thread-safe way in. A desktop never raises any of these, so the
// watch is installed everywhere and costs one switch per event.
//----------------------------------------------------------------------
static int SDLCALL AppLifecycleWatch(void* /*pUserData*/, SDL_Event* pEvent)
{
	switch (pEvent->type)
	{
	case SDL_APP_WILLENTERBACKGROUND:
		g_bPresentSuspended.store(true);
		break;

	case SDL_APP_DIDENTERFOREGROUND:
		g_bPresentSuspended.store(false);
		break;

	case SDL_APP_TERMINATING:
	{
		SDL_Event quit;
		SDL_zero(quit);
		quit.type = SDL_QUIT;
		SDL_PushEvent(&quit);
		break;
	}

	default:
		break;
	}
	return 1;
}

int main(int argc, char* argv[])
{
#ifdef PLATFORM_ANDROID
	RedirectStdioToLogcat();
#endif

	// A send() on a connection the server has closed raises SIGPIPE on
	// every POSIX system, and the default action kills the process with
	// no log line - the outcome SocketOutputStream's destructor comments
	// on and avoids by not flushing. Ignored, the send() fails with EPIPE
	// instead and SocketAPI::send_ex turns that into the ConnectException
	// the game already handles as a dropped connection. Linux could have
	// set MSG_NOSIGNAL per call; macOS has no such flag (SO_NOSIGPIPE is
	// per socket), and one line here covers both.
	signal(SIGPIPE, SIG_IGN);

	// The launch argument, as WinMain receives lpCmdLine: the first
	// argument or nothing. ClientMain folds the display settings' launch
	// command into it and may rewrite its own pointer, so it takes a
	// mutable buffer.
	std::string commandLine = (argc > 1 && argv[1] != NULL) ? argv[1] : "";

	fprintf(stderr, "Dark Eden (SDL2) starting, command line: \"%s\"\n", commandLine.c_str());

	// The subsystems initialise what they use (SpriteLib the video, dxlib
	// the audio, TextSystem the fonts); this only fails early and loudly
	// if SDL itself cannot start, before any window or data is touched.
#ifdef PLATFORM_MOBILE
	// The 800x600 frame is landscape and the UI is anchored to it; a
	// portrait window would letterbox it to a third of the screen.
	// Both landscapes, so the device's rotation lock decides which.
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0)
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	SDL_AddEventWatch(AppLifecycleWatch, NULL);

#ifdef PLATFORM_ANDROID
	// The data tree ships inside the APK (android/, from
	// tools/android/fetch-assets.sh) and is copied to the app's internal
	// files directory on the first launch, or the first after an
	// upgrade that changed it; every later launch finds the marker and
	// returns at once (basic/BundledAssets.h). ClientMain's data-root
	// search then finds Data/Info/FileDef.inf there. A failure is not
	// fatal here: the search goes on to external storage, where a tree
	// pushed by hand may be, and says on stderr - logcat - what it
	// tried. The copy is about a gigabyte and takes a while on a phone;
	// the window is black meanwhile and logcat shows the progress.
	if (const char* pInternal = SDL_AndroidGetInternalStoragePath())
	{
		struct SProgress
		{
			static void Report(size_t nDone, size_t nTotal, const char* pPath, void*)
			{
				if (nDone % 100 == 0)
					fprintf(stderr, "bundled assets: %zu of %zu files, at %s\n", nDone, nTotal, pPath);
			}
		};

		const Basic::SBundledAssetsResult Result = Basic::InstallBundledAssets(
			"", "darkeden-assets.manifest", pInternal, SProgress::Report, NULL);

		if (!Result.sError.empty())
			fprintf(stderr, "bundled assets: not installed: %s\n", Result.sError.c_str());
		else if (Result.bAlreadyCurrent)
			fprintf(stderr, "bundled assets: %s already installed under %s\n", Result.sVersion.c_str(), pInternal);
		else
			fprintf(stderr, "bundled assets: installed %s under %s (%zu files, %zu new directories)\n",
				Result.sVersion.c_str(), pInternal, Result.nFiles, Result.nDirectories);
	}
#endif

	const int result = ClientMain(&commandLine[0], 1);

	SDL_DelEventWatch(AppLifecycleWatch, NULL);
	SDL_Quit();
	return result;
}

#endif /* PLATFORM_WINDOWS */
