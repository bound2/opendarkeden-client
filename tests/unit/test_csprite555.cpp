//----------------------------------------------------------------------
// test_csprite555.cpp
//----------------------------------------------------------------------
//
// Tests for CSprite555::LoadFromFile in Client/SpriteLib/CSprite555.cpp.
//
// The on-disk layout is:
//
//      WORD  width
//      WORD  height
//      per scanline:
//          WORD  len              length of the scanline in WORDs
//          WORD  data[len]        run length encoded pixels
//
// and each scanline's data is:
//
//      data[0]                    number of segments
//      per segment:
//          WORD  transparentRun
//          WORD  colourRun
//          WORD  colour[colourRun]
//
// The loader walks that structure in place, converting each colour from
// 5:6:5 to 5:5:5. Both run lengths come from the file, so the walking
// index has to be kept inside the scanline that was allocated.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "CSprite555.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace {

const char* const	kTempFile = "csprite555_loadfromfile_test.bin";

void	PushWord(std::vector<unsigned char>& bytes, WORD value)
{
	bytes.push_back((unsigned char)(value & 0xFF));
	bytes.push_back((unsigned char)((value >> 8) & 0xFF));
}

void	WriteRawFile(const std::vector<unsigned char>& bytes)
{
	std::ofstream	out(kTempFile, std::ios::binary | std::ios::trunc);

	if (!bytes.empty())
		out.write((const char*)&bytes[0], (std::streamsize)bytes.size());
}

//----------------------------------------------------------------------
// Builds a single scanline sprite from a caller supplied scanline body.
// declaredLen is written as the scanline length, which lets a test
// declare a length that does not match the body it supplies.
//----------------------------------------------------------------------
void	AppendSpriteBytes(std::vector<unsigned char>& bytes, WORD width,
			  WORD height, WORD declaredLen,
			  const std::vector<WORD>& scanline)
{
	PushWord(bytes, width);
	PushWord(bytes, height);
	PushWord(bytes, declaredLen);

	for (size_t i = 0; i < scanline.size(); i++)
		PushWord(bytes, scanline[i]);
}

void	WriteSpriteFile(WORD width, WORD height, WORD declaredLen,
			const std::vector<WORD>& scanline)
{
	std::vector<unsigned char>	bytes;

	AppendSpriteBytes(bytes, width, height, declaredLen, scanline);

	WriteRawFile(bytes);
}

//----------------------------------------------------------------------
// A scanline the loader accepts: one segment, no transparent pixels,
// two colours, consuming the scanline exactly.
//----------------------------------------------------------------------
std::vector<WORD>	GoodScanline()
{
	std::vector<WORD>	scanline;

	scanline.push_back(1);
	scanline.push_back(0);
	scanline.push_back(2);
	scanline.push_back(0xFFFF);
	scanline.push_back(0x07E0);

	return scanline;
}

//----------------------------------------------------------------------
// The same shape but with a colour run that runs off the scanline.
//----------------------------------------------------------------------
std::vector<WORD>	BadScanline()
{
	std::vector<WORD>	scanline;

	scanline.push_back(1);
	scanline.push_back(0);
	scanline.push_back(60000);
	scanline.push_back(0xFFFF);
	scanline.push_back(0x07E0);

	return scanline;
}

void	RemoveTempFile()
{
	std::remove(kTempFile);
}

} // namespace

//----------------------------------------------------------------------
// A well formed scanline loads.
//
// One segment: no transparent pixels, then two colours. The decode
// consumes the scanline exactly.
//----------------------------------------------------------------------
TEST(CSprite555, LoadFromFileReadsAWellFormedSprite)
{
	std::vector<WORD>	scanline;

	scanline.push_back(1);		// one segment
	scanline.push_back(0);		// transparent run
	scanline.push_back(2);		// colour run
	scanline.push_back(0xFFFF);	// colour
	scanline.push_back(0x07E0);	// colour

	WriteSpriteFile(2, 1, (WORD)scanline.size(), scanline);

	CSprite555	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(sprite.LoadFromFile(in));
	CHECK_EQ(2, sprite.GetWidth());
	CHECK_EQ(1, sprite.GetHeight());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A colour run longer than the scanline must be rejected.
//
// The decode loop advanced its index by the colour run taken from the
// file and wrote a converted pixel at every step, with nothing comparing
// the index against the length of the scanline it had allocated. A
// segment claiming 60000 colours inside a five WORD scanline wrote tens
// of thousands of WORDs past the end of the allocation.
//----------------------------------------------------------------------
TEST(CSprite555, LoadFromFileRejectsColourRunPastTheScanline)
{
	std::vector<WORD>	scanline;

	scanline.push_back(1);		// one segment
	scanline.push_back(0);		// transparent run
	scanline.push_back(60000);	// colour run, far beyond the scanline
	scanline.push_back(0xFFFF);
	scanline.push_back(0x07E0);

	WriteSpriteFile(2, 1, (WORD)scanline.size(), scanline);

	CSprite555	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!sprite.LoadFromFile(in));

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A segment count that outruns the scanline must be rejected.
//
// Each segment reads two run lengths before its colours, so a scanline
// claiming more segments than it has room for walked the index past the
// end while reading them.
//----------------------------------------------------------------------
TEST(CSprite555, LoadFromFileRejectsSegmentCountPastTheScanline)
{
	std::vector<WORD>	scanline;

	scanline.push_back(500);	// far more segments than fit
	scanline.push_back(0);
	scanline.push_back(0);

	WriteSpriteFile(2, 1, (WORD)scanline.size(), scanline);

	CSprite555	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!sprite.LoadFromFile(in));

	RemoveTempFile();
}

