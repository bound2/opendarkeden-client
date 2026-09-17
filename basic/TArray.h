//----------------------------------------------------------------------
// TArray.h
//----------------------------------------------------------------------
//
// Template Array class
//
//----------------------------------------------------------------------
//
// Data Type과  Size Type이 Template이다.
//
// File I/O를 하려면  Data가 되는 class에 
//      bool		SaveToFile(std::ofstream& file);
//		bool		LoadFromFile(std::ifstream& file);  이 구현되어야 한다.
//
//----------------------------------------------------------------------

#ifndef	__TARRAY_H__
#define	__TARRAY_H__


#include "Platform.h"
#include <fstream>

template <class DataType, class SizeType>
class TArray {
	public :
		TArray(SizeType size=0);

		// TArray owns m_pData and frees it in the destructor, so it
		// needs a copy constructor. The compiler-supplied one copies
		// the pointer, which shares one buffer between both instances
		// and double frees it.
		TArray(const TArray<DataType, SizeType>& array);

		~TArray();

		//--------------------------------------------------------
		// Init / Release
		//--------------------------------------------------------
		void		Init(SizeType size);
		void		Release();
	

		//--------------------------------------------------------
		// Get Size
		//--------------------------------------------------------
		SizeType	GetSize() const				{ return m_Size; }
	
		//--------------------------------------------------------
		// File I/O
		//--------------------------------------------------------
		bool		SaveToFile(std::ofstream& file);
		bool		LoadFromFile(std::ifstream& file);

		//--------------------------------------------------------
		// Operator overloading
		//--------------------------------------------------------
		DataType&	operator [] (SizeType n)		{ return m_pData[n]; }
		const DataType&	operator [] (SizeType n) const	{ return m_pData[n]; }
		void		operator = (const TArray<DataType, SizeType>& array);
		
		// 현재 Array에 다른 array를 더한다.
		void		operator += (const TArray<DataType, SizeType>& array);


	protected :
		SizeType		m_Size;
		DataType*		m_pData;

		// sizeof(SizeType) 의 값
		static BYTE		s_SIZEOF_SizeType;
};




//----------------------------------------------------------------------
//
// Initialize static data member
//
//----------------------------------------------------------------------
template <class DataType, class SizeType>
BYTE	TArray<DataType, SizeType>::s_SIZEOF_SizeType = sizeof(SizeType);


//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------
template <class DataType, class SizeType> 
TArray<DataType, SizeType>::TArray(SizeType size)
{
	m_Size	= 0;
	m_pData = NULL;	

	Init(size);
}

template <class DataType, class SizeType>
TArray<DataType, SizeType>::TArray(const TArray<DataType, SizeType>& array)
{
	m_Size	= 0;
	m_pData	= NULL;

	Init(array.m_Size);

	for (SizeType i=0; i<m_Size; i++)
		m_pData[i] = array.m_pData[i];
}

