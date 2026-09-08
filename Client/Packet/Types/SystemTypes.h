//////////////////////////////////////////////////////////////////////////////
// Filename    : SystemTypes.h
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __SYSTEM_TYPES_H__
#define __SYSTEM_TYPES_H__

/* Platform detection is basic/Platform.h's: PLATFORM_WINDOWS,
   PLATFORM_LINUX, PLATFORM_MACOS, and PLATFORM_POSIX for the last two
   together. This header used to run a copy of its own, and spelled
   Linux as __LINUX__ - a macro no build ever defined - so its POSIX
   branches compiled on no platform at all. */
#include "Platform.h"

#ifdef PLATFORM_WINDOWS
	/* WIN32_LEAN_AND_MEAN keeps <Windows.h> from pulling in mmsystem.h,
	   ddraw-adjacent headers, etc., which redefine types this project's
	   own SDL stand-ins (AudioTypes.h, DXLib/CDirectDraw.h, ...) provide. */
	#ifndef _WINDOWS_
		#define WIN32_LEAN_AND_MEAN
		#include <Windows.h>
	#endif
#elif defined(PLATFORM_POSIX)
	#include <sys/types.h>
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>

//using namespace std;
using std::string;

//////////////////////////////////////////////////////////////////////////////
// built-in type redefinition
//////////////////////////////////////////////////////////////////////////////
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

/* BYTE, WORD and DWORD come from Platform.h on every platform: the
   Win32 headers on Windows, fixed-width typedefs elsewhere. */
#if defined(_MSC_VER) /* MSVC compiler */
	typedef unsigned __int64 ulonglong;
#else
	typedef unsigned long long ulonglong;
#endif

#if defined(PLATFORM_POSIX)
	const char separatorChar = '/';
	const std::string separator = "/";
#elif defined(__WINDOWS__)
	const char separatorChar = '\\';
	const std::string separator = "\\";
#endif


//////////////////////////////////////////////////////////////////////////////
// built-in type size
//////////////////////////////////////////////////////////////////////////////
const unsigned int szbool   = sizeof(bool);
const unsigned int szchar   = sizeof(char);
const unsigned int szshort  = sizeof(short);
const unsigned int szint    = sizeof(int);
const unsigned int szlong   = sizeof(int32_t);
const unsigned int szuchar  = sizeof(unsigned char);
const unsigned int szushort = sizeof(unsigned short);
const unsigned int szuint   = sizeof(unsigned int);
const unsigned int szulong  = sizeof(uint32_t);
const unsigned int szBYTE   = sizeof(BYTE);
const unsigned int szWORD   = sizeof(WORD);
const unsigned int szDWORD  = sizeof(DWORD);


//////////////////////////////////////////////////////////////////////////////
// ServerGroupInfo
//////////////////////////////////////////////////////////////////////////////
typedef BYTE ServerGroupID_t;
const uint szServerGroupID = sizeof(ServerGroupID_t);


//////////////////////////////////////////////////////////////////////////////
// SubServerInfo
//////////////////////////////////////////////////////////////////////////////
typedef WORD ServerID_t;
const uint szServerID = sizeof(ServerID_t);

typedef WORD UserNum_t;
const uint szUserNum = sizeof(UserNum_t);

//////////////////////////////////////////////////////////////////////////////
// SubServerInfo
//////////////////////////////////////////////////////////////////////////////
enum ServerStatus
{
	SERVER_FREE,
	SERVER_NORMAL,
	SERVER_BUSY,
	SERVER_VERY_BUSY,
	SERVER_FULL,
	SERVER_DOWN
};

enum WorldStatus
{
	WORLD_OPEN,
	WORLD_CLOSE
};

typedef uint32_t IP_t;
const uint szIP = sizeof(IP_t);

typedef BYTE WorldID_t;
const uint szWorldID = sizeof(WorldID_t);

#endif
