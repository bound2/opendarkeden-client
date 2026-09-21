#ifndef __CTYPEPACK_H__
#define __CTYPEPACK_H__

#ifdef PLATFORM_WINDOWS
	#include <windows.h>
	#include <fstream>
#else
	#include "../basic/Platform.h"
	#include <fstream>
	#include <cstring>
	#include <cstdio>
#endif
#include "CSpriteSetManager.h"
#include "COrderedList.h"
#ifdef PLATFORM_WINDOWS
	// CDirectDraw include removed - using ColorDraw instead
#else
	// CDirectDraw include removed - using ColorDraw instead
#endif
#include <algorithm>
#include <vector>

using std::ifstream;
using std::ios;
using std::ofstream;

#include "../../basic/ColorDraw.h"
#include <cstdint>

namespace CTypePackDetail {
inline bool OpenIndexedEntry(int fileID, LPCTSTR packFilename, LPCTSTR indexFilename,
	std::ifstream& dataFile)
{
	if (fileID < 0 || !packFilename || !indexFilename) return false;
	dataFile.open(packFilename, std::ios::binary | std::ios::ate);
	if (!dataFile) return false;
	const auto dataSize = dataFile.tellg();
	if (dataSize <= std::streampos(2)) return false;
	std::ifstream indexFile(indexFilename, std::ios::binary);
	std::uint16_t count = 0;
	if (!indexFile.read(reinterpret_cast<char*>(&count), sizeof count) || fileID >= count)
		return false;
	const auto indexOffset = std::streamoff(2) + std::streamoff(fileID) * 4;
	std::int32_t offset = 0;
	if (!indexFile.seekg(indexOffset) ||
		!indexFile.read(reinterpret_cast<char*>(&offset), sizeof offset) ||
		offset < 2 || std::streampos(offset) >= dataSize)
		return false;
	return static_cast<bool>(dataFile.seekg(static_cast<std::streamoff>(offset)));
}
} // namespace CTypePackDetail

template <class Type>
class CTypePack
{
public:
	CTypePack();
	virtual ~CTypePack();
	
	//--------------------------------------------------------
	// Init/Release
	//--------------------------------------------------------
	virtual void	Init(WORD size);
	virtual void	Release();
	
	//--------------------------------------------------------
	// Size
	//--------------------------------------------------------
	DWORD		GetSize() const { return m_Size; }
	
	//--------------------------------------------------------
	// operator
	//--------------------------------------------------------
	Type&		operator [] (WORD n);//		{ return m_pSpritePals[n]; }
	Type&		Get(WORD n);

	//--------------------------------------------------------
	// file I/O
	//--------------------------------------------------------
	virtual bool LoadFromFile(std::ifstream&file);
	virtual bool SaveToFile(std::ofstream&dataFile, std::ofstream&indexFile);
	
	virtual bool LoadFromFileRunning(LPCTSTR lpszFilename);
	virtual bool LoadFromFile(LPCTSTR lpszFilename);
	virtual bool LoadFromFilePart(int first, int last);
	virtual bool LoadFromFilePart(const CSpriteSetManager& SSM);
	virtual bool LoadFromFileData(int dataID, int fileID, LPCTSTR packFilename, LPCTSTR indexFilename);

	virtual bool ReleasePart(int first, int last);
	virtual bool ReleasePart(COrderedList<TYPE_SPRITEID> list);

	virtual bool SaveToFile(LPCTSTR lpszFilename);
	
protected:
	Type *			m_pData;
	WORD			m_Size;
	bool			m_bRunningLoad;

	// runtime loading
	WORD			m_nLoadData;	// Loading 된 CSprite의 개수
	std::ifstream *m_file;
	int*			m_file_index;
};

template <class Type>
CTypePack<Type>::CTypePack()
{
	m_pData = NULL;
	m_Size = 0;
	m_bRunningLoad = false;
	m_nLoadData = 0;
	m_file_index = NULL;
	m_file = NULL;
}

template <class Type>
CTypePack<Type>::~CTypePack()
{
	Release();
}

template <class Type>
void CTypePack<Type>::Release()
{
	m_bRunningLoad = false;
	
	if(m_file != NULL)
	{
		delete m_file;
		m_file = NULL;
	}

	if(m_file_index != NULL)
	{
		delete []m_file_index;
		m_file_index = NULL;
	}

	if(m_pData != NULL)
	{
		delete []m_pData;
		m_pData = NULL;
		m_Size = 0;
	}
}

