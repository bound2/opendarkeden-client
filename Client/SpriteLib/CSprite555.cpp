//----------------------------------------------------------------------
// CSprite555.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "CSpriteSurface.h"
#include "CSprite.h"
#include "CSprite555.h"

//extern BYTE	LOADING_STATUS_NONE;
//extern BYTE	LOADING_STATUS_NOW;
//extern BYTE	LOADING_STATUS_LOADING;

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// fstream에 save 한다.    ( file에는 5:6:5로 저장한다. )
//----------------------------------------------------------------------
bool
CSprite555::SaveToFile(ofstream& file)
{
	// width와 height를 저장한다.
	file.write((const char*)&m_Width , 2);
	file.write((const char*)&m_Height, 2);

	// NULL이면 저장하지 않는다. 길이만 저장되는 것이다.
	if (m_Pixels==NULL || m_Width==0 || m_Height==0)
		return false;
	
	// 압축 된 것 저장
	WORD index;	

	int i;
	int j;
	int k;

	//--------------------------------
	// 5:5:5
	//--------------------------------
	for (int i=0; i<m_Height; i++)
	{
		// 반복 회수의 2 byte
		int	count = m_Pixels[i][0], 
				colorCount;
		index	= 1;

		// 각 line마다 byte수를 세어서 저장해야한다.
		for (j=0; j<count; j++)
		{
			//transCount = m_Pixels[i][index];
			colorCount = m_Pixels[i][index+1];				

			index+=2;	// 두 count 만큼

			// m_Pixels[i][index] ~ m_Pixels[i][index+colorCount-1]
			// 5:5:5를 5:6:5로 바꿔서 저장하고 다시 5:5:5로 바꿔준다.
			for (k=0; k<colorCount; k++)								
			{
				m_Pixels[i][index] = ColorDraw::Convert555to565(m_Pixels[i][index]);
				index++;
			}

			//index += colorCount;	// 투명색 아닌것만큼 +				
		}

		// byte수와 실제 data를 저장한다.
		file.write((const char*)&index, 2);			
		file.write((const char*)m_Pixels[i], index<<1);			


		// 다시 5:5:5로 바꿔준다.						
		index	= 1;
			
		for (j=0; j<count; j++)
		{
			//transCount = m_Pixels[i][index];
			colorCount = m_Pixels[i][index+1];				

			index+=2;	// 두 count 만큼

			// m_Pixels[i][index] ~ m_Pixels[i][index+colorCount-1]
			// 5:5:5를 5:6:5로 바꿔서 저장하고 다시 5:5:5로 바꿔준다.
			for (k=0; k<colorCount; k++)								
			{					
				m_Pixels[i][index] = ColorDraw::Convert565to555(m_Pixels[i][index]);
				index++;
			}

			//index += colorCount;	// 투명색 아닌것만큼 +				
		}
	}

	return true;
}

