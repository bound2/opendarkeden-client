#include "test_framework.h"
#include "CAlphaSpritePack.h"
#include "ColorDraw.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
constexpr const char* dataPath = "alpha_pack_test.bin";
constexpr const char* indexPath = "alpha_pack_index_test.bin";
const std::vector<WORD> spriteWords{1, 1, 5, 1, 0, 1, 0x1010, 0x07e0};
struct Files { ~Files() { std::remove(dataPath); std::remove(indexPath); } };
void Write(const char* path, const std::vector<WORD>& words, size_t bytes = SIZE_MAX)
{
	std::vector<char> encoded;
	for (WORD word : words) { encoded.push_back(char(word)); encoded.push_back(char(word >> 8)); }
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(encoded.data(), static_cast<std::streamsize>((std::min)(bytes, encoded.size())));
}
void Seed(CAlphaSpritePack& pack, bool rgb565 = true)
{
	pack.Init(1, rgb565);
	Write(dataPath, spriteWords);
	std::ifstream input(dataPath, std::ios::binary);
	CHECK(pack[0].LoadFromFile(input));
}
void CheckSeed(CAlphaSpritePack& pack)
{
	CHECK_EQ(1, pack.GetSize());
	if (pack.GetSize() != 1) return;
	CHECK(pack[0].IsInit());
	CHECK_EQ(1, pack[0].GetWidth());
	CHECK_EQ(1, pack[0].GetHeight());
	if (pack[0].IsInit()) CHECK_EQ(0x07e0, pack[0].GetPixelLine(0)[4]);
}
}

TEST(AlphaSpritePack, InitializingZeroClearsThePreviousPack)
{
	Files files;
	CAlphaSpritePack pack;
	Seed(pack);
	pack.Init(0, true);
	CHECK_EQ(0, pack.GetSize());
	pack.Release();
	CHECK_EQ(0, pack.GetSize());
}

TEST(AlphaSpritePack, TruncatedPacksPreservePreviouslyLoadedSprites)
{
	Files files;
	std::vector<WORD> words{1};
	words.insert(words.end(), spriteWords.begin(), spriteWords.end());
	for (size_t size = 0; size < words.size() * 2; ++size) {
		CAlphaSpritePack pack;
		Seed(pack);
		Write(dataPath, words, size);
		std::ifstream input(dataPath, std::ios::binary);
		CHECK(!pack.LoadFromFile(input));
		CheckSeed(pack);
	}
}

TEST(AlphaSpritePack, IndexedReadsPropagateDecoderFailure)
{
	Files files;
	CAlphaSpritePack pack;
	Seed(pack);
	// A full index entry points at an incomplete sprite body.
	Write(dataPath, {1, 1, 1, 0});
	Write(indexPath, {1, 2, 0});
	CHECK(!pack.LoadFromFileSprite(0, 0, dataPath, indexPath));
}

TEST(AlphaSpritePack, ConcreteArraysRetainTheirFormatAcrossReloadAndRelease)
{
	static_assert(!std::is_copy_constructible_v<CAlphaSpritePack>);
	static_assert(!std::is_copy_assignable_v<CAlphaSpritePack>);
	static_assert(!std::is_move_constructible_v<CAlphaSpritePack>);
	Files files;
	CAlphaSpritePack pack;
	std::vector<WORD> words = spriteWords;
	words.insert(words.end(), spriteWords.begin(), spriteWords.end());
	for (bool rgb565 : {false, true, false, true}) {
		pack.Init(2, rgb565);
		Write(dataPath, words);
		std::ifstream input(dataPath, std::ios::binary);
		CHECK(pack.LoadFromFilePart(input, 0, 0, 1));
		for (WORD id : {WORD(0), WORD(1)}) {
			CHECK_EQ(rgb565, dynamic_cast<CAlphaSprite565*>(&pack[id]) != nullptr);
			CHECK_EQ(!rgb565, dynamic_cast<CAlphaSprite555*>(&pack[id]) != nullptr);
			CHECK_EQ(rgb565 ? 0x07e0 : ColorDraw::Convert565to555(0x07e0), pack[id].GetPixelLine(0)[4]);
		}
		pack.ReleasePart(1, 65535);
		CHECK(pack[0].IsInit());
		CHECK(!pack[1].IsInit());
		pack.ReleasePart(2, 65535);
		CHECK(pack[0].IsInit());
	}
	pack.Release();
	pack.Release();
	CHECK_EQ(0, pack.GetSize());
}

