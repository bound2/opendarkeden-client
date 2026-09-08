//----------------------------------------------------------------------
// test_cspritepalbase.cpp
//----------------------------------------------------------------------
//
// Tests for CSpritePalBase::LoadFromFile in
// Client/SpriteLib/CSpritePalBase.cpp.
//
// The container layout is:
//
//      DWORD  size      bytes of packed pixel data
//      WORD   width
//      WORD   height
//      BYTE   data[size]
//      WORD   index[height]   per-scanline byte lengths
//
// and each scanline of that data is run length encoded:
//
//      BYTE  segmentCount
//      per segment:
//          BYTE  transparentRun
//          BYTE  colourRun
//          then colourRun pixels, one byte each for a plain palette
//          sprite and two for an alpha sprite
//
// Two separate things are checked here. The loader turns the index
// array into m_pPixels[], a table of pointers into the pixel data, and
// those pointers have to stay inside the allocation. Then the run
// length data those pointers address has to describe a sprite of this
// width, which is what protects the blit routines.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "CSpritePalBase.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace {

//----------------------------------------------------------------------
// CSpritePalBase is abstract. This stands in for a concrete plain
// palette sprite, one byte per pixel, so the parser can be exercised on
// its own. The ValidateScanlineData override is what a real subclass
// supplies, and without it the scanline validation would not run at all.
//----------------------------------------------------------------------
class TestSpritePal : public CSpritePalBase
{
public:
	virtual void	Blt(int, int, WORD*, int, MPalette&)	{}
	virtual bool	IsColorPixel(short, short)		{ return false; }
	virtual WORD	GetPixel(short, short, MPalette&)	{ return 0; }

