//----------------------------------------------------------------------
// CMessageArray.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "CMessageArray.h"
#include "DebugInfo.h"
#include <sys/stat.h>

#ifdef OUTPUT_DEBUG
	//#define OUTPUT_FILE_LOG
#endif

#define __LOGGING__

// Platform-specific includes
#ifdef PLATFORM_WINDOWS
	#include <io.h>
	#include <fcntl.h>
#else
	#include <unistd.h>
	#include <fcntl.h>
#endif

// Platform-specific I/O functions
#ifdef PLATFORM_WINDOWS
	#define PLATFORM_WRITE(fd, buf, len)	_write(fd, buf, len)
	#define PLATFORM_OPEN	_open
	#define PLATFORM_CLOSE	_close
	#define PLATFORM_LSEEK	_lseek
	static constexpr int LogFileMode = _S_IREAD | _S_IWRITE;
#else
	#define PLATFORM_WRITE(fd, buf, len)	write(fd, buf, len)
	#define PLATFORM_OPEN	open
	#define PLATFORM_CLOSE	close
	#define PLATFORM_LSEEK	lseek
	static constexpr int LogFileMode = S_IRUSR | S_IWUSR;
#endif

// Platform-specific file flags
#ifndef PLATFORM_WINDOWS
	#define _O_WRONLY    O_WRONLY
	#define _O_TEXT      0
	#define _O_APPEND    O_APPEND
	#define _O_CREAT     O_CREAT
	#define _O_TRUNC     O_TRUNC
#endif

#if defined(OUTPUT_DEBUG) && defined(__GAME_CLIENT__)
	CRITICAL_SECTION			g_Lock;

	#define __BEGIN_LOCK	EnterCriticalSection(&g_Lock);
	#define __END_LOCK		LeaveCriticalSection(&g_Lock);
#else
	#define __BEGIN_LOCK	((void)0);
	#define __END_LOCK		((void)0);
#endif


//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------
CMessageArray::CMessageArray()
{
	m_Max		= 0;
	m_Length	= 0;
	m_Current	= 0;
	m_ppMessage = NULL;

	m_bLog		= false;
	m_LogFile	= 0;
	m_Filename	= NULL;
}

CMessageArray::~CMessageArray()
{
	Release();
}


