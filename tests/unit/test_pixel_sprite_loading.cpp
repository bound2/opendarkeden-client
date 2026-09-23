#include "test_framework.h"
#include "CSprite555.h"
#include "CSprite565.h"
#include "ColorDraw.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {
constexpr const char* path = "pixel_sprite_loading_test.bin";
const std::vector<WORD> valid{2, 1, 5, 1, 0, 2, 0xffff, 0x07e0};
struct Fixture { ~Fixture() { std::remove(path); } };
void Write(const std::vector<WORD>& words, size_t limit = SIZE_MAX)
{
	std::vector<char> bytes;
	for (WORD word : words) { bytes.push_back(char(word)); bytes.push_back(char(word >> 8)); }
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), static_cast<std::streamsize>((std::min)(limit, bytes.size())));
}
template<class Sprite> void RejectHeaders()
{
	Fixture fixture;
	for (size_t size = 0; size < 4; ++size) {
		Write(valid, size);
		std::ifstream in(path, std::ios::binary);
		Sprite sprite;
		CHECK(!sprite.LoadFromFile(in));
		CHECK(!sprite.IsInit());
		CHECK_EQ(0, sprite.GetWidth());
		CHECK_EQ(0, sprite.GetHeight());
		CHECK(!sprite.IsLoading());
	}
}
template<class Sprite> void RejectRows()
{
	Fixture fixture;
	for (const auto& words : {
		std::vector<WORD>{2, 1, 0},
		std::vector<WORD>{2, 1, 2, 1, 0},
		std::vector<WORD>{2, 1, 4, 1, 0, 2, 0xffff},
		std::vector<WORD>{2, 1, 3, 1, 4, 0},
		std::vector<WORD>{2, 1, 5, 1, 2, 2, 0xffff, 0x07e0},
		std::vector<WORD>{2, 1, 7, 2, 2, 0, 1, 1, 0xffff, 0},
		std::vector<WORD>{65535, 1, 4, 1, 65535, 1, 0xffff}}) {
		Write(words);
		std::ifstream in(path, std::ios::binary);
		Sprite sprite;
		CHECK(!sprite.LoadFromFile(in));
		CHECK(!sprite.IsInit());
		CHECK_EQ(0, sprite.GetWidth());
		CHECK_EQ(0, sprite.GetHeight());
	}
}
}

TEST(PixelSpriteLoading, BothFormatsRejectEveryIncompleteHeader)
{
	RejectHeaders<CSprite565>();
	RejectHeaders<CSprite555>();
}

TEST(PixelSpriteLoading, BothFormatsValidateEncodedAndDecodedRows)
{
	RejectRows<CSprite565>();
	RejectRows<CSprite555>();
}

TEST(PixelSpriteLoading, LongValidRowsConsumeTheirPixelsBeforeTheNextRecord)
{
	Fixture fixture;
	std::vector<WORD> words{8190, 1, 8193, 1, 0, 8190};
	words.insert(words.end(), 8190, 0x07e0);
	const auto next = static_cast<std::streamoff>(words.size() * 2);
	words.insert(words.end(), valid.begin(), valid.end());
	Write(words);
	std::ifstream in(path, std::ios::binary);
	CSprite565 sprite;
	CHECK(sprite.LoadFromFile(in));
	CHECK_EQ(next, static_cast<std::streamoff>(in.tellg()));
	CHECK_EQ(1, sprite.GetPixelLine(0)[0]);
	// Do not ask the old loader to parse pixels it failed to consume as a header.
	if (in.tellg() == std::streampos(next)) {
		CHECK(sprite.LoadFromFile(in));
		CHECK_EQ(2, sprite.GetWidth());
	}
}

TEST(PixelSpriteLoading, EveryTruncatedBodyClearsStateAndAllowsReload)
{
	Fixture fixture;
	for (bool use555 : {false, true}) {
		CSprite565 rgb565;
		CSprite555 rgb555;
		CSprite& sprite = use555 ? static_cast<CSprite&>(rgb555) : static_cast<CSprite&>(rgb565);
		for (size_t size = 4; size < valid.size() * 2; ++size) {
			Write(valid, size);
			{
				std::ifstream in(path, std::ios::binary);
				CHECK(!sprite.LoadFromFile(in));
				CHECK(!in);
			}
			CHECK(!sprite.IsInit());
			CHECK(!sprite.IsLoading());
			CHECK_EQ(0, sprite.GetWidth());
			Write(valid);
			std::ifstream in(path, std::ios::binary);
			CHECK(sprite.LoadFromFile(in));
			CHECK_EQ(2, sprite.GetWidth());
		}
	}
}

