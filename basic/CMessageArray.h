//----------------------------------------------------------------------
// CMessageArray.h - message ring and optional file logging
//----------------------------------------------------------------------
// String Array이다.
// 
// 초기화 할 때 : Init(String수, 한String의 최대길이, log File);
//
// Init(...)할때 메모리를 다 잡아버린다.
// filename을 써주면 자동으로 string이 추가될때 log한다.
//----------------------------------------------------------------------

#ifndef	__CMESSAGEARRAY_H__
#define	__CMESSAGEARRAY_H__

#include "SafeFormat.h"

class CMessageArray {
	public :
		CMessageArray();
		~CMessageArray();

		//--------------------------------------------------
		// Init / Release
		//--------------------------------------------------
		void		Init(int max, int length, const char* filename=NULL);
		void		Release();

		//--------------------------------------------------
		// Add / Get
		//--------------------------------------------------
		void		Add(const char *str);
		//void		Add(std::string str)			{ Add(str.c_str()); }
		void		AddToFile(const char *str);
		//void		AddToFile(std::string str)	{ AddToFile(str.c_str()); }
		void		AddFormat(const char* format, ...);
		void		AddFormatVL(const char* format, va_list& vl);

		//--------------------------------------------------
		// The same thing, for a format that came out of a
		// data file (docs/RESTRUCTURING.md task 5.4).
		//
		// AddFormat's buffer has been bounded since 0e9d247,
		// so what is left at those call sites is the half a
		// bound cannot fix: the entry decides how many
		// arguments are consumed, and it is read from
		// Data/Info/String.inf while the argument list is
		// fixed in the source. One %s too many and vsnprintf
		// reads a stack word as a char*.
		//
		// This overload takes the arguments as a typed pack
		// instead of varargs, so SafeFormat can refuse a
		// conversion the call site did not supply. Everything
		// after the formatting - the log file, the row width,
		// the ring advance - is what AddFormat always did.
		//--------------------------------------------------
		template <typename ...Args>
		void		AddSafeFormat(const char* format, Args... args)
		{
			// The trailing default keeps this a legal array when
			// the pack is empty; nCount is what AddSafeFormatV reads.
			const SafeFormat::Arg packed[] = { SafeFormat::MakeArg(args)..., SafeFormat::Arg() };

			AddSafeFormatV(format, packed, sizeof...(args));
		}

		void		AddSafeFormatV(const char* format,
						const SafeFormat::Arg* pArgs, size_t nCount);

		const char*	operator [] (int i);

		// 외부에서 편집..
		char*&		GetCurrent()		{ return m_ppMessage[m_Current]; }

		// 다음 것
		void		Next();

		// size
		int			GetSize() const			{ return m_Max; }

		// clear
		void		Clear();

		// filename
		const char*	GetFilename() const		{ return m_Filename; }


	protected :
		// The tail of every Add*Format: the log file, the row
		// width, the ring advance. AddFormat, AddFormatVL and
		// AddSafeFormatV differ only in how they fill the
		// buffer, and this is the part they must not differ in.
		void		StoreRow(const char* pBuffer, int nLength);

		int			m_Length;		// Message 하나의 길이
		int			m_Max;			// Message 개수

		char**		m_ppMessage;	// 입력된 Message
		int			m_Current;		// 입력할려는 Message 

		// file Log
		bool			m_bLog;
		int				m_LogFile;
		char*			m_Filename;


};

#endif


