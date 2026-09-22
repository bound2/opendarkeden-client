#include "test_framework.h"
#include "CIndexSprite555.h"
#include "CIndexSprite565.h"
#include "ColorDraw.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {
constexpr const char* path = "index_sprite_loading_test.bin";
// One transparent pixel, two palette pixels, then one fixed color.
const std::vector<WORD> valid{4, 1, 7, 1, 1, 2, 0x010a, 0xff1d, 1, 0x07e0};

void Write(const std::vector<WORD>& words, size_t byteLimit = SIZE_MAX)
{
	std::vector<char> bytes;
	for (WORD word : words) { bytes.push_back(char(word)); bytes.push_back(char(word >> 8)); }
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), std::streamsize((std::min)(bytes.size(), byteLimit)));
}

bool Load(CIndexSprite& sprite, const std::vector<WORD>& words, size_t byteLimit = SIZE_MAX)
{
	Write(words, byteLimit);
	std::ifstream in(path, std::ios::binary);
	const bool loaded = sprite.LoadFromFile(in);
	in.close();
	std::remove(path);
	return loaded;
}

template<class Sprite> void RejectTruncation()
{
	Sprite sprite;
	for (size_t bytes = 0; bytes < valid.size() * 2; ++bytes) {
		CHECK(Load(sprite, {0, 3}));
		CHECK(!Load(sprite, valid, bytes));
		CHECK(!sprite.IsInit());
		CHECK_EQ(0, sprite.GetWidth());
		CHECK_EQ(0, sprite.GetHeight());
	}
	CHECK(Load(sprite, valid));
	CHECK_EQ(4, sprite.GetWidth());
	CHECK_EQ(1, sprite.GetHeight());
}

template<class Sprite> void RejectMalformedRows()
{
	Sprite sprite;
	for (const auto& words : {
		std::vector<WORD>{4, 1, 0}, // missing segment count
		std::vector<WORD>{4, 1, 2, 1, 0}, // missing indexed count
		std::vector<WORD>{4, 1, 4, 1, 0, 2, 0x010a}, // missing indexed data/fixed count
		std::vector<WORD>{4, 1, 5, 1, 0, 0, 2, 0x07e0}, // missing fixed color
		std::vector<WORD>{4, 1, 4, 2, 0, 0, 0}, // missing second segment
		std::vector<WORD>{4, 1, 4, 1, 5, 0, 0}, // transparent run exceeds width
		std::vector<WORD>{4, 1, 5, 1, 4, 1, 0x010a, 0}, // indexed run exceeds width
		std::vector<WORD>{4, 1, 5, 1, 4, 0, 1, 0x07e0}, // fixed run exceeds width
		std::vector<WORD>{4, 1, 5, 1, 0, 1, 0x011e, 0}, // palette gradation out of range
		std::vector<WORD>{4, 1, 8, 2, 2, 0, 0, 3, 0, 1, 0x07e0}}) { // cumulative width
		CHECK(!Load(sprite, words));
		CHECK(!sprite.IsInit());
		CHECK_EQ(0, sprite.GetWidth());
		CHECK_EQ(0, sprite.GetHeight());
	}
}
}

TEST(IndexSpriteLoading, BothFormatsRejectEveryTruncatedHeaderAndBody)
{
	RejectTruncation<CIndexSprite565>();
	RejectTruncation<CIndexSprite555>();
}

TEST(IndexSpriteLoading, BothFormatsValidateEncodedAndDecodedRowBounds)
{
	RejectMalformedRows<CIndexSprite565>();
	RejectMalformedRows<CIndexSprite555>();
}

TEST(IndexSpriteLoading, CompleteRejectedRowsLeaveTheNextPackedSpriteReadable)
{
	for (bool use555 : {false, true}) {
		CIndexSprite565 rgb565;
		CIndexSprite555 rgb555;
		CIndexSprite& sprite = use555 ? static_cast<CIndexSprite&>(rgb555) : static_cast<CIndexSprite&>(rgb565);
		std::vector<WORD> words{4, 2, 3, 1, 0, 7, 1, 0};
		const auto next = std::streamoff(words.size() * sizeof(WORD));
		words.insert(words.end(), valid.begin(), valid.end());
		Write(words);
		std::ifstream in(path, std::ios::binary);
		CHECK(!sprite.LoadFromFile(in));
		CHECK(!sprite.IsInit());
		CHECK_EQ(next, std::streamoff(in.tellg()));
		CHECK(sprite.LoadFromFile(in));
		CHECK_EQ(4, sprite.GetWidth());
		CHECK_EQ(1, sprite.GetHeight());
		in.close();
		std::remove(path);
	}
}

TEST(IndexSpriteLoading, PaletteWordsAndPaddingSurvive555ColorConversion)
{
	for (bool use555 : {false, true}) {
		CIndexSprite565 rgb565;
		CIndexSprite555 rgb555;
		CIndexSprite& sprite = use555 ? static_cast<CIndexSprite&>(rgb555) : static_cast<CIndexSprite&>(rgb565);
		auto words = valid;
		words[2] += 2;
		words.push_back(0x1234);
		words.push_back(0xffff);
		const bool loaded = Load(sprite, words);
		CHECK(loaded);
		if (loaded) {
			const auto* row = sprite.GetPixelLine(0);
			CHECK_EQ(0x010a, row[3]);
			CHECK_EQ(0xff1d, row[4]);
			CHECK_EQ(use555 ? ColorDraw::Convert565to555(0x07e0) : 0x07e0, row[6]);
			CHECK_EQ(0x1234, row[7]);
			CHECK_EQ(0xffff, row[8]);
		}
		CHECK(Load(sprite, {4, 1, 1, 0})); // fully transparent row
		CHECK(Load(sprite, {4, 0})); // empty sprite has only its complete header
		sprite.Release();
		CHECK(!sprite.IsInit());
		CHECK_EQ(0, sprite.GetWidth());
		CHECK_EQ(0, sprite.GetHeight());
	}
}
