//--------------------------------------------------------------------------------
//
// Filename   : Assert.h
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
// This header is an unreferenced duplicate of PacketAssert.h - nothing
// includes it, and it carries the same include guard, so it could not be
// included alongside it anyway. It is kept in step with its twin rather
// than left behind, because a stale duplicate beside live code is a
// correctness trap the day someone edits the wrong one.
//
//--------------------------------------------------------------------------------

// Compatibility entry point, for the call sites that name a file and a line
// explicitly. It writes exactly what it is given.
void __assert__ ( const char * file , uint line , const char * func , const char * expr ) throw ( AssertionError );

// C++20 entry point. The failing site arrives through the defaulted
// DiagnosticSite (Exception.h), so the macros below no longer forward
// __FILE__ and __LINE__. No dynamic exception specification, as in
// PacketAssert.h.
void __assert__ ( const char * func , const char * expr , const DiagnosticSite & site = DiagnosticSite() );

#if defined(NDEBUG)
	#define Assert(expr) ((void)0)
#elif __LINUX__
	#define Assert(expr) ((void)((expr)?0:(__assert__(__PRETTY_FUNCTION__,#expr),0)))
#elif __WIN_CONSOLE__ || __WIN32__
	#define Assert(expr) ((void)((expr)?0:(__assert__("",#expr),0)))
#elif __MFC__
	#define Assert(expr) ASSERT(expr)
#endif

#endif