//----------------------------------------------------------------------
// An empty scanline has no segment count to read.
//
// A declared length of zero allocated a zero element array and the
// loader then read element zero out of it to get the segment count.
//----------------------------------------------------------------------
TEST(CSprite555, LoadFromFileRejectsEmptyScanline)
{
	std::vector<WORD>	scanline;

	WriteSpriteFile(2, 1, 0, scanline);

	CSprite555	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!sprite.LoadFromFile(in));

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A scanline shorter than its declared length is a truncated file.
//----------------------------------------------------------------------
TEST(CSprite555, LoadFromFileRejectsTruncatedScanline)
{
	std::vector<WORD>	scanline;

	scanline.push_back(1);
	scanline.push_back(0);

	// Declares ten WORDs but supplies two.
	WriteSpriteFile(2, 1, 10, scanline);

	CSprite555	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!sprite.LoadFromFile(in));

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A zero sized sprite is a valid empty sprite, not a failure.
//----------------------------------------------------------------------
TEST(CSprite555, LoadFromFileAcceptsZeroSizedSprite)
{
	std::vector<WORD>	scanline;

	std::vector<unsigned char>	bytes;

	PushWord(bytes, 0);
	PushWord(bytes, 0);

	WriteRawFile(bytes);

	CSprite555	sprite;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(sprite.LoadFromFile(in));
	CHECK_EQ(0, sprite.GetWidth());
	CHECK_EQ(0, sprite.GetHeight());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// Rejecting a sprite must leave the stream at the next one.
//
// Sprites are stored back to back in a pack file. Pack loaders record
// rejected entries while continuing through complete records, so the
// stream must advance past each sprite. An earlier bounds check returned
// as soon as it spotted bad data, part way through the sprite, so one
// malformed entry silently destroyed every entry after it.
//----------------------------------------------------------------------
TEST(CSprite555, RejectionLeavesTheStreamAtTheNextSprite)
{
	std::vector<unsigned char>	bytes;

	const std::vector<WORD>		bad	= BadScanline();
	const std::vector<WORD>		good	= GoodScanline();

	AppendSpriteBytes(bytes, 2, 1, (WORD)bad.size(), bad);
	AppendSpriteBytes(bytes, 2, 1, (WORD)good.size(), good);

	WriteRawFile(bytes);

	std::ifstream	in(kTempFile, std::ios::binary);

	CSprite555	first;
	CSprite555	second;

	CHECK(!first.LoadFromFile(in));

	// The second sprite must still be readable.
	CHECK(second.LoadFromFile(in));
	CHECK_EQ(2, second.GetWidth());
	CHECK_EQ(1, second.GetHeight());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// Rejecting a sprite must not brick the object.
//
// LoadFromFile sets m_bLoading on entry and refuses to run while it is
// set, and CSprite::Release() does not clear it. An early return that
// skipped clearing the flag left the object permanently unable to load
// anything, which through CTypePack2's lazy-load retry would go on to
// disable lazy loading for the whole pack.
//----------------------------------------------------------------------
TEST(CSprite555, RejectionDoesNotLatchTheLoadingFlag)
{
	CSprite555	sprite;

	{
		const std::vector<WORD>	bad = BadScanline();

		WriteSpriteFile(2, 1, (WORD)bad.size(), bad);

		std::ifstream	in(kTempFile, std::ios::binary);

		CHECK(!sprite.LoadFromFile(in));
	}

	// The same object must still be able to load a good sprite.
	{
		const std::vector<WORD>	good = GoodScanline();

		WriteSpriteFile(2, 1, (WORD)good.size(), good);

		std::ifstream	in(kTempFile, std::ios::binary);

		CHECK(sprite.LoadFromFile(in));
		CHECK_EQ(2, sprite.GetWidth());
	}

	RemoveTempFile();
}
