/*-----------------------------------------------------------------------------

	Client_PCH.h

	The one precompiled header for the game's translation units: the
	executable, VS_UI, the packet layer and the game model. It replaces
	the original VC6 precompiled header, and since 2026-09-08 it also
	replaces the second copy that lived in VS_UI/ (that file is a
	wrapper around this one now, keeping only VS_UI's warning pragmas).
	The two had drifted: only this one defined __GAME_CLIENT__, which
	changes Packet's virtual set, so the two VS_UI translation units
	that include packet classes saw a different vtable layout from the
	library that defines them. Which copy a translation unit got was
	decided by include-path order.

	2025.01.14, merged 2026-09-08

-----------------------------------------------------------------------------*/

#ifndef __CLIENT_PCH_H__
#define __CLIENT_PCH_H__

/* Define this as a game client build. The executable and the wire
   libraries also pass __GAME_CLIENT__=1 on the command line; the two
   spellings are compatible. */
#ifndef __GAME_CLIENT__
#define __GAME_CLIENT__
#endif

/* Platform types. Platform.h detects the platform from the compiler,
   defines PLATFORM_WINDOWS / PLATFORM_LINUX / PLATFORM_MACOS /
   PLATFORM_POSIX, and on Windows includes <windows.h> with
   WIN32_LEAN_AND_MEAN, which keeps <mmsystem.h> and the DirectX-adjacent
   headers out - basic/AudioTypes.h and DXLib/CDirectDraw.h define
   SDL-backed stand-ins for those type names. */
#include "../basic/Platform.h"

/* Standard C and C++ library */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdarg>
#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <deque>
#include <bitset>
#include <algorithm>
#include <fstream>
#include <iostream>

#ifdef PLATFORM_WINDOWS
	#include <io.h>
	#include <fcntl.h>
#else
	#include <unistd.h>
#endif

/* Keep the legacy shorthand without importing every C++20 library name. */
using std::cout;
using std::endl;
using std::ifstream;
using std::ios;
using std::ofstream;
using std::string;
using std::vector;
using std::map;
using std::list;
using std::deque;
using std::bitset;

/* The game's window and sector geometry, defined in Client/Client.cpp
   and read by VS_UI. Declared here so VS_UI's translation units, which
   have no header of their own for them, see one declaration. */
extern BOOL g_MyFull;
extern RECT g_GameRect;
extern	LONG g_SECTOR_WIDTH;
extern	LONG g_SECTOR_HEIGHT;
extern	LONG g_SECTOR_WIDTH_HALF;
extern	LONG g_SECTOR_HEIGHT_HALF;
extern	LONG g_SECTOR_SKIP_PLAYER_LEFT;
extern	LONG g_SECTOR_SKIP_PLAYER_UP;

extern	LONG g_TILESURFACE_SECTOR_WIDTH;
extern	LONG g_TILESURFACE_SECTOR_HEIGHT;
extern	LONG g_TILESURFACE_SECTOR_OUTLINE_RIGHT;
extern	LONG g_TILESURFACE_SECTOR_OUTLINE_DOWN;
extern	LONG g_TILESURFACE_WIDTH;
extern	LONG g_TILESURFACE_HEIGHT;
extern	LONG g_TILESURFACE_OUTLINE_RIGHT;
extern	LONG g_TILESURFACE_OUTLINE_DOWN;
extern	LONG g_TILE_X_HALF;
extern	LONG g_TILE_Y_HALF;

/* Exception handling for Packet system */
#include "Packet/Exception.h"

#endif /* __CLIENT_PCH_H__ */