TEST(AlphaSpritePack, InvalidRangesAreRejectedBeforeTouchingLoadedSprites)
{
	Files files;
	CAlphaSpritePack pack;
	Seed(pack);
	for (const auto& range : std::vector<std::pair<WORD, WORD>>{{1, 1}, {0, 1}, {1, 0}, {0, 65535}, {65535, 65535}}) {
		std::ifstream input(dataPath, std::ios::binary);
		CHECK(!pack.LoadFromFilePart(input, 0, range.first, range.second));
		CHECK_EQ(std::streampos(0), input.tellg());
		CheckSeed(pack);
	}
	for (int offset : {-1, 16, 2147483647}) {
		std::ifstream input(dataPath, std::ios::binary);
		CHECK(!pack.LoadFromFilePart(input, offset, 0, 0));
		CheckSeed(pack);
	}
	bool threw = false;
	try { (void)pack[1]; } catch (const std::out_of_range&) { threw = true; }
	CHECK(threw);
	pack.Release();
	threw = false;
	try { (void)pack[0]; } catch (const std::out_of_range&) { threw = true; }
	CHECK(threw);
}

TEST(AlphaSpritePack, IndexedHeadersAndOffsetsAreValidatedBeforeDecoding)
{
	Files files;
	CAlphaSpritePack pack;
	Seed(pack);
	std::vector<WORD> words{1};
	words.insert(words.end(), spriteWords.begin(), spriteWords.end());
	for (const auto& index : std::vector<std::vector<WORD>>{
		{}, {1}, {1, 2}, {2, 2, 0}, {1, 0, 0}, {1, 1, 0}, {1, 18, 0}, {1, 65535, 65535}}) {
		Write(dataPath, words);
		Write(indexPath, index);
		CHECK(!pack.LoadFromFileSprite(0, 0, dataPath, indexPath));
		CheckSeed(pack);
	}
	Write(indexPath, {1, 2, 0});
	for (const auto& ids : std::vector<std::pair<int, int>>{{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, 2147483647}}) {
		CHECK(!pack.LoadFromFileSprite(ids.first, ids.second, dataPath, indexPath));
		CheckSeed(pack);
	}
	CHECK(!pack.LoadFromFileSprite(0, 0, nullptr, indexPath));
	CHECK(!pack.LoadFromFileSprite(0, 0, dataPath, nullptr));
	CheckSeed(pack);
	Write(dataPath, words, 1);
	CHECK(!pack.LoadFromFileSprite(0, 0, dataPath, indexPath));
	CheckSeed(pack);
}

TEST(AlphaSpritePack, ReusingIndexStreamsAlwaysReadsTheHeaderFromTheBeginning)
{
	Files files;
	CAlphaSpritePack pack;
	pack.Init(2, false);
	std::vector<WORD> words{2};
	words.insert(words.end(), spriteWords.begin(), spriteWords.end());
	words.insert(words.end(), spriteWords.begin(), spriteWords.end());
	Write(dataPath, words);
	Write(indexPath, {2, 2, 0, 18, 0});
	std::ifstream input(dataPath, std::ios::binary), index(indexPath, std::ios::binary);
	for (int id : {1, 0, 1}) {
		CHECK(pack.LoadFromFileSprite(id, id, input, index));
		CHECK_EQ(ColorDraw::Convert565to555(0x07e0), pack[static_cast<WORD>(id)].GetPixelLine(0)[4]);
	}
}

TEST(AlphaSpritePack, CompleteRejectedPacksPreserveTheNextPackAndTheOldData)
{
	Files files;
	CAlphaSpritePack pack;
	Seed(pack);
	// The first complete row overruns its width; the second sprite is empty.
	std::vector<WORD> words{2, 1, 1, 5, 1, 1, 1, 0x1010, 0x07e0, 0, 0};
	const auto next = std::streampos(static_cast<std::streamoff>(words.size() * 2));
	words.push_back(1);
	words.insert(words.end(), spriteWords.begin(), spriteWords.end());
	Write(dataPath, words);
	std::ifstream input(dataPath, std::ios::binary);
	CHECK(!pack.LoadFromFile(input));
	CHECK_EQ(next, input.tellg());
	CheckSeed(pack);
	CHECK(pack.LoadFromFile(input));
	CheckSeed(pack);
}

