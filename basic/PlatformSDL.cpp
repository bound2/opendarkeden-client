/*-----------------------------------------------------------------------------

	PlatformSDL.cpp

	SDL/POSIX implementation of platform abstraction layer.
	Uses SDL2 and POSIX APIs for cross-platform support.

	2025.01.14

-----------------------------------------------------------------------------*/

#include "Platform.h"
#include "ConfigFile.h"

/* Most of this file (time/thread/mutex/event/dynamic-library/keyboard/
   error-reporting/init-shutdown) is plain SDL2 calls that work identically
   on Windows, and Platform.h declares these functions unconditionally
   (e.g. platform_get_ticks(), which timeGetTime()/GetTickCount() route
   through even on PLATFORM_WINDOWS - see Platform.h). This file used to be
   entirely `#ifndef PLATFORM_WINDOWS`-only with no Windows-native
   implementation anywhere else in the project, so on Windows every one of
   these was an unresolved external at link time (LNK2001/LNK2019) the
   moment code that called them actually got compiled - which every one of
   them now does after the __WIN32__/__WINDOWS__ CMake fixes unblocked the
   rest of the codebase.
   The File/Path Functions section below (platform_get_executable_dir(),
   platform_create_directory()) is genuinely POSIX-only (dirname(),
   readlink(), 2-arg mkdir()) and stays guarded out on Windows; nothing in
   the current Windows build calls either. */

#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef PLATFORM_WINDOWS
#include <unistd.h>
#endif

#ifdef PLATFORM_LINUX
	#include <limits.h>
	#include <stdlib.h>
	/* dirname(), used by platform_get_executable_dir() below. glibc declares
	   it only here, so without this the Linux build fails to compile rather
	   than falling back to anything. */
	#include <libgen.h>
#endif

#ifdef PLATFORM_MACOS
	#include <limits.h>  /* For PATH_MAX */
#endif

#ifdef __EMSCRIPTEN__
	/* Emscripten doesn't define PATH_MAX, define a reasonable value */
	#ifndef PATH_MAX
		#define PATH_MAX 4096
	#endif
	#include <limits.h>
	#include <stdlib.h>
	#include <libgen.h>  /* For dirname */
#endif

#ifdef PLATFORM_MACOS
	#include <libgen.h>
	#include <mach-o/dyld.h>
#endif

/* Event structure definition (opaque in header) */
struct platform_event_s {
	SDL_mutex* mutex;
	SDL_cond* cond;
	int signaled;
	/* Win32 CreateEvent() semantics, which this layer mirrors: a manual-reset
	   event stays signalled until platform_event_reset(), while an auto-reset
	   event is consumed by the single waiter that observes it.
	   platform_event_wait() cannot tell the two apart without this. */
	int manual_reset;
};

/*=============================================================================
 * Time Functions
 *=============================================================================*/

/* ============================================================================
 * Time Functions
 * ============================================================================ */

DWORD platform_get_ticks(void) {
	return SDL_GetTicks();
}

uint64_t platform_get_performance_counter(void) {
	return SDL_GetPerformanceCounter();
}

uint64_t platform_get_performance_frequency(void) {
	return SDL_GetPerformanceFrequency();
}

void platform_sleep(DWORD ms) {
	SDL_Delay(ms);
}

/* ============================================================================
 * Thread/Mutex/Event/Dynamic-Library Functions
 * ============================================================================ */
/* platform_thread_t/platform_mutex_t/platform_event_t/platform_lib_t are
   real Win32 HANDLE/HMODULE on PLATFORM_WINDOWS (see Platform.h) - callers
   like MWorkThread.cpp rely on that (e.g. casting platform_thread_create()'s
   result straight to HANDLE, and creating its event members with the real
   CreateEvent() while closing them via platform_event_close()). The SDL
   versions below return SDL_Thread pointers, SDL_mutex pointers, and
   platform_event_s pointers, which are not interchangeable with those -
   so on Windows this needs a genuine
   native implementation instead of sharing the SDL one. */
