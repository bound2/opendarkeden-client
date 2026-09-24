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
		CMessageArray(const CMessageArray&) = delete;
		CMessageArray& operator=(const CMessageArray&) = delete;

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

		// Pack argument types before a format from code or game data is read.
		// The shared implementation owns formatting, row truncation and logging.
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
		char*		GetCurrent()		{ return m_ppMessage ? m_ppMessage[m_Current] : nullptr; }

		// 다음 것
		void		Next();

		// size
		int			GetSize() const			{ return m_Max; }

		// clear
		void		Clear();

		// filename
		const char*	GetFilename() const		{ return m_Filename; }


	protected :
		// Store the formatted message in the log and ring.
		void		StoreRow(const char* pBuffer, size_t nLength);

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


