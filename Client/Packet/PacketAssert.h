//--------------------------------------------------------------------------------
//
// Filename   : PacketAssert.h
//
// Renamed from Assert.h: the old name collided with the standard C++
// <cassert>/<assert.h> header. With Client/Packet on the include path,
// <cassert>'s internal #include <assert.h> resolved to this file instead
// of the real CRT header, so the standard assert() macro was never
// defined - breaking any translation unit that used plain assert().
//
// Written By : Reiot
//
//--------------------------------------------------------------------------------

#ifndef __ASSERT_H__
#define __ASSERT_H__

// include files
#include "Types.h"
#include "Exception.h"

//--------------------------------------------------------------------------------
//
// When NDEBUG is defined in Config.h, every Assert is ignored. Otherwise a
// failed Assert logs to a file and then throws AssertionError.
//
//--------------------------------------------------------------------------------

// Compatibility entry point, for the call sites that name a file and a line
// explicitly. It writes exactly what it is given.
void __assert__ ( const char * file , uint line , const char * func , const char * expr ) throw ( AssertionError );

// C++20 entry point. The failing site arrives through the defaulted
// DiagnosticSite (Exception.h), so the macros below no longer forward
// __FILE__ and __LINE__; only the two things a location cannot carry - the
// function spelling this platform wants and the stringized expression - are
// still passed by hand. It writes the same message, because it hands the
// captured file and line straight to the overload above. It carries no
// dynamic exception specification: those are the conformance workload the
// assessment's finding 3 counts, and a new declaration does not add to it.
void __assert__ ( const char * func , const char * expr , const DiagnosticSite & site = DiagnosticSite() );

#if defined(NDEBUG)
	#define Assert(expr) ((void)0)
#elif __LINUX__
	#define Assert(expr) ((void)((expr)?0:(__assert__(__PRETTY_FUNCTION__,#expr),0)))
#elif __WIN_CONSOLE__ || __WIN32__
	#define Assert(expr) ((void)((expr)?0:(__assert__("",#expr),0)))
#elif __MFC__
	#define Assert(expr) ASSERT(expr)
#elif defined(__APPLE__) || defined(__macos__)
	#define Assert(expr) ((void)((expr)?0:(__assert__(__PRETTY_FUNCTION__,#expr),0)))
#endif

#endif