template <class Type>
void CTypePack<Type>::Init(WORD size)
{
	if(size == 0)
		return;
	
	Release();
	
	m_Size = size;
	
	m_pData = new Type[size];
}

template <class Type>
Type &CTypePack<Type>::operator [] (WORD n)
{
	return Get(n);
}

template <class Type>
Type &CTypePack<Type>::Get(WORD n)
{
	// Get returns a reference, so it cannot report a bad index back to
	// the caller. An out of range index, or any index at all before
	// Init() has allocated the array, is answered with a shared empty
	// element instead of indexing past the end of the array or through a
	// null pointer.
	static Type	s_OutOfRange;

	if(m_pData == NULL || n >= m_Size)
		return s_OutOfRange;

	if(m_bRunningLoad && !m_pData[n].IsInit())
	{
		m_file->seekg(m_file_index[n]);
		// file에 있는 Sprite들을 Load	
		m_pData[n].LoadFromFile(*m_file);	// Sprite 읽어오기
		if(++m_nLoadData >= m_Size)
		{
			m_bRunningLoad = false;
			m_file->close();
			delete m_file;
			m_file = NULL;
			delete []m_file_index;
			m_file_index = NULL;
		}
	}
	
	return m_pData[n];
}

template <class Type>
bool CTypePack<Type>::LoadFromFile(LPCTSTR lpszFilename)
{
	std::ifstream file(lpszFilename, std::ios::binary);
	bool re = LoadFromFile(file);
	file.close();

	return re;
}

template <class Type>
bool CTypePack<Type>::SaveToFile(LPCTSTR lpszFilename)
{
	char szIndexFilename[512];
	snprintf(szIndexFilename, sizeof(szIndexFilename), "%si", lpszFilename);

	std::ofstream dataFile(lpszFilename, std::ios::binary);
	std::ofstream indexFile(szIndexFilename, std::ios::binary);

	bool re = SaveToFile(dataFile, indexFile);

	dataFile.close();
	indexFile.close();

	return re;
}


template <class Type>
bool CTypePack<Type>::LoadFromFile(std::ifstream&file)
{
//	Release();

	file.read((char *)&m_Size, 2);
	
	Init(m_Size);
	
	int i;

	for(i = 0; i < m_Size; i++)
	{
		m_pData[i].LoadFromFile(file);
	}
	
	return true;
}

//----------------------------------------------------------------------
// Load From File Running
//----------------------------------------------------------------------
// 실시간 로딩
//----------------------------------------------------------------------
template <class Type>
bool CTypePack<Type>::LoadFromFileRunning(LPCTSTR lpszFilename)
{
	//인덱스 파일 로딩
	std::string filename = lpszFilename;
	filename += 'i';
	std::ifstream indexFile(filename.c_str(), std::ios::binary);
	indexFile.read((char *)&m_Size, 2); 
	Init(m_Size);

	if(m_file == NULL)
	{
		m_file = new std::ifstream;
	}
	
	m_file_index = new int[m_Size];
	for (int i = 0; i < m_Size; i++)
	{
		indexFile.read((char*)&m_file_index[i], 4);
	}
	indexFile.close();
	
	// file에서 sprite 개수를 읽어온다.	
	m_file->open(lpszFilename, std::ios::binary);
	
	m_file->read((char*)&m_Size, 2);
	
	m_bRunningLoad = true;
	m_nLoadData = 0;
	
	return true;
}

template <class Type>
bool CTypePack<Type>::SaveToFile(std::ofstream&dataFile, std::ofstream&indexFile)
{
	//--------------------------------------------------
	// index file을 생성하기 위한 정보
	//--------------------------------------------------
//	long*	pIndex = new long [m_Size];
	std::vector<DWORD> vIndex;

	//--------------------------------------------------
	// Size 저장
	//--------------------------------------------------
	dataFile.write((const char *)&m_Size, 2);
	indexFile.write((const char *)&m_Size, 2);
	WORD realSize = m_Size;
	DWORD index = 0;
	int i;  // Declare at function scope for both loops

	for(i = 0; i < m_Size; i++)
	{
		index = dataFile.tellp();
		if(m_pData[i].SaveToFile(dataFile) == false)
		{
			realSize--;
		}
		else
		{
			vIndex.push_back(index);
		}
	}
	
	if(realSize != m_Size)
	{
		char szTemp[512];
		snprintf(szTemp, sizeof(szTemp), "real size : %d size : %d", realSize, m_Size);
		MessageBox(NULL, szTemp, "CTypePack", MB_OK);

		dataFile.seekp(0);
		dataFile.write((const char *)&realSize, 2);
		indexFile.seekp(0);
		indexFile.write((const char *)&realSize, 2);
	}

	//--------------------------------------------------
	// index 저장
	//--------------------------------------------------
	for (i=0; i<vIndex.size(); i++)
	{
		indexFile.write((const char*)&vIndex[i], 4);
	}
	
//	delete [] pIndex;
	
//	indexFile.close();
//	dataFile.close();

	if(m_bRunningLoad)
	{
		m_bRunningLoad = false;
		m_file->close();
		delete m_file;
		m_file = NULL;
		delete []m_file_index;
		m_file_index = NULL;
	}

	return true;
}

