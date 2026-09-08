//----------------------------------------------------------------------
// test_calphaspritepal.cpp
//----------------------------------------------------------------------
//
// Tests for CAlphaSpritePal in Client/SpriteLib/CAlphaSpritePal.cpp.
//
// Scanline layout, all counts one byte:
//
//      BYTE  segmentCount
//      per segment:
//          BYTE  transparentRun
//          BYTE  colourRun
//          then colourRun pairs of (alpha, paletteIndex)
//
// A scanline encodes exactly the sprite's width in pixels, and every
// scanline carries at least its segment count byte. Both are guaranteed
// by the encoder in SetPixel.
//
// There are two layers of defence here and they are tested separately.
// LoadFromFile rejects a sprite whose scanlines do not decode to that
// shape, which protects every one of the two dozen blit routines at
// once. Blt additionally bounds its own walk, which is tested by
// installing malformed scanlines directly rather than through the
// loader.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "CAlphaSpritePal.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace {

const char* const	kTempFile = "calphaspritepal_test.bin";

const WORD	kSentinel	= 0xDEAD;
const int	kSpriteWidth	= 4;
const int	kGuardWords	= 512;

void	PushWord(std::vector<unsigned char>& bytes, WORD value)
{
	bytes.push_back((unsigned char)(value & 0xFF));
	bytes.push_back((unsigned char)((value >> 8) & 0xFF));
}

void	PushDword(std::vector<unsigned char>& bytes, DWORD value)
{
	bytes.push_back((unsigned char)(value & 0xFF));
	bytes.push_back((unsigned char)((value >> 8) & 0xFF));
	bytes.push_back((unsigned char)((value >> 16) & 0xFF));
	bytes.push_back((unsigned char)((value >> 24) & 0xFF));
}

//----------------------------------------------------------------------
// Builds one segment: a transparent run, a colour run, and the (alpha,
// palette index) pairs the colour run promises.
//----------------------------------------------------------------------
std::vector<unsigned char>	MakeScanline(int transparentRun, int colourRun)
{
	std::vector<unsigned char>	scanline;

	scanline.push_back(1);				// one segment
	scanline.push_back((unsigned char)transparentRun);
	scanline.push_back((unsigned char)colourRun);

	for (int i = 0; i < colourRun; i++)
	{
		scanline.push_back(0x20);		// alpha
		scanline.push_back(0x01);		// palette index
	}

	return scanline;
}

//----------------------------------------------------------------------
// Writes a single scanline sprite in the CSpritePalBase container
// format: size, width, height, the pixel data, then one WORD per
// scanline giving that scanline's length in bytes.
//----------------------------------------------------------------------
void	WriteSprite(const std::vector<unsigned char>& scanline)
{
	std::vector<unsigned char>	bytes;

	PushDword(bytes, (DWORD)scanline.size());
	PushWord(bytes, (WORD)kSpriteWidth);
	PushWord(bytes, 1);

	for (size_t i = 0; i < scanline.size(); i++)
		bytes.push_back(scanline[i]);

	PushWord(bytes, (WORD)scanline.size());

	std::ofstream	out(kTempFile, std::ios::binary | std::ios::trunc);

	out.write((const char*)&bytes[0], (std::streamsize)bytes.size());
}

void	RemoveTempFile()
{
	std::remove(kTempFile);
}

//----------------------------------------------------------------------
// Installs scanline data directly, bypassing LoadFromFile, so Blt's own
// bound can be exercised on data the loader would now refuse.
//----------------------------------------------------------------------
class InjectableAlphaSprite : public CAlphaSpritePal
{
public:
	void	InstallScanline(WORD width, const std::vector<unsigned char>& scanline)
	{
		Release();

		m_Width		= width;
		m_Height	= 1;
		m_Size		= (DWORD)scanline.size();

		AllocateDataAndScanlineTable(m_Size, m_Height);

		std::memcpy(m_pData, &scanline[0], m_Size);

		m_pPixels[0]	= m_pData;

		m_bInit		= true;
	}
};

//----------------------------------------------------------------------
// Destination surface with a sentinel filled guard band to the right of
// the sprite.
//----------------------------------------------------------------------
class GuardedSurface
{
public:
	GuardedSurface() : m_Pixels(kSpriteWidth + kGuardWords, kSentinel) {}

