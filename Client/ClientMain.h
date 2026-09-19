//----------------------------------------------------------------------
// ClientMain.h
//
// The game's entry point, shared by the two platform entry points:
// WinMain in Client.cpp on Windows and main in SDLMain.cpp elsewhere.
// It is WinMain's old body - the command line, the working directory,
// the file definitions, the language and character input, InitApp,
// the frame loop and the shutdown - with the Win32-only steps (the
// single-instance mutex, the patcher hand-off, the DLL whitelist, the
// message pump) behind PLATFORM_WINDOWS and their SDL equivalents in
// the other branch (docs/linux-macos-port-assessment-2026-09-07.md,
// area D). SDLMain.cpp used to carry a hand-copied partial of that
// body instead, with about twenty globals re-declared at the wrong
// types.
//
// lpCmdLine is the launch argument as WinMain received it (mutable:
// the body rewrites its own pointer to the display settings' launch
// command); nCmdShow is passed through to InitApp. Returns the process
// exit code.
//----------------------------------------------------------------------
#ifndef __CLIENT_MAIN_H__
#define __CLIENT_MAIN_H__

#include <atomic>

int ClientMain(char* lpCmdLine, int nCmdShow);

// True between SDL_APP_WILLENTERBACKGROUND and SDL_APP_DIDENTERFOREGROUND
// on Android and iOS (never set on a desktop, where SDL raises neither);
// CSDLGraphics::Flip presents nothing while it holds. Defined in
// CSDLGraphicsFlip.cpp, set by the event watch SDLMain.cpp installs.
extern std::atomic<bool> g_bPresentSuspended;

#endif // __CLIENT_MAIN_H__