TEST(AlphaSpritePack, RangeFailuresRetainEarlierCompleteRowsAndRejectTheBadRow)
{
	Files files;
	std::vector<WORD> words = spriteWords;
	words.insert(words.end(), {1, 1, 0});
	words.insert(words.end(), spriteWords.begin(), spriteWords.end());
	Write(dataPath, words);
	std::ifstream input(dataPath, std::ios::binary);
	CAlphaSpritePack pack;
	pack.Init(3, true);
	CHECK(!pack.LoadFromFilePart(input, 0, 0, 2));
	CHECK(pack[0].IsInit());
	CHECK(!pack[1].IsInit());
	CHECK(pack[2].IsInit());
	CHECK_EQ(std::streampos(static_cast<std::streamoff>(words.size() * 2)), input.tellg());
}

TEST(AlphaSpritePack, StreamFailuresAndExceptionsCannotReplaceTheFullPack)
{
	Files files;
	CAlphaSpritePack pack;
	Seed(pack);
	std::vector<WORD> words{1};
	words.insert(words.end(), spriteWords.begin(), spriteWords.end());
	for (size_t size = 0; size < words.size() * 2; ++size) {
		Write(dataPath, words, size);
		std::ifstream input(dataPath, std::ios::binary);
		input.exceptions(std::ios::failbit | std::ios::badbit);
		CHECK(!pack.LoadFromFile(input));
		CheckSeed(pack);
	}
	std::ifstream failed;
	failed.setstate(std::ios::failbit);
	CHECK(!pack.LoadFromFile(failed));
	CHECK(!pack.LoadFromFilePart(failed, 0, 0, 0));
	std::ifstream index;
	CHECK(!pack.LoadFromFileSprite(0, 0, failed, index));
	CheckSeed(pack);
}

TEST(AlphaSpritePack, EmptyAndMaximumCountsRetainTheFollowingRecord)
{
	Files files;
	CAlphaSpritePack pack;
	Seed(pack);
	Write(dataPath, {0, 0xabcd});
	{
		std::ifstream input(dataPath, std::ios::binary);
		CHECK(pack.LoadFromFile(input));
		CHECK_EQ(0, pack.GetSize());
		CHECK_EQ(0xcd, input.get());
	}
	std::vector<WORD> words{65535};
	words.resize(1 + 2 * 65535, 0); // Complete empty sprite headers.
	words.push_back(0xabcd);
	Write(dataPath, words);
	std::ifstream input(dataPath, std::ios::binary);
	CHECK(pack.LoadFromFile(input));
	CHECK_EQ(65535, pack.GetSize());
	CHECK(pack[65534].IsInit());
	CHECK_EQ(0xcd, input.get());
}

TEST(AlphaSpritePack, BothSavePathsTraverseTheConcreteArrays)
{
	Files files;
	for (bool rgb565 : {false, true}) {
		CAlphaSpritePack pack;
		pack.Init(2, rgb565);
		std::vector<WORD> words = spriteWords;
		words.insert(words.end(), spriteWords.begin(), spriteWords.end());
		Write(dataPath, words);
		{
			std::ifstream input(dataPath, std::ios::binary);
			CHECK(pack.LoadFromFilePart(input, 0, 0, 1));
		}
		const WORD original = pack[0].GetPixelLine(0)[4];
		{
			std::ofstream data(dataPath, std::ios::binary | std::ios::trunc);
			std::ofstream index(indexPath, std::ios::binary | std::ios::trunc);
			CHECK(pack.SaveToFile(data, index));
		}
		CAlphaSpritePack loaded;
		loaded.Init(2, rgb565);
		CHECK(loaded.LoadFromFileSprite(0, 0, dataPath, indexPath));
		CHECK(loaded.LoadFromFileSprite(1, 1, dataPath, indexPath));
		CHECK_EQ(original, loaded[0].GetPixelLine(0)[4]);
		CHECK_EQ(original, loaded[1].GetPixelLine(0)[4]);
		CHECK_EQ(original, pack[0].GetPixelLine(0)[4]);
		int32_t position = -1;
		{
			std::ofstream data(dataPath, std::ios::binary | std::ios::trunc);
			data.write("xx", 2);
			CHECK(pack.SaveToFileSpriteOnly(data, position));
		}
		CHECK_EQ(2, position);
		std::ifstream input(dataPath, std::ios::binary);
		CHECK(loaded.LoadFromFilePart(input, position, 0, 1));
		CHECK_EQ(original, loaded[1].GetPixelLine(0)[4]);
	}
}