	WORD*	Data()		{ return &m_Pixels[0]; }
	WORD	Pitch() const	{ return (WORD)(m_Pixels.size() * sizeof(WORD)); }

	//------------------------------------------------------------
	// Nothing beyond the sprite's own width may have been written.
	//------------------------------------------------------------
	bool	GuardBandIntact() const
	{
		for (size_t i = kSpriteWidth; i < m_Pixels.size(); i++)
		{
			if (m_Pixels[i] != kSentinel)
				return false;
		}

		return true;
	}

private:
	std::vector<WORD>	m_Pixels;
};

//----------------------------------------------------------------------
// A palette large enough for every index the test sprites use.
//----------------------------------------------------------------------
void	FillPalette(MPalette& pal)
{
	pal.Init(255);

	for (int i = 0; i < 255; i++)
		pal[(BYTE)i] = (WORD)(i * 7);
}

} // namespace

//----------------------------------------------------------------------
// A sprite whose scanline fills its declared width loads and draws.
//----------------------------------------------------------------------
TEST(CAlphaSpritePal, LoadsAndDrawsAWellFormedSprite)
{
	WriteSprite(MakeScanline(0, kSpriteWidth));

	CAlphaSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(sprite.LoadFromFile(in));
	CHECK_EQ(kSpriteWidth, sprite.GetWidth());

	MPalette	pal;

	FillPalette(pal);

	GuardedSurface	surface;

	sprite.Blt(surface.Data(), surface.Pitch(), pal);

	CHECK(surface.GuardBandIntact());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A scanline that decodes to more pixels than the sprite is wide must
// be rejected by the loader.
//
// This is the check that protects every blit routine at once. The
// clipping variants walk the same run lengths as Blt and write into the
// destination surface from them, and there are more than twenty of them
// across CAlphaSpritePal and CSpritePal, so the shape of a scanline is
// established once here rather than re-checked in each of them.
//----------------------------------------------------------------------
TEST(CAlphaSpritePal, LoadFromFileRejectsScanlineWiderThanTheSprite)
{
	// 200 transparent plus 50 colour pixels in a sprite four wide.
	WriteSprite(MakeScanline(200, 50));

	CAlphaSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!sprite.LoadFromFile(in));

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A colour run that runs past the end of the scanline data is rejected
// by the loader too.
//----------------------------------------------------------------------
TEST(CAlphaSpritePal, LoadFromFileRejectsColourRunPastTheScanlineData)
{
	std::vector<unsigned char>	scanline;

	scanline.push_back(1);		// one segment
	scanline.push_back(0);		// transparent run
	scanline.push_back(200);	// colour run, with no colours behind it

	WriteSprite(scanline);

	CAlphaSpritePal	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!sprite.LoadFromFile(in));

	RemoveTempFile();
}

//----------------------------------------------------------------------
// Blt bounds its own walk as well, which is checked here on data
// installed directly rather than through the loader.
//
// Blt added the transparent run straight to the destination pointer
// with nothing comparing it against the sprite's width, so the colours
// that followed were written far to the right of where the sprite ends.
//----------------------------------------------------------------------
TEST(CAlphaSpritePal, BltRejectsTransparentRunWiderThanTheSprite)
{
	InjectableAlphaSprite	sprite;

	sprite.InstallScanline(kSpriteWidth, MakeScanline(200, 50));

	MPalette	pal;

	FillPalette(pal);

	GuardedSurface	surface;

	sprite.Blt(surface.Data(), surface.Pitch(), pal);

	CHECK(surface.GuardBandIntact());
}

//----------------------------------------------------------------------
// The same for an oversized colour run.
//----------------------------------------------------------------------
TEST(CAlphaSpritePal, BltRejectsColourRunWiderThanTheSprite)
{
	InjectableAlphaSprite	sprite;

	sprite.InstallScanline(kSpriteWidth, MakeScanline(0, 200));

	MPalette	pal;

	FillPalette(pal);

	GuardedSurface	surface;

	sprite.Blt(surface.Data(), surface.Pitch(), pal);

	CHECK(surface.GuardBandIntact());
}
