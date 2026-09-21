//--------------------------------------------------------------------------
// MString.h
//--------------------------------------------------------------------------

#ifndef	__MSTRING_H__
#define	__MSTRING_H__

#pragma warning(disable:4786)

#define	MAX_BUFFER_LENGTH		1024

#include <string.h>
#include <fstream>

#include "SafeFormat.h"

class MString {
	public :
		MString();
		MString(const MString& str);
		MString(const char* str);
		virtual ~MString();

		//----------------------------------------------------
		// Init / Release
		//----------------------------------------------------
		void	Init(int len);
		void	Release();

		//----------------------------------------------------
		// assign
		//----------------------------------------------------
		void	operator = (const MString& str);
		void	operator = (const char* str);
		void	Format(const char* format, ...);

		//----------------------------------------------------
		// FormatChecked
		//
		// Format() for a format that came out of a data file.
		//
		// Format() above is an ordinary varargs printf: it trusts the
		// format and the argument list to agree, and at the three call
		// sites in Client/GameUI.cpp that hand it a String.inf entry
		// they cannot, because the format is read off disk and the
		// arguments are fixed in the source
		// (docs/code-health-review-2026-08-29.md finding C19). The
		// vsnprintf inside Format() bounds the write but not the read:
		// one %s more than the call site passed still makes the CRT
		// take a stack word as a char* and copy from wherever it
		// points. This overload checks the entry's conversions against
		// the arguments it was really handed, so an unmatched
		// conversion is copied out as text instead.
		//
		// Named apart from the SafeFormat namespace it calls into, so
		// that the namespace stays reachable by its own name inside
		// this class.
		//----------------------------------------------------
		template <typename ...Args>
		void	FormatChecked(const char* format, Args... args)
		{
			char	Buffer[MAX_BUFFER_LENGTH];

			SafeFormat::Format(Buffer, sizeof(Buffer), format, args...);

			*this = Buffer;

			// operator=(const char*) keeps no allocation for an empty
			// string, so GetString() comes back NULL - and the call
			// sites pass GetString() straight to code that reads it
			// (g_pSystemMessage->Add). An empty result is reachable
			// whenever the entry is missing, since GetGameString
			// answers "" for one, so a formatter that cannot be trusted
			// to return a readable string would not be worth much.
			if (GetString() == NULL)
			{
				Init(0);
			}
		}

		//----------------------------------------------------
		// get
		//----------------------------------------------------
		size_t		GetLength()	const		{ return m_Length; }
		char*		GetString() const		{ return m_pString; }
		operator const char*() const		{ return m_pString; }
		operator	char*() const		{ return m_pString; }		

		//----------------------------------------------------
		// other operator
		//----------------------------------------------------
		bool		operator == (const char* str)		{ return strcmp(m_pString, str)==0; }
		bool		operator == (const MString& str)	{ return strcmp(m_pString, str.m_pString)==0; }
		bool		operator >	(const char* str)		{ return strcmp(m_pString, str)>0; }
		bool		operator >	(const MString& str)	{ return strcmp(m_pString, str.m_pString)>0; }
		bool		operator <	(const char* str)		{ return strcmp(m_pString, str)<0; }
		bool		operator <	(const MString& str)	{ return strcmp(m_pString, str.m_pString)<0; }
		bool		operator != (const char* str)		{ return strcmp(m_pString, str)!=0; }
		bool		operator != (const MString& str)	{ return strcmp(m_pString, str.m_pString)!=0; }

		//----------------------------------------------------
		// File I/O: resource encoding on disk, UTF-8 in memory.
		// Malformed loaded bytes become U+FFFD. Saves reject text that cannot
		// be represented in the declared resource encoding.
		//----------------------------------------------------
		virtual void	SaveToFile(std::ofstream& file);
		virtual void	LoadFromFile(std::ifstream& file);

	protected :
		size_t	m_Length;
		char*	m_pString;
};


#endif