	//------------------------------------------------------------
	// Every scanline pointer must land inside the pixel data. One
	// past the end is allowed, for an empty trailing scanline.
	//------------------------------------------------------------
	bool	ScanlinePointersAreInsideData() const
	{
		if (m_pData == NULL)
			return true;

		for (int i = 0; i < m_Height; i++)
		{
			if (m_pPixels[i] < m_pData)
				return false;

			if (m_pPixels[i] > m_pData + m_Size)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------
	// The scanline table lives in the same allocation as the pixel
	// data, after it. A pointer table at an odd byte offset is a
	// misaligned load on every read of it - undefined behaviour that
	// x86 tolerates and UBSan reports.
	//------------------------------------------------------------
	bool	ScanlineTableIsAligned() const
	{
		return m_pPixels == NULL ||
			reinterpret_cast<std::uintptr_t>(m_pPixels) % alignof(BYTE*) == 0;
	}

protected:
	virtual bool	ValidateScanlineData() const	{ return ValidateScanlines(1); }
};

const char* const	kTempFile = "cspritepalbase_loadfromfile_test.bin";

void	WriteRawFile(const std::vector<unsigned char>& bytes)
{
	std::ofstream	out(kTempFile, std::ios::binary | std::ios::trunc);

	if (!bytes.empty())
		out.write((const char*)&bytes[0], (std::streamsize)bytes.size());
}

//----------------------------------------------------------------------
// One run length encoded scanline: a single segment of transparentRun
// transparent pixels followed by colourRun opaque ones.
//----------------------------------------------------------------------
std::vector<unsigned char>	MakeScanline(int transparentRun, int colourRun)
{
	std::vector<unsigned char>	scanline;

	scanline.push_back(1);					// one segment
	scanline.push_back((unsigned char)transparentRun);
	scanline.push_back((unsigned char)colourRun);

	for (int i = 0; i < colourRun; i++)
		scanline.push_back((unsigned char)(i + 1));	// palette index

	return scanline;
}

//----------------------------------------------------------------------
// Assembles the container around a set of scanlines. declaredSize and
// the index entries can be overridden so a test can describe something
// the file does not actually contain.
//----------------------------------------------------------------------
void	WriteSprite(WORD width, const std::vector<std::vector<unsigned char> >& scanlines,
		    const std::vector<WORD>* indexOverride = NULL,
		    const DWORD* sizeOverride = NULL,
		    int dataBytesOverride = -1)
{
	std::vector<unsigned char>	data;

	std::vector<WORD>		index;

	for (size_t i = 0; i < scanlines.size(); i++)
	{
		index.push_back((WORD)scanlines[i].size());

		for (size_t j = 0; j < scanlines[i].size(); j++)
			data.push_back(scanlines[i][j]);
	}

	const DWORD	size	= (sizeOverride != NULL) ? *sizeOverride : (DWORD)data.size();
	const WORD	height	= (WORD)scanlines.size();

	std::vector<unsigned char>	bytes;

	bytes.resize(8);

	std::memcpy(&bytes[0], &size, 4);
	std::memcpy(&bytes[4], &width, 2);
	std::memcpy(&bytes[6], &height, 2);

	const int	dataBytes =
		(dataBytesOverride >= 0) ? dataBytesOverride : (int)data.size();

	for (int i = 0; i < dataBytes && i < (int)data.size(); i++)
		bytes.push_back(data[i]);

	const std::vector<WORD>&	written =
		(indexOverride != NULL) ? *indexOverride : index;

	for (size_t i = 0; i < written.size(); i++)
	{
		bytes.push_back((unsigned char)(written[i] & 0xFF));
		bytes.push_back((unsigned char)((written[i] >> 8) & 0xFF));
	}

	WriteRawFile(bytes);
}

void	RemoveTempFile()
{
	std::remove(kTempFile);
}

} // namespace

//----------------------------------------------------------------------
// A well formed sprite loads: two scanlines, each decoding to exactly
// the sprite's width.
//----------------------------------------------------------------------
TEST(CSpritePalBase, LoadFromFileReadsAWellFormedSprite)
{
	std::vector<std::vector<unsigned char> >	scanlines;

	scanlines.push_back(MakeScanline(0, 4));
	scanlines.push_back(MakeScanline(2, 2));

	WriteSprite(4, scanlines);

	TestSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(sprite.LoadFromFile(in));
	CHECK_EQ(4, sprite.GetWidth());
	CHECK_EQ(2, sprite.GetHeight());
	CHECK(sprite.ScanlinePointersAreInsideData());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// The pointer table is placed after the pixel data in one allocation.
// With 13 bytes of pixel data (7 + 6) the table would start at an odd
// address unless the loader pads up to the pointer alignment.
//----------------------------------------------------------------------
TEST(CSpritePalBase, LoadFromFileAlignsTheScanlineTable)
{
	std::vector<std::vector<unsigned char> >	scanlines;

	scanlines.push_back(MakeScanline(0, 4));	// 1 + 2 + 4 = 7 bytes
	scanlines.push_back(MakeScanline(1, 3));	// 1 + 2 + 3 = 6 bytes

	WriteSprite(4, scanlines);

	TestSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(sprite.LoadFromFile(in));
	CHECK(sprite.ScanlineTableIsAligned());
	CHECK(sprite.ScanlinePointersAreInsideData());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A fully transparent scanline is legitimate and must be accepted.
//----------------------------------------------------------------------
TEST(CSpritePalBase, LoadFromFileAcceptsAFullyTransparentScanline)
{
	std::vector<std::vector<unsigned char> >	scanlines;

	scanlines.push_back(MakeScanline(4, 0));

	WriteSprite(4, scanlines);

	TestSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(sprite.LoadFromFile(in));
	CHECK(sprite.ScanlinePointersAreInsideData());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A scanline decoding to more pixels than the sprite is wide must be
// rejected. This is the check that protects the blit routines, and it
// only runs because the subclass supplies ValidateScanlineData.
//----------------------------------------------------------------------
TEST(CSpritePalBase, LoadFromFileRejectsScanlineWiderThanTheSprite)
{
	std::vector<std::vector<unsigned char> >	scanlines;

	// 200 transparent plus 50 colour pixels in a sprite four wide.
	scanlines.push_back(MakeScanline(200, 50));

	WriteSprite(4, scanlines);

	TestSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!sprite.LoadFromFile(in));

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A colour run that runs off the end of the scanline data is rejected.
//----------------------------------------------------------------------
TEST(CSpritePalBase, LoadFromFileRejectsColourRunPastTheScanlineData)
{
	std::vector<unsigned char>	scanline;

	scanline.push_back(1);		// one segment
	scanline.push_back(0);		// transparent run
	scanline.push_back(4);		// claims four colours
	scanline.push_back(1);		// supplies one

	std::vector<std::vector<unsigned char> >	scanlines;

	scanlines.push_back(scanline);

	WriteSprite(4, scanlines);

	TestSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!sprite.LoadFromFile(in));

	RemoveTempFile();
}

//----------------------------------------------------------------------
// Scanline offsets that run past the pixel data must be rejected.
//
// The loader accumulated the index entries into a pointer without ever
// comparing the running total against the size of the data it had
// allocated, so a file supplying large counts produced scanline
// pointers aimed far outside the buffer.
//----------------------------------------------------------------------
TEST(CSpritePalBase, LoadFromFileRejectsScanlineOffsetsPastTheData)
{
	std::vector<std::vector<unsigned char> >	scanlines;

	scanlines.push_back(MakeScanline(0, 4));
	scanlines.push_back(MakeScanline(0, 4));

	std::vector<WORD>	index;

	index.push_back(60000);
	index.push_back(60000);

	WriteSprite(4, scanlines, &index);

	TestSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	const bool	loaded = sprite.LoadFromFile(in);

	// Whether or not the load is reported as successful, no scanline
	// pointer may be left aimed outside the allocation.
	CHECK(sprite.ScanlinePointersAreInsideData());
	CHECK(!loaded);

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A declared data size the file cannot supply must be rejected before
// it is used to size an allocation.
//----------------------------------------------------------------------
TEST(CSpritePalBase, LoadFromFileRejectsSizeLargerThanTheFile)
{
	std::vector<std::vector<unsigned char> >	scanlines;

	scanlines.push_back(MakeScanline(0, 4));

	const DWORD	hugeSize = 100000;

	WriteSprite(4, scanlines, NULL, &hugeSize);

	TestSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!sprite.LoadFromFile(in));
	CHECK(sprite.ScanlinePointersAreInsideData());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A file that cannot supply the index array is rejected too.
//----------------------------------------------------------------------
TEST(CSpritePalBase, LoadFromFileRejectsMissingIndexArray)
{
	std::vector<std::vector<unsigned char> >	scanlines;

	scanlines.push_back(MakeScanline(0, 4));

	// Declares four scanlines but supplies one index entry.
	std::vector<WORD>	index;

	index.push_back(7);

	std::vector<std::vector<unsigned char> >	padded = scanlines;

	padded.push_back(MakeScanline(0, 4));
	padded.push_back(MakeScanline(0, 4));
	padded.push_back(MakeScanline(0, 4));

	WriteSprite(4, padded, &index);

	TestSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!sprite.LoadFromFile(in));
	CHECK(sprite.ScanlinePointersAreInsideData());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A file too short to hold the header is rejected.
//----------------------------------------------------------------------
TEST(CSpritePalBase, LoadFromFileRejectsTruncatedHeader)
{
	std::vector<unsigned char>	bytes;

	bytes.push_back(0x08);
	bytes.push_back(0x00);

	WriteRawFile(bytes);

	TestSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!sprite.LoadFromFile(in));
	CHECK(sprite.ScanlinePointersAreInsideData());

	RemoveTempFile();
}
