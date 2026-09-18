//----------------------------------------------------------------------
// CMessageArray.cpp
//----------------------------------------------------------------------
#include "Platform.h"
#include "CMessageArray.h"
#include <sys/stat.h>
#include <algorithm>
#include <memory>
#include <vector>

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

#ifdef OUTPUT_DEBUG
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
	m_LogFile	= -1;
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
	// Allocate under owners before replacing the previous ring. A failed
	// allocation leaves it intact, and a filename may alias GetFilename().
	if (max <= 0 || length < 0) {
		Release();
		return;
	}
	std::vector<std::unique_ptr<char[]>> rows(static_cast<size_t>(max));
	auto table = std::make_unique<char*[]>(static_cast<size_t>(max));
	for (int i = 0; i < max; ++i) {
		rows[i] = std::make_unique<char[]>(static_cast<size_t>(length) + 1);
		table[i] = rows[i].get();
	}
	std::unique_ptr<char[]> name;
	if (filename) {
		const size_t bytes = strlen(filename) + 1;
		name = std::make_unique<char[]>(bytes);
		memcpy(name.get(), filename, bytes);
	}

	Release();
	m_Max = max;
	m_Length = length;
	m_ppMessage = table.release();
	for (auto& row : rows) row.release();
	m_Filename = name.release();
	if (m_Filename) {
		m_LogFile = PLATFORM_OPEN(m_Filename, _O_WRONLY | _O_TEXT | _O_CREAT | _O_TRUNC, LogFileMode);
		m_bLog = m_LogFile != -1;
	}
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
CMessageArray::Release()
{
	if (m_ppMessage) {
		for (int i = 0; i < m_Max; ++i) delete[] m_ppMessage[i];
		delete[] m_ppMessage;
	}
	m_ppMessage = nullptr;
	m_Max = m_Length = m_Current = 0;

	if (m_bLog) PLATFORM_CLOSE(m_LogFile);
	m_bLog = false;
	m_LogFile = -1;
	// A failed open still owns the copied filename.
	delete[] m_Filename;
	m_Filename = nullptr;
}

//----------------------------------------------------------------------
// Add 
//----------------------------------------------------------------------
// String을 추가한다. 끝에~..
//----------------------------------------------------------------------
void		
CMessageArray::Add(const char *str)
{
	if (!str || !m_ppMessage) return;
	__BEGIN_LOCK
	StoreRow(str, strlen(str));
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
	if (!str) return;
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
CMessageArray::StoreRow(const char* pBuffer, size_t nLength)
{
	if (!m_ppMessage || !pBuffer) return;
	if (m_bLog) {
		PLATFORM_WRITE(m_LogFile, pBuffer, nLength);
		PLATFORM_WRITE(m_LogFile, "\n", 1);
		#ifdef OUTPUT_FILE_LOG
			PLATFORM_CLOSE(m_LogFile);
			m_LogFile = PLATFORM_OPEN(m_Filename, _O_WRONLY | _O_TEXT | _O_APPEND | _O_CREAT, LogFileMode);
			m_bLog = m_LogFile != -1;
			m_bLog = m_LogFile != -1;
		#endif
	}
	const size_t count = (std::min)(nLength, static_cast<size_t>(m_Length));
	// memmove also permits Add(GetCurrent()) and other ring-backed text.
	memmove(m_ppMessage[m_Current], pBuffer, count);
	m_ppMessage[m_Current][count] = '\0';
	if (++m_Current == m_Max) m_Current = 0;
}

//--------------------------------------------------------------------------
// Add Format VL
//--------------------------------------------------------------------------
void
CMessageArray::AddFormatVL(const char* format, va_list& vl)
{
	if (!format || !m_ppMessage) return;
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
	if (!format || !m_ppMessage) return;
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
	if (!m_ppMessage) return;
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
	if (!m_ppMessage || i < 0 || i >= m_Max) return "";
	__BEGIN_LOCK
	// Index zero is the oldest slot. Keep both branches below m_Max
	// without truncating to BYTE or overflowing m_Current + i.
	const int untilWrap = m_Max - m_Current;
	const int index = i < untilWrap ? m_Current + i : i - untilWrap;
	const char* message = m_ppMessage[index];
	__END_LOCK
	return message;
}

//----------------------------------------------------------------------
// Clear
//----------------------------------------------------------------------
void
CMessageArray::Clear()
{
	if (!m_ppMessage) return;
	for (int i=0; i<m_Max; i++)
	{
		m_ppMessage[i][0] = NULL;
	}
}