template <class Type>
bool CTypePack<Type>::LoadFromFilePart(int first, int last)
{
	last = (std::min)(last, 0xFFFE);
	for(int i = first; i <= last; i++)
		operator[](i);

	return true;
}

template <class Type>
bool CTypePack<Type>::LoadFromFilePart(const CSpriteSetManager& SSM)
{
	CSpriteSetManager::DATA_LIST::const_iterator iID = SSM.GetIterator();
	for (int t=0; t<SSM.GetSize(); t++)
	{
		if(*iID != 0xFFFF)
			Get(*iID);

		// The iterator has to advance. Without this the loop asked for
		// whatever the first entry named once per pass and never
		// touched the rest of the set.
		++iID;
	}

	return true;
}

template <class Type>
bool CTypePack<Type>::ReleasePart(int first, int last)
{
	if(m_pData == NULL)
		return false;

	// Release() is called through m_pData[i], so the range has to be
	// clamped to the pack itself. Capping last at 0xFFFE only bounded it
	// by the index type, which let the loop write through elements past
	// the end of the allocation.
	if(first < 0)
		first = 0;

	if(last >= (int)m_Size)
		last = (int)m_Size - 1;

	for(int i = first; i <= last; i++)
		m_pData[i].Release();

	return true;
}

template <class Type>
bool CTypePack<Type>::ReleasePart(COrderedList<TYPE_SPRITEID> list)
{
	if(m_pData == NULL)
		return false;

	COrderedList<TYPE_SPRITEID>::DATA_LIST::const_iterator iID = list.GetIterator();
	for (int t=0; t<list.GetSize(); t++)
	{
		// An entry naming an element outside the pack is skipped rather
		// than written through.
		if(*iID != 0xFFFF && *iID < m_Size)
			m_pData[*iID].Release();

		// The iterator has to advance. Without this the loop released
		// whatever the first entry named once per pass and ignored every
		// other element in the list.
		++iID;
	}

	return true;
}

template <class Type>
bool CTypePack<Type>::LoadFromFileData(int dataID, int fileID, LPCTSTR packFilename, LPCTSTR indexFilename)
{
	if (!m_pData || dataID < 0 || dataID >= m_Size) return false;
	std::ifstream dataFile;
	if (!CTypePackDetail::OpenIndexedEntry(fileID, packFilename, indexFilename, dataFile))
		return false;
	return m_pData[dataID].LoadFromFile(dataFile);
}

// CTypePack2
template <class TypeBase, class Type1, class Type2>
class CTypePack2
{
private:
	// Disable copy constructor and copy assignment to prevent issues with m_file pointer
	CTypePack2(const CTypePack2&) = delete;
	CTypePack2& operator=(const CTypePack2&) = delete;

	// Disable move constructor and move assignment to prevent m_file pointer from being moved
	CTypePack2(CTypePack2&&) = delete;
	CTypePack2& operator=(CTypePack2&&) = delete;

public:
	CTypePack2();
	virtual ~CTypePack2();
	
	//--------------------------------------------------------
	// Init/Release
	//--------------------------------------------------------
	virtual void	Init(WORD size );
	virtual void	Release();
	
	//--------------------------------------------------------
	// Size
	//--------------------------------------------------------
	DWORD		GetSize() const { return m_Size; }
	
	//--------------------------------------------------------
	// operator
	//--------------------------------------------------------
	TypeBase&		operator [] (WORD n);//		{ return m_pSpritePals[n]; }
	TypeBase&		Get(WORD n);