template <class DataType, class SizeType>
TArray<DataType, SizeType>::~TArray()
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
template <class DataType, class SizeType> 
void	
TArray<DataType, SizeType>::Init(SizeType size)
{
	// 일단 해제
	Release();

	if (size==0) return;

	m_Size = size;
	m_pData = new DataType [m_Size];
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
template <class DataType, class SizeType> 
void	
TArray<DataType, SizeType>::Release()
{
	if (m_pData!=NULL)
	{
		delete [] m_pData;
		m_pData = NULL;
		
		m_Size = 0;
	}
}

//----------------------------------------------------------------------
// Add Array to *this
//----------------------------------------------------------------------
// 두 Array에 존재하는 data의 개수를 더한만큼의 
// memory를 *this에 다시 잡고 copy~~
//----------------------------------------------------------------------
template <class DataType, class SizeType>
void
TArray<DataType, SizeType>::operator += (const TArray<DataType, SizeType>& array)
{
	//------------------------------------------------
	// The combined count is worked out in a wider type first.
	// Assigning the sum straight into SizeType truncates it for narrow
	// instantiations - for a BYTE, 200 + 100 becomes 44 - which would
	// size the allocation below smaller than the number of elements the
	// copy loops go on to write.
	//------------------------------------------------
	const unsigned long long	combinedSize	=
		(unsigned long long)m_Size + (unsigned long long)array.m_Size;

	const SizeType			newSize		= (SizeType)combinedSize;

	//------------------------------------------------
	// Refuse the append rather than overrun the allocation when the
	// result cannot be represented in SizeType. *this is left unchanged.
	//------------------------------------------------
	if ((unsigned long long)newSize != combinedSize)
		return;

	//------------------------------------------------
	// Allocate room for the elements of both arrays.
	//------------------------------------------------
	DataType*	pTempData = new DataType [newSize];
	
	//------------------------------------------------
	// temp에 *this를 copy
	//------------------------------------------------
	SizeType k=0;
	for (SizeType i=0; i<m_Size; i++)
	{
		pTempData[k] = m_pData[i];		
		
		k++;
	}

	//------------------------------------------------
	// temp에 FramePack을 copy
	//------------------------------------------------
	for (SizeType i=0; i<array.m_Size; i++)
	{
		pTempData[k] = array.m_pData[i];

		k++;
	}

	//------------------------------------------------
	// memory해제한다.
	//------------------------------------------------
	Release();

	//------------------------------------------------
	// *this가 temp를 가리키도록 한다.
	//------------------------------------------------
	m_Size		= newSize;
	m_pData		= pTempData;	
}


//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
template <class DataType, class SizeType> 
bool
TArray<DataType, SizeType>::SaveToFile(std::ofstream& file)
{
	// 0이라도 개수는 저장한다.
	file.write((const char*)&m_Size, s_SIZEOF_SizeType);

	// 아무것도 없으면..
	if (m_pData==NULL || m_Size==0) 
		return false;

	for (SizeType i=0; i<m_Size; i++)
		m_pData[i].SaveToFile(file);

	return true;
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
template <class DataType, class SizeType> 
bool
TArray<DataType, SizeType>::LoadFromFile(std::ifstream& file)
{
	// Start from a known state so a rejected file leaves an empty array
	// behind rather than stale storage.
	Release();

	// The count is read into a local. Reading straight into m_Size lets
	// a short read overwrite part of the member, and leaves the previous
	// value in place when the read fails outright, so the load would
	// carry on with a size the file never specified.
	SizeType	count = 0;

	file.read((char*)&count, s_SIZEOF_SizeType);

	if (!file)
		return false;

	if (count==0) return false;

	// Reject a count the file cannot hold. Every element consumes at
	// least one byte, so a count larger than the number of bytes
	// remaining is corrupt, and sizing the allocation from it would read
	// the rest of the elements from a dead stream.
	const std::streampos	afterCount	= file.tellg();

	file.seekg(0, std::ios::end);

	const std::streampos	endOfFile	= file.tellg();

	file.seekg(afterCount, std::ios::beg);

	if (!file)
		return false;

	if ((unsigned long long)count >
	    (unsigned long long)(endOfFile - afterCount))
		return false;

	// Allocate storage.
	Init(count);

	for (SizeType i=0; i<m_Size; i++)
	{
		if (!file)
		{
			Release();
			return false;
		}

		m_pData[i].LoadFromFile(file);
	}

	return true;
}

//----------------------------------------------------------------------
// assign
//----------------------------------------------------------------------
template <class DataType, class SizeType> 
void	
TArray<DataType, SizeType>::operator = (const TArray<DataType, SizeType>& array)
{
	// Init() below releases the current buffer before allocating. When
	// the source and the destination are the same object that release
	// also destroys the source, and the copy loop would then read the
	// freshly allocated, uninitialised memory back over itself.
	if (this == &array)
		return;

	// Match the source array.
	Init( array.m_Size );

	// Copy every element.
	for (SizeType i=0; i<m_Size; i++)
	{
		m_pData[i] = array.m_pData[i];
	}
}


#endif

