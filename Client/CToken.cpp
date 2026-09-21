//------------------------------------------------------------------------
// CToken.cpp
//------------------------------------------------------------------------
#include "Client_PCH.h"
#include "CToken.h"
#include <memory>


//------------------------------------------------------------------------
//
// constructor/destructor
//
//------------------------------------------------------------------------
CToken::CToken(const char* str)
{
   m_pString = NULL;
   m_pCurrent = NULL;

   SetString(str);
}

CToken::~CToken()
{
   Release();
}


//------------------------------------------------------------------------
//
// member functions
//
//------------------------------------------------------------------------

//------------------------------------------------------------------------
// Free the buffer.
//
// Both pointers have to be cleared, not just the one being freed. Release()
// is called by ~CToken and when SetString() replaces the buffer, and m_pCurrent
// points into the same allocation - so leaving either set makes the second
// Release() a double free and makes any GetToken() in between write ('\0'
// over the delimiter) into memory that is already back on the heap.
//------------------------------------------------------------------------
void
CToken::Release()
{
   delete [] m_pString;

   m_pString  = NULL;
   m_pCurrent = NULL;
}

//------------------------------------------------------------------------
// Replace the owned string, including text borrowed from the current buffer.
//------------------------------------------------------------------------
void
CToken::SetString(const char *str)
{
	std::unique_ptr<char[]> replacement;
	if (str != NULL)
	{
		const size_t size = strlen(str) + 1;
		replacement = std::make_unique<char[]>(size);
		memcpy(replacement.get(), str, size);
	}
	// Keep borrowed tokens alive until the copy completes. An allocation
	// failure also leaves the previous string and iteration position intact.
	Release();
	m_pString = replacement.release();
	m_pCurrent = m_pString;
}

//------------------------------------------------------------------------
// delimiter에 의해서 현재의 token string을 구한다.
//------------------------------------------------------------------------
const char*
CToken::GetToken(const char* delimiter)
{
	if (m_pCurrent==NULL)
	{
		return NULL;
	}

	SkipSpace();

   char* pTemp = m_pCurrent;

   // delimiter가 최초로 나타나는 pointer를 구한다.
   char* pFound = strpbrk(m_pCurrent, delimiter);

   // last token
   if (pFound==NULL)
   {
      m_pCurrent = NULL;
   }
   // else
   else
   {
      *pFound = '\0';

      m_pCurrent = pFound+1;
   }

   return pTemp;
}

//------------------------------------------------------------------------
// 현재 위치부터 끝까지의 string을 넘겨준다.
//------------------------------------------------------------------------
const char*
CToken::GetEnd()
{
   SkipSpace();

   char* pTemp = m_pCurrent;

   m_pCurrent = NULL;

   return pTemp;
}

//------------------------------------------------------------------------
// 공백이 여러개 있을때 무시한다.
//------------------------------------------------------------------------
void
CToken::SkipSpace()
{
	while (m_pCurrent!=NULL && *m_pCurrent!='\0' && *m_pCurrent == ' ')
    {
        m_pCurrent += 1;
    }
}