	//--------------------------------------------------------
	// file I/O
	//--------------------------------------------------------
	virtual bool LoadFromFile(std::ifstream&file);
	virtual bool SaveToFile(std::ofstream&dataFile, std::ofstream&indexFile);
	
	virtual bool LoadFromFileRunning(LPCTSTR lpszFilename);
	virtual bool LoadFromFile(LPCTSTR lpszFilename);
	virtual bool LoadFromFilePart(int first, int last);
	virtual bool LoadFromFilePart(const CSpriteSetManager& SSM);
	virtual bool LoadFromFileData(int dataID, int fileID, LPCTSTR packFilename, LPCTSTR indexFilename);

	virtual bool ReleasePart(int first, int last);
	virtual bool ReleasePart(COrderedList<TYPE_SPRITEID> list);

	virtual bool SaveToFile(LPCTSTR lpszFilename);

	virtual bool Is565() { return m_bSecond; }
	
protected:
	TypeBase *			m_pData;
	WORD			m_Size;
	bool			m_bRunningLoad;

	// runtime loading
	WORD			m_nLoadData;	// Loading 된 CSprite의 개수
	std::ifstream *m_file;
	int*			m_file_index;
	bool			m_bSecond;
};

template <class TypeBase, class Type1, class Type2>
CTypePack2<TypeBase, Type1, Type2>::CTypePack2()
{
	m_pData = NULL;
	m_Size = 0;
	m_bRunningLoad = false;
	m_nLoadData = 0;
	m_file_index = NULL;
	m_file = NULL;

	// Read by Release() to choose which concrete type to delete[] and
	// by Get() to choose the matching spare element, so it cannot be
	// left holding whatever was on the stack.
	m_bSecond = false;
}

template <class TypeBase, class Type1, class Type2>
CTypePack2<TypeBase, Type1, Type2>::~CTypePack2()
{
	Release();
}

template <class TypeBase, class Type1, class Type2>
void CTypePack2<TypeBase, Type1, Type2>::Release()
{
//	printf("DEBUG Release: this=%p, m_file=%p, m_bRunningLoad=%d\n", this, m_file, m_bRunningLoad);
	m_bRunningLoad = false;

	if(m_file != NULL)
	{
//		printf("DEBUG Release: this=%p, deleting m_file=%p\n", this, m_file);
		delete m_file;
		m_file = NULL;
	}

	if(m_file_index != NULL)
	{
		delete []m_file_index;
		m_file_index = NULL;
	}

	if(m_pData != NULL)
	{
		// IMPORTANT: Delete with correct type to match new Type1[size] or new Type2[size]
		// Using base class pointer to delete derived class array is UB even with virtual destructor
		if(m_bSecond)
			delete [] ((Type2*)m_pData);
		else
			delete [] ((Type1*)m_pData);
		m_pData = NULL;
		m_Size = 0;
	}
}

template <class TypeBase, class Type1, class Type2>
void CTypePack2<TypeBase, Type1, Type2>::Init(WORD size)
{
	if(size == 0)
		return;
	
	Release();
	
	m_Size = size;
	m_bSecond = ColorDraw::Is565();

	if( m_bSecond == true )
		m_pData = new Type2[size];
	else
		m_pData = new Type1[size];
}

template <class TypeBase, class Type1, class Type2>
TypeBase &CTypePack2<TypeBase, Type1, Type2>::operator [] (WORD n)
{
	return Get(n);
}