#ifdef PLATFORM_WINDOWS

platform_thread_t platform_thread_create(platform_thread_func_t func, void* param) {
	return CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, param, 0, NULL);
}

int platform_thread_wait(platform_thread_t thread) {
	if (thread == NULL) return 1;
	return (WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0) ? 0 : 1;
}

void platform_thread_close(platform_thread_t thread) {
	if (thread != NULL) {
		CloseHandle(thread);
	}
}

void platform_event_close(platform_event_t event) {
	if (event != NULL) {
		CloseHandle(event);
	}
}

/* platform_mutex_*()/platform_event_create()/platform_event_wait()/
   platform_event_signal()/platform_event_reset()/platform_lib_*() are not
   implemented on Windows - nothing in the current Windows build calls
   them (code needing real synchronization primitives/event
   creation/dynamic loading on Windows uses the native
   CreateMutex/CreateEvent/SetEvent/LoadLibrary APIs directly instead of
   this abstraction layer, as MWorkThread.cpp does). */

#else /* !PLATFORM_WINDOWS */

struct ThreadWrapperData {
	platform_thread_func_t func;
	void* param;
};

int SDLCALL ThreadWrapper(void* data) {
	ThreadWrapperData* wrapper = (ThreadWrapperData*)data;
	wrapper->func(wrapper->param);
	delete wrapper;
	return 0;
}

platform_thread_t platform_thread_create(platform_thread_func_t func, void* param) {
	ThreadWrapperData* wrapper = new ThreadWrapperData;
	wrapper->func = func;
	wrapper->param = param;

	return SDL_CreateThread(ThreadWrapper, "thread", wrapper);
}

int platform_thread_wait(platform_thread_t thread) {
	if (thread == NULL) return 1;
	int status = 0;
	SDL_WaitThread(thread, &status);
	return 0;
}

void platform_thread_close(platform_thread_t thread) {
	/* SDL threads are automatically cleaned up by SDL_WaitThread */
	/* No explicit close needed */
}

platform_mutex_t platform_mutex_create(int initial_locked) {
	SDL_mutex* mutex = SDL_CreateMutex();
	if (mutex != NULL && initial_locked) {
		SDL_LockMutex(mutex);
	}
	return mutex;
}

int platform_mutex_lock(platform_mutex_t mutex) {
	return (SDL_LockMutex(mutex) == 0) ? 0 : 1;
}

int platform_mutex_unlock(platform_mutex_t mutex) {
	return (SDL_UnlockMutex(mutex) == 0) ? 0 : 1;
}

void platform_mutex_close(platform_mutex_t mutex) {
	if (mutex != NULL) {
		SDL_DestroyMutex(mutex);
	}
}

platform_event_t platform_event_create(int manual_reset, int initial_state) {
	platform_event_t event = new struct platform_event_s;
	if (event == NULL) return NULL;

	event->mutex = SDL_CreateMutex();
	event->cond = SDL_CreateCond();
	event->signaled = initial_state ? 1 : 0;
	event->manual_reset = manual_reset ? 1 : 0;

	if (event->mutex == NULL || event->cond == NULL) {
		if (event->mutex) SDL_DestroyMutex(event->mutex);
		if (event->cond) SDL_DestroyCond(event->cond);
		delete event;
		return NULL;
	}

	return event;
}

