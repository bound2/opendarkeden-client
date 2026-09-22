#include "test_framework.h"
#include "CShadowSprite.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {
constexpr const char* path = "shadow_sprite_loading_test.bin";
const std::vector<WORD> valid{4, 1, 5, 2, 1, 1, 1, 1};
struct Fixture { ~Fixture() { std::remove(path); } };
void Write(const std::vector<WORD>& words, size_t limit = SIZE_MAX)
{
	std::vector<char> bytes;
	for (WORD word : words) { bytes.push_back(char(word)); bytes.push_back(char(word >> 8)); }
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), static_cast<std::streamsize>((std::min)(limit, bytes.size())));
}
void CheckEmpty(const CShadowSprite& sprite)
{
	CHECK(!sprite.IsInit());
	CHECK_EQ(0, sprite.GetWidth());
	CHECK_EQ(0, sprite.GetHeight());
}
}

TEST(ShadowSpriteLoading, IncompleteHeadersAreNotEmptySprites)
{
	Fixture fixture;
	for (size_t size = 0; size < 4; ++size) {
		Write({0, 0}, size);
		std::ifstream in(path, std::ios::binary);
		CShadowSprite sprite;
		CHECK(!sprite.LoadFromFile(in));
		CHECK(!in);
		CheckEmpty(sprite);
	}
}

TEST(ShadowSpriteLoading, RejectsIncompleteAndOutOfBoundsRows)
{
	Fixture fixture;
	for (const auto& words : {
		std::vector<WORD>{4, 1, 0},
		std::vector<WORD>{4, 1, 2, 1, 0},
		std::vector<WORD>{4, 1, 3, 1, 5, 0},
		std::vector<WORD>{4, 1, 3, 1, 1, 4},
		std::vector<WORD>{4, 1, 5, 2, 1, 1, 1, 2},
		std::vector<WORD>{65535, 1, 3, 1, 65535, 1},
		std::vector<WORD>{4, 1, 5, 2, 1, 1}}) {
		Write(words);
		std::ifstream in(path, std::ios::binary);
		CShadowSprite sprite;
		CHECK(!sprite.LoadFromFile(in));
		CheckEmpty(sprite);
	}
}

TEST(ShadowSpriteLoading, ReleaseClearsCompleteEmptySpriteState)
{
	Fixture fixture;
	for (const auto& words : {std::vector<WORD>{0, 3}, std::vector<WORD>{3, 0}}) {
		Write(words);
		std::ifstream in(path, std::ios::binary);
		CShadowSprite sprite;
		CHECK(sprite.LoadFromFile(in));
		CHECK(sprite.IsInit());
		sprite.Release();
		CheckEmpty(sprite);
		sprite.Release();
		CheckEmpty(sprite);
	}
}

TEST(ShadowSpriteLoading, EveryTruncatedPrefixClearsStateAndAllowsReload)
{
	Fixture fixture;
	const std::vector<WORD> twoRows{4, 2, 5, 2, 1, 1, 1, 1, 3, 1, 0, 4};
	CShadowSprite sprite;
	for (size_t size = 0; size < twoRows.size() * 2; ++size) {
		Write(twoRows, size);
		{
			std::ifstream in(path, std::ios::binary);
			CHECK(!sprite.LoadFromFile(in));
			CHECK(!in);
		}
		CheckEmpty(sprite);
		Write(valid);
		std::ifstream in(path, std::ios::binary);
		CHECK(sprite.LoadFromFile(in));
		CHECK_EQ(4, sprite.GetWidth());
	}
}

TEST(ShadowSpriteLoading, CompleteRejectedRecordsPreserveTheFollowingHeader)
{
	Fixture fixture;
	for (const auto& invalid : {
		std::vector<WORD>{4, 2, 3, 1, 1, 4, 1, 0},
		std::vector<WORD>{4, 2, 0, 1, 0}}) {
		auto words = invalid;
		const auto next = static_cast<std::streamoff>(words.size() * 2);
		words.insert(words.end(), valid.begin(), valid.end());
		Write(words);
		std::ifstream in(path, std::ios::binary);
		CShadowSprite sprite;
		CHECK(!sprite.LoadFromFile(in));
		CheckEmpty(sprite);
		CHECK_EQ(next, static_cast<std::streamoff>(in.tellg()));
		CHECK(sprite.LoadFromFile(in));
		CHECK_EQ(4, sprite.GetWidth());
		CHECK_EQ(1, sprite.GetHeight());
		std::ifstream throwing;
		throwing.exceptions(std::ios::badbit | std::ios::failbit);
		CHECK(!sprite.LoadFromFile(throwing));
		CheckEmpty(sprite);
	}
}

TEST(ShadowSpriteLoading, ValidRowsSupportDrawingPaddingAndMaximumLengths)
{
	Fixture fixture;
	CShadowSprite sprite;
	auto padded = valid;
	padded[2] += 2;
	padded.push_back(0x1234);
	padded.push_back(0xffff);
	Write(padded);
	{
		std::ifstream in(path, std::ios::binary);
		const bool loaded = sprite.LoadFromFile(in);
		CHECK(loaded);
		if (loaded) {
			CHECK_EQ(0xffff, sprite.GetPixelLine(0)[6]);
			CHECK(!sprite.IsColorPixel(0, 0));
			CHECK(sprite.IsColorPixel(1, 0));
			CHECK(!sprite.IsColorPixel(2, 0));
			CHECK(sprite.IsColorPixel(3, 0));
			std::vector<WORD> pixels(6, 0x1234);
			sprite.Blt(pixels.data() + 1, 4 * sizeof(WORD));
			CHECK(pixels == std::vector<WORD>({0x1234, 0x1234, 0, 0x1234, 0, 0x1234}));
		}
	}
	std::vector<WORD> maximum{65534, 1, 65535, 32767};
	for (size_t run = 0; run < 32767; ++run) { maximum.push_back(1); maximum.push_back(1); }
	Write(maximum);
	{
		std::ifstream in(path, std::ios::binary);
		const bool loaded = sprite.LoadFromFile(in);
		CHECK(loaded);
		if (loaded) CHECK_EQ(1, sprite.GetPixelLine(0)[65534]);
		CHECK_EQ(static_cast<std::streamoff>(maximum.size() * 2), static_cast<std::streamoff>(in.tellg()));
	}
	std::vector<WORD> tall{1, 65535};
	for (size_t y = 0; y < 65535; ++y) { tall.push_back(1); tall.push_back(0); }
	Write(tall);
	std::ifstream in(path, std::ios::binary);
	const bool loaded = sprite.LoadFromFile(in);
	CHECK(loaded);
	if (loaded) CHECK_EQ(0, sprite.GetPixelLine(65534)[0]);
	CHECK_EQ(65535, sprite.GetHeight());
}