template <class TypeBase, class Type1, class Type2>
TypeBase &CTypePack2<TypeBase, Type1, Type2>::Get(WORD n)
{
	// Get returns a reference, so it cannot report a bad index back to
	// the caller. An out of range index, or any index at all before
	// Init() has allocated the array, is answered with a shared empty
	// element instead of indexing past the end of the array or through a
	// null pointer.
	//
	// This has to happen before anything touches m_pData: the range
	// check that used to live further down ran after the element had
	// already been read once to test IsInit(), and only covered the
	// running-load path.
	//
	// One spare of each concrete type, picked by the same flag Init()
	// used to choose what to allocate. Handing back a Type1 while the
	// pack holds Type2 would give the caller an element of a different
	// type from every other element in the pack.
	static Type1	s_OutOfRangeFirst;
	static Type2	s_OutOfRangeSecond;

	if(m_pData == NULL || n >= m_Size)
	{
		if(m_bSecond)
			return s_OutOfRangeSecond;

		return s_OutOfRangeFirst;
	}

	if(m_bRunningLoad && !m_pData[n].IsInit())
	{
		// Safety check: disable lazy loading if file pointer is invalid
		if (m_file == NULL)
		{
			m_bRunningLoad = false;
			return m_pData[n];
		}

		// Debug: print object and file pointer info BEFORE using m_file
//		printf("DEBUG Get[%d]: this=%p, m_file=%p, m_nLoadData=%d, m_Size=%d\n",
//		       n, this, m_file, m_nLoadData, m_Size);

		// Try to load sprite - use exception handler to detect file corruption
		try {
			// Check if file stream is valid before using it
			if (!m_file->good())
			{
				printf("WARNING Get[%d]: this=%p, m_file=%p is not good(), disabling lazy loading\n",
				       n, this, m_file);
				m_bRunningLoad = false;
				return m_pData[n];
			}
			m_file->seekg(m_file_index[n]);
			m_pData[n].LoadFromFile(*m_file);	// Sprite 읽어오기
		}
		catch (...)
		{
			// File operation failed, disable lazy loading
			printf("WARNING: Failed to load sprite %d from file, disabling lazy loading\n", n);
			m_bRunningLoad = false;
			return m_pData[n];
		}

		if(++m_nLoadData >= m_Size)
		{
			m_bRunningLoad = false;
			m_file->close();
			delete m_file;
			m_file = NULL;
			delete []m_file_index;
			m_file_index = NULL;
		}
	}

	return m_pData[n];
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFile(LPCTSTR lpszFilename)
{
	std::ifstream file(lpszFilename, std::ios::binary);
	bool re = LoadFromFile(file);
	file.close();

	return re;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::SaveToFile(LPCTSTR lpszFilename)
{
	char szIndexFilename[512];
	snprintf(szIndexFilename, sizeof(szIndexFilename), "%si", lpszFilename);

	std::ofstream dataFile(lpszFilename, std::ios::binary);
	std::ofstream indexFile(szIndexFilename, std::ios::binary);

	bool re = SaveToFile(dataFile, indexFile);

	dataFile.close();
	indexFile.close();

	return re;
}


template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFile(std::ifstream&file)
{
//	Release();

	file.read((char *)&m_Size, 2);
	
	Init(m_Size);
	
	int i;

	for(i = 0; i < m_Size; i++)
	{
		m_pData[i].LoadFromFile(file);
	}
	
	return true;
}

//----------------------------------------------------------------------
// Load From File Running
//----------------------------------------------------------------------
// 실시간 로딩
//----------------------------------------------------------------------
template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFileRunning(LPCTSTR lpszFilename)
{
	//인덱스 파일 로딩
	std::string filename = lpszFilename;
	filename += 'i';
	std::ifstream indexFile(filename.c_str(), std::ios::binary);

	// Check if index file opened successfully
	if (!indexFile.is_open() || !indexFile.good())
	{
		printf("ERROR: Failed to open index file: %s\n", filename.c_str());
		return false;
	}

	indexFile.read((char *)&m_Size, 2);

	if (!indexFile.good())
	{
		printf("ERROR: Failed to read size from index file: %s\n", filename.c_str());
		indexFile.close();
		return false;
	}

	Init(m_Size);

	if(m_file == NULL)
	{
		m_file = new std::ifstream;
//		printf("DEBUG LoadFromFileRunning: Created new m_file=%p for %s\n", (void*)m_file, lpszFilename);
	}
	else
	{
//		printf("DEBUG LoadFromFileRunning: Reusing existing m_file=%p for %s\n", (void*)m_file, lpszFilename);
	}

	m_file_index = new int[m_Size];
	for (int i = 0; i < m_Size; i++)
	{
		indexFile.read((char*)&m_file_index[i], 4);
	}
	indexFile.close();

	// file에서 sprite 개수를 읽어온다.	
	m_file->open(lpszFilename, std::ios::binary);

	// Check if data file opened successfully
	if (!m_file->is_open() || !m_file->good())
	{
		printf("ERROR: Failed to open data file: %s\n", lpszFilename);
		delete []m_file_index;
		m_file_index = NULL;
		m_bRunningLoad = false;
		return false;
	}

	m_file->read((char*)&m_Size, 2);

	if (!m_file->good())
	{
		printf("ERROR: Failed to read size from data file: %s\n", lpszFilename);
		m_file->close();
		delete m_file;
		m_file = NULL;
		delete []m_file_index;
		m_file_index = NULL;
		m_bRunningLoad = false;
		return false;
	}

	m_bRunningLoad = true;
	m_nLoadData = 0;

//	printf("DEBUG LoadFromFileRunning: Successfully loaded %s (size=%d)\n", lpszFilename, m_Size);

	return true;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::SaveToFile(std::ofstream&dataFile, std::ofstream&indexFile)
{
	//--------------------------------------------------
	// index file을 생성하기 위한 정보
	//--------------------------------------------------
//	long*	pIndex = new long [m_Size];
	std::vector<DWORD> vIndex;

	//--------------------------------------------------
	// Size 저장
	//--------------------------------------------------
	dataFile.write((const char *)&m_Size, 2);
	indexFile.write((const char *)&m_Size, 2);
	WORD realSize = m_Size;
	DWORD index = 0;
	int i;  // Declare at function scope for both loops

	for(i = 0; i < m_Size; i++)
	{
		index = dataFile.tellp();
		if(m_pData[i].SaveToFile(dataFile) == false)
		{
			realSize--;
		}
		else
		{
			vIndex.push_back(index);
		}
	}
	
	if(realSize != m_Size)
	{
		char szTemp[512];
		snprintf(szTemp, sizeof(szTemp), "real size : %d size : %d", realSize, m_Size);
		MessageBox(NULL, szTemp, "CTypePack2", MB_OK);

		dataFile.seekp(0);
		dataFile.write((const char *)&realSize, 2);
		indexFile.seekp(0);
		indexFile.write((const char *)&realSize, 2);
	}

	//--------------------------------------------------
	// index 저장
	//--------------------------------------------------
	for (i=0; i<vIndex.size(); i++)
	{
		indexFile.write((const char*)&vIndex[i], 4);
	}
	
//	delete [] pIndex;
	
//	indexFile.close();
//	dataFile.close();

	if(m_bRunningLoad)
	{
		m_bRunningLoad = false;
		m_file->close();
		delete m_file;
		m_file = NULL;
		delete []m_file_index;
		m_file_index = NULL;
	}

	return true;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFilePart(int first, int last)
{
	last = (std::min)(last, 0xFFFE);
	for(int i = first; i <= last; i++)
		operator[](i);

	return true;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFilePart(const CSpriteSetManager& SSM)
{
	CSpriteSetManager::DATA_LIST::const_iterator iID = SSM.GetIterator();
	for (int t=0; t<SSM.GetSize(); t++)
	{
		if(*iID != 0xFFFF)
			Get(*iID);

		// The iterator has to advance. Without this the loop asked for
		// whatever the first entry named once per pass and never
		// touched the rest of the set.
		++iID;
	}

	return true;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::ReleasePart(int first, int last)
{
	if(m_pData == NULL)
		return false;

	// Release() is called through m_pData[i], so the range has to be
	// clamped to the pack itself. Capping last at 0xFFFE only bounded it
	// by the index type, which let the loop write through elements past
	// the end of the allocation. This is the overload the sprite packs
	// actually instantiate, so it matters more than the CTypePack one.
	if(first < 0)
		first = 0;

	if(last >= (int)m_Size)
		last = (int)m_Size - 1;

	for(int i = first; i <= last; i++)
		m_pData[i].Release();

	return true;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::ReleasePart(COrderedList<TYPE_SPRITEID> list)
{
	if(m_pData == NULL)
		return false;

	COrderedList<TYPE_SPRITEID>::DATA_LIST::const_iterator iID = list.GetIterator();
	for (int t=0; t<list.GetSize(); t++)
	{
		// An entry naming an element outside the pack is skipped rather
		// than written through.
		if(*iID != 0xFFFF && *iID < m_Size)
			m_pData[*iID].Release();

		// The iterator has to advance. Without this the loop released
		// whatever the first entry named once per pass and ignored every
		// other element in the list.
		++iID;
	}

	return true;
}

template <class TypeBase, class Type1, class Type2>
bool CTypePack2<TypeBase, Type1, Type2>::LoadFromFileData(int dataID, int fileID, LPCTSTR packFilename, LPCTSTR indexFilename)
{
	if (!m_pData || dataID < 0 || dataID >= m_Size) return false;
	std::ifstream dataFile;
	if (!CTypePackDetail::OpenIndexedEntry(fileID, packFilename, indexFilename, dataFile))
		return false;
	return m_pData[dataID].LoadFromFile(dataFile);
}

#endif