int platform_event_wait(platform_event_t event, DWORD timeout) {
	if (event == NULL) return 1;

	SDL_LockMutex(event->mutex);

	int result = 0;

	/* The condition variable can return without the flag being set - a
	   spurious wakeup, or another waiter having already consumed an
	   auto-reset signal - so the flag is the loop's predicate and the wait's
	   own return only ends the loop on failure. An event that is already
	   signalled never enters the loop, which is the old fast path. */
	if (timeout == PLATFORM_INFINITE) {
		while (!event->signaled) {
			if (SDL_CondWait(event->cond, event->mutex) != 0) {
				/* Deliberately not SDL_MUTEX_TIMEDOUT, which is 1: an
				   infinite wait cannot time out, so reporting one here would
				   be a lie the caller has no way to see through. Callers that
				   collapse every non-zero return to a timeout are unaffected
				   either way, but the value is now honest for one that does
				   not. */
				result = -1;
				break;
			}
		}
	} else {
		/* Re-waiting must not restart the caller's timeout, so the remaining
		   time is measured against a deadline taken before the first wait.
		   The clamp keeps the signed arithmetic below valid: DWORD spans
		   values a Sint32 cannot hold, and without it any timeout of 2^31 ms
		   or more would compute a negative remainder on the first pass and
		   return without ever waiting. No caller passes one - the live values
		   are 0, 2000 and PLATFORM_INFINITE - but the signature accepts it. */
		const Uint32 SDL_MAX_WAIT = 0x7FFFFFFFu;
		const Uint32 capped   = ((Uint32)timeout > SDL_MAX_WAIT) ? SDL_MAX_WAIT : (Uint32)timeout;
		const Uint32 deadline = SDL_GetTicks() + capped;

		while (!event->signaled) {
			/* Unsigned subtraction, then a signed compare, so this stays
			   correct across the ~49-day SDL_GetTicks() wrap. */
			const Sint32 remaining = (Sint32)(deadline - SDL_GetTicks());

			if (remaining <= 0) {
				result = SDL_MUTEX_TIMEDOUT;
				break;
			}

			result = SDL_CondWaitTimeout(event->cond, event->mutex,
			                             (Uint32)remaining);

			/* SDL_MUTEX_TIMEDOUT goes back round so the flag is re-checked
			   against the deadline; a negative return is a real SDL error. */
			if (result < 0) {
				break;
			}
		}
	}

	/* Whoever observes the flag consumes it, unless the event is manual-reset
	   and therefore stays signalled until platform_event_reset(). */
	if (event->signaled) {
		result = 0;
		if (!event->manual_reset) {
			event->signaled = 0;
		}
	}

	SDL_UnlockMutex(event->mutex);
	return result;
}

int platform_event_signal(platform_event_t event) {
	if (event == NULL) return 1;

	SDL_LockMutex(event->mutex);
	event->signaled = 1;

	/* A manual-reset event stays signalled for every waiter, so every waiter
	   has to be woken: SDL_CondSignal() releases exactly one and leaves the
	   rest blocked on the condition even though the flag is set. An auto-reset
	   signal is consumed by a single waiter, so waking one is what it means. */
	if (event->manual_reset) {
		SDL_CondBroadcast(event->cond);
	} else {
		SDL_CondSignal(event->cond);
	}

	SDL_UnlockMutex(event->mutex);

	return 0;
}

int platform_event_reset(platform_event_t event) {
	if (event == NULL) return 1;

	SDL_LockMutex(event->mutex);
	event->signaled = 0;
	SDL_UnlockMutex(event->mutex);

	return 0;
}

void platform_event_close(platform_event_t event) {
	if (event != NULL) {
		if (event->mutex) SDL_DestroyMutex(event->mutex);
		if (event->cond) SDL_DestroyCond(event->cond);
		delete event;
	}
}

platform_lib_t platform_lib_load(const char* filename) {
	return SDL_LoadObject(filename);
}

void* platform_lib_get_symbol(platform_lib_t lib, const char* symbol) {
	if (lib == NULL) return NULL;
	return SDL_LoadFunction(lib, symbol);
}

void platform_lib_free(platform_lib_t lib) {
	if (lib != NULL) {
		SDL_UnloadObject(lib);
	}
}

#endif /* PLATFORM_WINDOWS */

/* ============================================================================
 * File/Path Functions
 * ============================================================================ */

char platform_get_path_separator(void) {
	return '/';
}

int platform_file_exists(const char* filename) {
	struct stat st;
	return (stat(filename, &st) == 0);
}

