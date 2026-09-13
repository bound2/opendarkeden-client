//-----------------------------------------------------------------------------
// DebugInfo.h
//-----------------------------------------------------------------------------
// CMessageArray를 통한..
// Debug정보를 표시해주는가?
//-----------------------------------------------------------------------------

#ifdef PLATFORM_WINDOWS
#include "MinTr.h"
#endif

#ifndef	__DEBUGINFO_H__
#define	__DEBUGINFO_H__

	#ifdef	OUTPUT_DEBUG
		
		//------------------------------------------
		// g_DebugMessage
		//------------------------------------------		
		#include "CMessageArray.h"


		extern CMessageArray*	g_pDebugMessage;
				

	//#endif


	//#ifdef	_DEBUG
		#include <Windows.h>
		#include <WinBase.h>
		#include <stdio.h> 
		#include <stdarg.h>

		extern char	g_StringBuffer[256];

		//------------------------------------------
		// debug message
		//------------------------------------------
		#define	DEBUG_MESSAGE(debugMessage)					\
			{												\
				va_list		vl;								\
				va_start(vl, debugMessage);					\
				vsprintf(g_StringBuffer, debugMessage, vl);	\
				va_end(vl);									\
				OutputDebugString(g_StringBuffer);			\
			}
		
//		void* PASCAL operator new( unsigned int cb, LPCSTR lpszFileName, int nLine);
//		void PASCAL operator delete(void *p, LPCSTR lpszFileName, int nLine);
		//void PASCAL operator delete [] (void *p, LPCSTR lpszFileName, int nLine);
		
//		#define	DEBUG_NEW			new (__FILE__, __LINE__)

		#define DEBUG_NEW new

		
//		#define	DEBUG_ADD( message )		\
//				if (g_pDebugMessage!=NULL)	\
//					g_pDebugMessage->Add( message );
//
		void	DEBUG_ADD_FORMAT(const char* format, ...);
		void	DEBUG_ADD_FORMAT_ERR(const char* format, ...);
		void	DEBUG_ADD_FORMAT_WAR(const char* format, ...);

		void	DEBUG_ADD(const char* message);
		void	DEBUG_ADD_ERR(const char* message);
		void	DEBUG_ADD_WAR(const char* message);
		void	DEBUG_CMD(int cmd, const char* message);

	// debug가 아닌 경우..
	#else
		#define	DEBUG_MESSAGE(debugMessage)	((void)0)

		// Route the legacy DEBUG_ADD macros to the logging system instead of
		// compiling them out. Defining them as no-ops here made logging depend
		// on whether a translation unit included this header before or after
		// DebugLog.h, which silently discarded the socket and login error
		// reporting in GameInit.cpp and UIMessageManager.cpp.
		#include "DebugLog.h"

		// DebugLog.h has no equivalent - the console commands it drove only
		// exist in the OUTPUT_DEBUG build.
		#define	DEBUG_CMD(cmd, message)			((void)0)

//		#define	DEBUG_NEW			new
	#endif	


#endif