//----------------------------------------------------------------------
// fstream에서 load한다.
//----------------------------------------------------------------------
bool	
CSprite555::LoadFromFile(ifstream& file)
{
	// 이거를 하나로 묶어야 하는데..
	if (m_bLoading) 
	{	
		return false;
	}
	m_bLoading = true;

	// F --> T
	/*
	static BYTE LoadingStatus = LOADING_STATUS_NONE;
	BYTE*	pCheck = &LoadingStatus;
	
	// Loading하고 있지 않은 상태인 경우
	// Loading할려는 상태로 만든다.
	InterlockedCompareExchange( 
		(PVOID *)&pCheck,  // pointer to the destination pointer
		(PVOID)&LOADING_STATUS_NOW,      // the exchange value
		(PVOID)&LOADING_STATUS_NONE		// the value to compare
	);
 
	// 지금 loading할것이 아니면 return
	if (LoadingStatus!=LOADING_STATUS_NOW)
	{
		return false;
	}	

	LoadingStatus = LOADING_STATUS_LOADING;
	*/
	// loadind중이라고 표시
	m_bLoading = true;
	

	

	// 이미 잡혀있는 memory를 release한다.
	Release();

	// width와 height를 저장한다.
	file.read((char*)&m_Width , 2);
	file.read((char*)&m_Height, 2);	

	// 길이가 0이면 더 Load할게 없겠지..
	if (m_Width==0 || m_Height==0) 
	{	
		m_bInit = true;

		m_bLoading = false;

		return true;
	}

	//---------------------------------
	// for OLD version of CSprite
	//---------------------------------
	//BOOL dummy;
	//file.read((char*)&dummy, 1);	
	//---------------------------------
	
	m_Pixels = new WORD* [m_Height];

	// Cleared up front so Release() is safe if a scanline below is
	// rejected: it walks every row of this array and frees it, and would
	// otherwise be handed the uninitialised tail.
	for (int i=0; i<m_Height; i++)
		m_Pixels[i] = NULL;

	WORD len;

	//--------------------------------
	// 5:5:5
	//--------------------------------
	// The data was stored as 5:6:5, so it is converted to 5:5:5 here.
	WORD index;
	int	count, colorCount;

	int j;
	int k;

	// Scanline lengths are kept for the decode pass below.
	WORD*	pLengths = new WORD [m_Height];

	for (int i=0; i<m_Height; i++)
		pLengths[i] = 0;

	bool	bValid = true;

	//--------------------------------
	// Pass one: read the whole sprite.
	//
	// Sprites are stored back to back in a pack file, and the callers
	// that load them ignore the return value and rely on the stream
	// having advanced to the next sprite. Reading every scanline before
	// anything is validated keeps that true, so rejecting a malformed
	// sprite cannot desynchronise the ones that follow it.
	//--------------------------------
	for (int i=0; i<m_Height; i++)
	{
		file.read((char*)&len, 2);

		if (!file)
		{
			bValid = false;
			break;
		}

		pLengths[i] = len;

		// A zero length scanline carries no segment count; it is
		// rejected in the decode pass rather than here, so the read
		// loop stays a straight pass over the file.
		if (len==0)
			continue;

		m_Pixels[i] = new WORD [len];

		file.read((char*)m_Pixels[i], len<<1);

		if (!file)
		{
			bValid = false;
			break;
		}
	}

	//--------------------------------
	// Pass two: decode in place.
	//
	// The stream is not touched here, so a rejection leaves the
	// position at the end of this sprite.
	//--------------------------------
	for (int i=0; bValid && i<m_Height; i++)
	{
		len = pLengths[i];

		// A scanline has to carry at least the segment count that is
		// read from element zero below.
		if (len==0)
		{
			bValid = false;
			break;
		}

		count = m_Pixels[i][0];
		index = 1;

		for (j=0; j<count; j++)
		{
			// Both run lengths are read before the colours, so they
			// have to lie inside the scanline. The counts come from the
			// file, so nothing else bounds this walk.
			if (index+1 >= len)
			{
				bValid = false;
				break;
			}

			//transCount = m_Pixels[i][index];
			colorCount = m_Pixels[i][index+1];

			index+=2;	// past both counts

			// The colour run has to fit in what is left of the
			// scanline, otherwise the conversion loop below writes past
			// the end of the allocation.
			if (index+colorCount > (int)len)
			{
				bValid = false;
				break;
			}

			// m_Pixels[i][index] ~ m_Pixels[i][index+colorCount-1]
			// Converted from 5:6:5 to 5:5:5 in place.
			for (k=0; k<colorCount; k++)
			{
				m_Pixels[i][index] = ColorDraw::Convert565to555(m_Pixels[i][index]);
				index++;
			}
		}
	}

	delete [] pLengths;

	if (!bValid)
	{
		Release();

		// Release() does not clear m_bLoading, and the guard at the top
		// of this function refuses to run again while it is set, so
		// leaving it would make one bad sprite permanently unloadable.
		m_bLoading = false;

		return false;
	}

	m_bInit = true;

	m_bLoading = false;
//	LoadingStatus = LOADING_STATUS_NONE;

	return true;
}


//----------------------------------------------------------------------
// fstream에서 load한다.
//----------------------------------------------------------------------
/*
bool	
CSprite555::LoadFromFileToBuffer(ifstream& file)
{
	// width와 height를 저장한다.
	file.read((char*)&s_Width , 2);
	file.read((char*)&s_Height, 2);	

	// 길이가 0이면 더 Load할게 없겠지..
	if (s_Width==0 || s_Height==0) 
		return false;

	//---------------------------------
	// for OLD version of CSprite
	//---------------------------------
	//BOOL dummy;
	//file.read((char*)&dummy, 1);	
	//---------------------------------
	
	//--------------------------------
	// 5:5:5
	//--------------------------------
	// 5:6:5로 저장된걸 읽었기 때문에 5:6:5를 5:5:5로 바꿔줘야 한다.	
	WORD	count, index, colorCount;

	for (int i=0; i<s_Height; i++)
	{			
		// byte수와 실제 data를 Load한다.
		file.read((char*)&s_BufferLen[i], 2);
		
		file.read((char*)s_Buffer[i], s_BufferLen[i]<<1);

		// converter to 5:5:5
		count = s_Buffer[i][0];			
		index = 1;

		for (int j=0; j<count; j++)
		{
			//transCount = s_Buffer[i][index];
			colorCount = s_Buffer[i][index+1];				

			index+=2;	// 두 count 만큼

			// s_Buffer[i][index] ~ s_Buffer[i][index+colorCount-1]
			// 5:5:5를 5:6:5로 바꿔서 저장하고 다시 5:5:5로 바꿔준다.
			for (int j=0; j<colorCount; j++)								
			{					
				s_Buffer[i][index] = ColorDraw::Convert565to555(s_Buffer[i][index]);
				index++;
			}

			//index += colorCount;	// 투명색 아닌것만큼 +				
		}
	}


	return true;
}
*/