TEST(PixelSpriteLoading, CompleteRejectedRecordsPreserveTheNextPackedHeader)
{
	Fixture fixture;
	for (bool use555 : {false, true}) {
		CSprite565 rgb565;
		CSprite555 rgb555;
		CSprite& sprite = use555 ? static_cast<CSprite&>(rgb555) : static_cast<CSprite&>(rgb565);
		std::vector<WORD> words{2, 2, 3, 1, 4, 0, 1, 0};
		const auto next = static_cast<std::streamoff>(words.size() * 2);
		words.insert(words.end(), valid.begin(), valid.end());
		Write(words);
		std::ifstream in(path, std::ios::binary);
		CHECK(!sprite.LoadFromFile(in));
		CHECK(!sprite.IsInit());
		CHECK_EQ(next, static_cast<std::streamoff>(in.tellg()));
		CHECK(sprite.LoadFromFile(in));
		CHECK_EQ(2, sprite.GetWidth());
		CHECK_EQ(1, sprite.GetHeight());
	}
}

TEST(PixelSpriteLoading, MaximumRowsAndPaddingRetainTheirDocumentedMeaning)
{
	Fixture fixture;
	for (bool use555 : {false, true}) {
		CSprite565 rgb565;
		CSprite555 rgb555;
		CSprite& sprite = use555 ? static_cast<CSprite&>(rgb555) : static_cast<CSprite&>(rgb565);
		std::vector<WORD> words{65532, 1, 65535, 1, 0, 65532};
		words.insert(words.end(), 65532, 0x07e0);
		Write(words);
		{
			std::ifstream in(path, std::ios::binary);
			const bool loaded = sprite.LoadFromFile(in);
			CHECK(loaded);
			if (loaded) CHECK_EQ(use555 ? ColorDraw::Convert565to555(0x07e0) : 0x07e0,
				sprite.GetPixelLine(0)[65534]);
		}
		words = valid;
		words[2] += 2;
		words.push_back(0x1234);
		words.push_back(0xffff);
		Write(words);
		{
			std::ifstream in(path, std::ios::binary);
			const bool loaded = sprite.LoadFromFile(in);
			CHECK(loaded);
			if (loaded) {
				CHECK_EQ(0x1234, sprite.GetPixelLine(0)[5]);
				CHECK_EQ(0xffff, sprite.GetPixelLine(0)[6]);
			}
		}
		for (const auto& empty : {std::vector<WORD>{0, 3}, std::vector<WORD>{3, 0}}) {
			Write(empty);
			std::ifstream in(path, std::ios::binary);
			CHECK(sprite.LoadFromFile(in));
			CHECK(sprite.IsInit());
			CHECK(!sprite.IsLoading());
		}
		std::ifstream throwing;
		throwing.exceptions(std::ios::failbit | std::ios::badbit);
		CHECK(!sprite.LoadFromFile(throwing));
		CHECK(!sprite.IsInit());
		CHECK(!sprite.IsLoading());
	}
}

TEST(PixelSpriteLoading, LegacyInclusiveWidthsPreserveTheFinalColorPixel)
{
	Fixture fixture;
	for (bool use555 : {false, true}) {
		CSprite565 rgb565;
		CSprite555 rgb555;
		CSprite& sprite = use555 ? static_cast<CSprite&>(rgb555) : static_cast<CSprite&>(rgb565);
		// The installed Effect.spk contains rows whose final pixel lies at the
		// stored width coordinate. Normalize the extent before drawing them.
		Write({2, 1, 5, 1, 1, 2, 0xffff, 0x07e0});
		std::ifstream in(path, std::ios::binary);
		const bool loaded = sprite.LoadFromFile(in);
		CHECK(loaded);
		CHECK_EQ(3, sprite.GetWidth());
		if (loaded) CHECK_EQ(use555 ? ColorDraw::Convert565to555(0x07e0) : 0x07e0,
			sprite.GetPixelLine(0)[4]);
	}
}