//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Init
//----------------------------------------------------------------------
void
CMessageArray::Init(int max, int length, const char* filename)
{
	// 일단 메모리 제거..
	Release();

	m_Max		= max;
	m_Length	= length;
	m_Current	= 0;

	// new
	m_ppMessage = new char* [ m_Max ];

	for (int i=0; i<m_Max; i++)
	{
		m_ppMessage[i] = new char [ m_Length+1 ];
		m_ppMessage[i][0] = NULL;
	}

	// file Log
	if (filename!=NULL)
	{
		// filename을 기억해둔다.
		m_Filename = new char [strlen(filename)+1];
		strcpy(m_Filename, filename);

		m_LogFile = PLATFORM_OPEN(filename, _O_WRONLY | _O_TEXT | _O_CREAT | _O_TRUNC, LogFileMode);

		if (m_LogFile!=-1)
		{
			m_bLog = true;
		}
	}
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
CMessageArray::Release()
{
	// (upstream note) This kept erroring out here.. hmm.. where on earth does
	// the problem come from. Can't find it.. oh dear..
	//#ifndef _DEBUG
		if (m_ppMessage!=NULL)
		{
			for (int i=0; i<m_Max; i++)
			{
				if (m_ppMessage[i]!=NULL)
				{			
					delete [] m_ppMessage[i];			
				}
			}

			delete [] m_ppMessage;

			m_Max		= 0;
			m_Length	= 0;
			m_Current	= 0;
			m_ppMessage = NULL;
		}
	//#endif

	// file log
	if (m_bLog)
	{
		PLATFORM_CLOSE(m_LogFile);
		m_bLog = false;
	}

	// Init() allocates m_Filename before it attempts the open and only raises
	// m_bLog once the open has succeeded, so the name outlives a failed open
	// and cannot be released under m_bLog. Clearing the pointer is what keeps
	// GetFilename() from handing back freed memory: Init() reassigns
	// m_Filename only when it is given a filename, so a later Init() without
	// one would otherwise leave the freed pointer in place.
	if (m_Filename != NULL)
	{
		delete [] m_Filename;
		m_Filename = NULL;
	}
}

//----------------------------------------------------------------------
// Add 
//----------------------------------------------------------------------
// String을 추가한다. 끝에~..
//----------------------------------------------------------------------
void		
CMessageArray::Add(const char *str)
{	
	#ifndef __LOGGING__
		return;
	#endif

	__BEGIN_LOCK	

	int len = strlen(str);
	
	// file log
	if (m_bLog)
	{ 
		// [ TEST CODE ] 시간 출력
		//sprintf(g_MessageBuffer, "[%4d] ", timeGetTime() % 10000);
		//PLATFORM_WRITE( m_LogFile, g_MessageBuffer, strlen(g_MessageBuffer) );

		//m_LogFile << str << endl;
		PLATFORM_WRITE( m_LogFile, str, len );
		PLATFORM_WRITE( m_LogFile, "\n", 1 );

		// [ TEST CODE ] 화일 닫고 다시 열기
		#ifdef OUTPUT_FILE_LOG
			PLATFORM_CLOSE( m_LogFile );
			m_LogFile = PLATFORM_OPEN(m_Filename, _O_WRONLY | _O_TEXT | _O_APPEND | _O_CREAT, LogFileMode);
		#endif
	}	

	if (len >= m_Length)
	{		
		for (int i=0; i<m_Length; i++)
		{
			m_ppMessage[m_Current][i] = str[i];
		}	
		m_ppMessage[m_Current][m_Length] = NULL;		
		//strcpy(m_ppMessage[m_Current], "[ERROR] CMessageArray::Add() - String too long");		
	}
	else
	{
		// 저장
		strcpy(m_ppMessage[m_Current], str);
	}

	m_Current++;
	if (m_Current==m_Max) m_Current=0;

	__END_LOCK
}

//----------------------------------------------------------------------
// Add To File
//----------------------------------------------------------------------
// File에만 추가한다. 끝에~..
//----------------------------------------------------------------------
void		
CMessageArray::AddToFile(const char *str)
{
	#ifndef __LOGGING__
		return;
	#endif

	__BEGIN_LOCK
	
	// file log
	if (m_bLog)
	{
		// [ TEST CODE ] 시간 출력
		//sprintf(g_MessageBuffer, "[%4d] ", timeGetTime() % 10000);
		//PLATFORM_WRITE( m_LogFile, g_MessageBuffer, strlen(g_MessageBuffer) );

		//m_LogFile << str << endl;
		PLATFORM_WRITE( m_LogFile, str, strlen( str ) );
		PLATFORM_WRITE( m_LogFile, "\n", 1 );

		// [ TEST CODE ] 화일 닫고 다시 열기
		#ifdef OUTPUT_FILE_LOG
			PLATFORM_CLOSE( m_LogFile );
			m_LogFile = PLATFORM_OPEN(m_Filename, _O_WRONLY | _O_TEXT | _O_APPEND | _O_CREAT, LogFileMode);
		#endif
	}	

	__END_LOCK
}

//--------------------------------------------------------------------------
// Store Row
//--------------------------------------------------------------------------
// The tail every Add*Format shares: the log file, the row width, the ring
// advance. It was written out twice, identically, in AddFormat and
// AddFormatVL; a third copy for the checked formatter would have been three
// places to keep a row-width rule in step, so there is one.
//
// Not locked here. The public entry points hold __BEGIN_LOCK across the
// whole operation, which is what has to be atomic - the format and the
// store together, not the store alone.
//--------------------------------------------------------------------------
void
CMessageArray::StoreRow(const char* pBuffer, int nLength)
{
	// file log
	if (m_bLog)
	{
		// [ TEST CODE ] print the time
		//sprintf(g_MessageBuffer, "[%4d] ", timeGetTime() % 10000);
		//PLATFORM_WRITE( m_LogFile, g_MessageBuffer, strlen(g_MessageBuffer) );

		//m_LogFile << str << endl;
		PLATFORM_WRITE( m_LogFile, pBuffer, nLength );
		PLATFORM_WRITE( m_LogFile, "\n", 1 );

		// [ TEST CODE ] close the file and open it again
		#ifdef OUTPUT_FILE_LOG
			PLATFORM_CLOSE( m_LogFile );
			m_LogFile = PLATFORM_OPEN(m_Filename, _O_WRONLY | _O_TEXT | _O_APPEND | _O_CREAT, LogFileMode);
		#endif
	}

	// in case it runs over.. (this one is serious. - -;;)
	if (nLength >= m_Length)
	{
		for (int i=0; i<m_Length; i++)
		{
			m_ppMessage[m_Current][i] = pBuffer[i];
		}

		m_ppMessage[m_Current][m_Length] = NULL;
	}
	else
	{
		// store
		strcpy(m_ppMessage[m_Current], pBuffer);
	}

	m_Current++;
	if (m_Current==m_Max) m_Current=0;
}

//--------------------------------------------------------------------------
// Add Format VL
//--------------------------------------------------------------------------
void
CMessageArray::AddFormatVL(const char* format, va_list& vl)
{
	__BEGIN_LOCK

	//AddFormat( format, vl );
//	va_list		vl;
	char		Buffer[4096];

//    va_start(vl, format);
	int written = vsnprintf(Buffer, sizeof(Buffer), format, vl);
 //   va_end(vl);

	// vsnprintf returns the length the output *would* have had, so the length
	// is taken from the buffer instead - len must be what is really stored.
	// A negative return is an encoding error: nothing usable was produced, so
	// the row is made empty instead of letting strlen walk uninitialised stack.
	// On every other path vsnprintf has already terminated inside the buffer;
	// the explicit terminator is a redundant guard on the truncation path.
	if (written < 0)
	{
		Buffer[0] = '\0';
	}
	else
	{
		Buffer[sizeof(Buffer)-1] = '\0';
	}

	StoreRow(Buffer, strlen(Buffer));

	__END_LOCK
}

//--------------------------------------------------------------------------
// Add Safe Format VL
//--------------------------------------------------------------------------
// AddFormat for a format string that came out of Data/Info/String.inf
// (docs/RESTRUCTURING.md task 5.4, code-health finding C19). The arguments
// arrive already packed with their types, so SafeFormat can decline a
// conversion the call site never supplied instead of reading a stack word
// as a char*. See CMessageArray.h for why a bounded vsnprintf was not
// enough on its own.
//
// SafeFormat::FormatV always terminates and returns what it really wrote,
// so there is no negative-return case to handle here and no need to take
// the length again with strlen.
//--------------------------------------------------------------------------
void
CMessageArray::AddSafeFormatV(const char* format,
							  const SafeFormat::Arg* pArgs, size_t nCount)
{
	// Mirrors AddFormat's guard rather than AddFormatVL's lack of one,
	// because AddFormat is what these call sites used to call. Neither
	// guard fires today: __LOGGING__ is defined unconditionally at the
	// top of this file.
	#ifndef __LOGGING__
		return;
	#endif

	__BEGIN_LOCK

	char	Buffer[4096];

	const int len = SafeFormat::FormatV(Buffer, sizeof(Buffer), format, pArgs, nCount);

	StoreRow(Buffer, len);

	__END_LOCK
}

//--------------------------------------------------------------------------
// Add Format
//--------------------------------------------------------------------------
// Build a string from a printf style format.
//--------------------------------------------------------------------------
void
CMessageArray::AddFormat(const char* format, ...)
{
	#ifndef __LOGGING__
		return;
	#endif

	__BEGIN_LOCK
	

	va_list		vl;
	char		Buffer[4096];

    va_start(vl, format);
	int written = vsnprintf(Buffer, sizeof(Buffer), format, vl);
    va_end(vl);

	// vsnprintf returns the length the output *would* have had, so the length
	// is taken from the buffer instead - len must be what is really stored.
	// A negative return is an encoding error: nothing usable was produced, so
	// the row is made empty instead of letting strlen walk uninitialised stack.
	// On every other path vsnprintf has already terminated inside the buffer;
	// the explicit terminator is a redundant guard on the truncation path.
	if (written < 0)
	{
		Buffer[0] = '\0';
	}
	else
	{
		Buffer[sizeof(Buffer)-1] = '\0';
	}

	StoreRow(Buffer, strlen(Buffer));

	__END_LOCK
}

//----------------------------------------------------------------------
// Next
//----------------------------------------------------------------------
// Current를 next로 바꾼다..
//----------------------------------------------------------------------
void
CMessageArray::Next()
{	
	#ifndef __LOGGING__
		return;
	#endif

	__BEGIN_LOCK

	// file log
	if (m_bLog)
	{
		//m_LogFile << str << endl;
		PLATFORM_WRITE( m_LogFile, m_ppMessage[m_Current], strlen( m_ppMessage[m_Current] ) );
		PLATFORM_WRITE( m_LogFile, "\n", 1 );
	}	

	m_Current++; 
	if (m_Current==m_Max) m_Current=0;

	__END_LOCK
}

//----------------------------------------------------------------------
// operator []
//----------------------------------------------------------------------
// 0 ~ MAX-1
// 0이 가장 오래된 String이고 MAX-1이 가장 최근에 것으로
// return해야 한다.
//----------------------------------------------------------------------
const char*	
CMessageArray::operator [] (int i)
{ 
	//                i   = 실제로 return되어야 하는 값
	//m_Current - (3-[0]) = m_Current;
	//m_Current - (3-[1]) = m_Current - 2;
	//m_Current - (3-[2]) = m_Current - 1;

	__BEGIN_LOCK

	BYTE gap = m_Max - i;

	if (gap > m_Current)
	{

		const char* pMessage = m_ppMessage[m_Current + m_Max - gap];
		__END_LOCK

		return pMessage;
	}
	
	const char* pMessage = m_ppMessage[m_Current - gap];	

	__END_LOCK

	return pMessage;
}

//----------------------------------------------------------------------
// Clear
//----------------------------------------------------------------------
void
CMessageArray::Clear()
{
	for (int i=0; i<m_Max; i++)
	{
		m_ppMessage[i][0] = NULL;
	}
}