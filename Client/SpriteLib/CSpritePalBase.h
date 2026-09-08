#ifndef __CSPRITEPALBASE_H__
#define __CSPRITEPALBASE_H__

#include "MPalette.h"
#ifdef PLATFORM_WINDOWS
	#include <windows.h>
#else
	#include "../basic/Platform.h"
	#include <cstring>
#endif

// Included here rather than only on the non-Windows branch: the
// declarations below name the stream types, so the header has to be
// usable without a precompiled header having pulled them in first.
#include <fstream>

class CSpritePalBase
{
public:
	CSpritePalBase();
	virtual ~CSpritePalBase();
	
	void SetEmptySprite();
	bool IsEmptySprite() const		{ if(IsInit() && m_Size == 0) return true; return false; }
	
	//--------------------------------------------------------
	// Init/Release
	//--------------------------------------------------------
	bool		IsNotInit() const	{ return !m_bInit; }
	bool		IsInit() const		{ return m_bInit; }
	bool		IsLoading() const	{ return m_bLoading; }
	
	//---------------------------------------------------------
	// m_pData의 memory를 Release한다.		
	//---------------------------------------------------------
	void		Release();
	
	//--------------------------------------------------------
	// file I/O
	//--------------------------------------------------------
	bool LoadFromFile(std::ifstream &file);
	bool SaveToFile(std::ofstream &file);
	
	//--------------------------------------------------------
	// Get Functions
	//--------------------------------------------------------
	WORD	GetWidth() const	{ return m_Width; }
	WORD	GetHeight() const	{ return m_Height; }
	
	static void	SetColorKey(BYTE color)		{ s_Colorkey = color; }
	static BYTE	GetColorKey()				{ return s_Colorkey; }
	
	//--------------------------------------------------------
	// operator
	//--------------------------------------------------------
	void		operator = (const CSpritePalBase& Sprite);
	
	//---------------------------------------------------------
	// Blt functions
	//---------------------------------------------------------
	virtual void Blt(int x, int y, WORD* pDest, int pitch, MPalette &pal) = 0;
	
	//---------------------------------------------------------
	// Pixel
	//---------------------------------------------------------
	virtual bool	IsColorPixel(short x,short y) = 0;
	virtual WORD	GetPixel(short x, short y, MPalette &pal) = 0;
	
protected:
	//---------------------------------------------------------
	// Walks every scanline's run length data once, checking that it
	// stays inside the pixel data and decodes to no more than the
	// sprite's width.
	//
	// bytesPerPixel is 1 for a plain palette sprite and 2 for an alpha
	// sprite, which stores an alpha byte alongside each palette index.
	//
	// The blit routines take their transparent and colour runs straight
	// from this data and write into the destination surface from them.
	// There are more than twenty of those routines across the two
	// subclasses, counting every clipping and pixel format variant, so
	// the shape of a scanline is established once here rather than
	// re-checked in each of them.
	//---------------------------------------------------------
	bool		ValidateScanlines(int bytesPerPixel) const;

	//---------------------------------------------------------
	// One allocation for the pixel data and, after it, the scanline
	// pointer table: m_pData gets size bytes and m_pPixels a table of
	// height pointers at the first pointer-aligned offset past them.
	// Every loader and the copy constructor go through this. Placing
	// the table at m_pData + m_Size directly, as they used to, put it
	// at whatever byte offset the pixel data happened to end on, and
	// every read of it was then a misaligned load - undefined
	// behaviour that x86 tolerates and UBSan reports.
	//---------------------------------------------------------
	void		AllocateDataAndScanlineTable(DWORD size, WORD height);

	//---------------------------------------------------------
	// Called by LoadFromFile once the scanline table has been built.
	// Only the subclass knows how many bytes a pixel occupies.
	//---------------------------------------------------------
	virtual bool	ValidateScanlineData() const	{ return true; }

	WORD			m_Width;		// 가로 pixel수
	WORD			m_Height;		// 세로 pixel수		
	DWORD			m_Size;			// 스프라이트의 size
	
	BYTE**			m_pPixels;		// pixels point array
	BYTE*			m_pData;			// data
	
	bool			m_bInit;		// data가 있는가?
	bool			m_bLoading;		// Loading중인가?
	
	static BYTE		s_Colorkey;
	
};

#endif