/* Not needed on Windows yet (nothing in the current Windows build calls
   either), and genuinely POSIX-only (PATH_MAX, dirname(), readlink(), the
   2-arg POSIX mkdir() signature - Windows' _mkdir() takes just the path). */
#ifndef PLATFORM_WINDOWS
int platform_get_executable_dir(char* buffer, size_t size) {
	if (buffer == NULL || size == 0) return 1;

	char path[PATH_MAX] = {0};

	#ifdef PLATFORM_MACOS
		uint32_t bufsize = sizeof(path);
		if (_NSGetExecutablePath(path, &bufsize) != 0) {
			return 1;
		}
	#elif defined(PLATFORM_LINUX)
		/* readlink() does not terminate, and returns as many bytes as it was
		   given room for - so one byte has to be held back for the terminator
		   that is written at path[count] below. */
		ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
		if (count < 0) return 1;
		path[count] = '\0';
	#else
		return 1;
	#endif

	/* Extract directory */
	char* dir = dirname(path);
	if (dir == NULL) return 1;

	/* buffer receives dir, the separator and the terminator: len + 2 bytes.
	   The caller's buffer is only sized by `size`, so a directory whose length
	   is exactly size - 1 must be rejected, not truncated into it. */
	size_t len = strlen(dir);
	if (len + 2 > size) return 1;

	snprintf(buffer, size, "%s/", dir);
	return 0;
}

int platform_create_directory(const char* path) {
	#ifdef PLATFORM_LINUX
		return mkdir(path, 0755) == 0 ? 0 : 1;
	#else
		return mkdir(path, 0755) == 0 ? 0 : 1;
	#endif
}
#endif /* !PLATFORM_WINDOWS */

/* ============================================================================
 * Keyboard Functions
 * ============================================================================ */

int platform_is_ctrl_pressed(void) {
	/* Check keyboard state via SDL */
	const Uint8* state = SDL_GetKeyboardState(NULL);
	return (state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL]) ? 1 : 0;
}

BYTE platform_get_scan_code(DWORD lParam) {
	/* SDL uses scancodes directly */
	return (BYTE)lParam;
}

/* ============================================================================
 * Registry/Configuration Functions
 * ============================================================================ */

/* Not needed on Windows yet (nothing in the current Windows build calls
   any of these), and depends on platform_get_executable_dir()/PATH_MAX
   above, which are themselves POSIX-only and guarded out on Windows. */
#ifndef PLATFORM_WINDOWS

/* Config file path (fallback for registry) */
static char g_config_file_path[PATH_MAX] = {0};

static void get_config_file_path(void) {
	if (g_config_file_path[0] != '\0') return; /* Already computed */

	/* Get executable directory */
	char exeDir[PATH_MAX];
	if (platform_get_executable_dir(exeDir, sizeof(exeDir)) != 0) {
		snprintf(exeDir, sizeof(exeDir), "%s", "./");
	}

	/* Use config file in executable directory */
	snprintf(g_config_file_path, sizeof(g_config_file_path),
	         "%sDarkEden.conf", exeDir);
}

int platform_config_get_string(const char* key, const char* value,
                               char* buffer, DWORD* size) {
	get_config_file_path();
	return ConfigFile::GetString(g_config_file_path, key, value, buffer, size);
}

int platform_config_set_string(const char* key, const char* value,
                               const char* data) {
	get_config_file_path();
	return ConfigFile::SetString(g_config_file_path, key, value, data);
}
#endif /* !PLATFORM_WINDOWS */

/* ============================================================================
 * Error Reporting
 * ============================================================================ */

void platform_show_error(const char* title, const char* message) {
	/* On SDL platforms, show error via SDL message box */
	if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL) != 0) {
		/* Fallback to stderr */
		fprintf(stderr, "ERROR [%s]: %s\n", title, message);
	}
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

int platform_init(void) {
	/* Initialize SDL subsystems we need */
	if (SDL_Init(0) < 0) {
		return 1;
	}
	return 0;
}

void platform_shutdown(void) {
	SDL_Quit();
}
