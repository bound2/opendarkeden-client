//----------------------------------------------------------------------
// test_cfilter.cpp
//----------------------------------------------------------------------
//
// Tests for CFilter in Client/SpriteLib/CFilter.h.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "CFilter.h"
#include "CFilterPack.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace {
const char* const kPackFile = "cfilter_pack_test.bin";
struct FilterPackFile { ~FilterPackFile() { std::remove(kPackFile); } };
void WritePack(const std::vector<unsigned char>& bytes, size_t length)
{
	std::ofstream out(kPackFile, std::ios::binary | std::ios::trunc);
	if (length) out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(length));
}
void SeedPack(CFilterPack& pack)
{
	pack.Init(1);
	pack[0].Init(1, 1);
	pack[0].SetFilter(0, 0, 42);
}
void CheckSeedPack(CFilterPack& pack)
{
	CHECK_EQ(1, pack.GetSize());
	if (pack.GetSize() != 1) return;
	CHECK(pack[0].IsInit());
	CHECK_EQ(1, pack[0].GetWidth());
	CHECK_EQ(1, pack[0].GetHeight());
	if (pack[0].IsInit()) CHECK_EQ(42, pack[0].GetFilter(0)[0]);
}
}

TEST(CFilterPack, EveryTruncatedPrefixPreservesThePreviousFilters)
{
	FilterPackFile fixture;
	const std::vector<unsigned char> bytes{2, 0, 2, 0, 1, 0, 17, 18, 1, 0, 2, 0, 19, 20};
	for (size_t length = 0; length < bytes.size(); ++length) {
		CFilterPack pack;
		SeedPack(pack);
		WritePack(bytes, length);
		std::ifstream input(kPackFile, std::ios::binary);
		CHECK(!pack.LoadFromFile(input));
		CheckSeedPack(pack);
	}
}

TEST(CFilterPack, InitializingAnEmptyPackClearsItsPreviousFilters)
{
	CFilterPack pack;
	SeedPack(pack);
	pack.Init(0);
	CHECK_EQ(0, pack.GetSize());
	pack.Release();
	CHECK_EQ(0, pack.GetSize());
}

TEST(CFilterPack, CompletePacksReplaceAllRowsAndLeaveTheNextRecordReadable)
{
	FilterPackFile fixture;
	const std::vector<unsigned char> bytes{2, 0, 2, 0, 1, 0, 17, 18, 1, 0, 2, 0, 19, 20, 0xab};
	WritePack(bytes, bytes.size());
	std::ifstream input(kPackFile, std::ios::binary);
	CFilterPack pack;
	SeedPack(pack);
	CHECK(pack.LoadFromFile(input));
	CHECK_EQ(2, pack.GetSize());
	CHECK_EQ(2, pack[0].GetWidth());
	CHECK_EQ(1, pack[0].GetHeight());
	CHECK_EQ(18, pack[0].GetFilter(0)[1]);
	CHECK_EQ(1, pack[1].GetWidth());
	CHECK_EQ(2, pack[1].GetHeight());
	CHECK_EQ(20, pack[1].GetFilter(1)[0]);
	CHECK_EQ(0xab, input.get());
}

TEST(CFilterPack, RejectedCountsAndLaterRowsPreserveThePreviousPack)
{
	FilterPackFile fixture;
	for (const auto& bytes : std::vector<std::vector<unsigned char>>{
		{255, 255}, {1, 0, 0, 0, 1, 0, 17}, {1, 0, 1, 0, 0, 0, 17},
		{1, 0, 255, 255, 255, 255, 17},
		{2, 0, 1, 0, 1, 0, 17, 0, 0, 1, 0, 18}}) {
		CFilterPack pack;
		SeedPack(pack);
		WritePack(bytes, bytes.size());
		std::ifstream input(kPackFile, std::ios::binary);
		CHECK(!pack.LoadFromFile(input));
		CheckSeedPack(pack);
	}
}

TEST(CFilterPack, EmptyFilePacksClearThePreviousDataAndConsumeOnlyTheCount)
{
	FilterPackFile fixture;
	WritePack({0, 0, 0xab}, 3);
	std::ifstream input(kPackFile, std::ios::binary);
	CFilterPack pack;
	SeedPack(pack);
	CHECK(pack.LoadFromFile(input));
	CHECK_EQ(0, pack.GetSize());
	CHECK_EQ(0xab, input.get());
	pack.Release();
	pack.Release();
	CHECK_EQ(0, pack.GetSize());
}

TEST(CFilterPack, StreamExceptionsAndPreexistingFailuresPreserveCurrentData)
{
	FilterPackFile fixture;
	const std::vector<unsigned char> bytes{1, 0, 1, 0, 1, 0, 17};
	CFilterPack pack;
	SeedPack(pack);
	for (size_t size = 0; size < bytes.size(); ++size) {
		WritePack(bytes, size);
		std::ifstream input(kPackFile, std::ios::binary);
		input.exceptions(std::ios::failbit | std::ios::badbit);
		CHECK(!pack.LoadFromFile(input));
		CheckSeedPack(pack);
	}
	WritePack(bytes, bytes.size());
	std::ifstream input(kPackFile, std::ios::binary);
	input.setstate(std::ios::failbit);
	CHECK(!pack.LoadFromFile(input));
	CHECK(input.fail());
	CheckSeedPack(pack);
}

TEST(CFilterPack, TheMaximumCountDoesNotWrapDuringLoading)
{
	FilterPackFile fixture;
	std::vector<unsigned char> bytes{255, 255};
	for (unsigned i = 0; i < 65535; ++i) bytes.insert(bytes.end(), {1, 0, 1, 0, 17});
	WritePack(bytes, bytes.size());
	std::ifstream input(kPackFile, std::ios::binary);
	CFilterPack pack;
	CHECK(pack.LoadFromFile(input));
	CHECK_EQ(65535, pack.GetSize());
	CHECK_EQ(17, pack[65534].GetFilter(0)[0]);
	CHECK_EQ(static_cast<std::streamoff>(bytes.size()), static_cast<std::streamoff>(input.tellg()));
}

namespace {

const char* const	kTempFile = "cfilter_loadfromfile_test.bin";

void	WriteRawFile(const void* data, size_t bytes)
{
	std::ofstream	out(kTempFile, std::ios::binary | std::ios::trunc);

	if (bytes > 0)
		out.write((const char*)data, (std::streamsize)bytes);
}

//----------------------------------------------------------------------
// Builds a CFilter file: a WORD width, a WORD height, then height rows
// of width bytes. bodyBytes controls how much of that body is actually
// written, so a caller can produce a truncated file.
//----------------------------------------------------------------------
void	WriteFilterFile(WORD width, WORD height, size_t bodyBytes)
{
	unsigned char	buffer[4 + 256];

	std::memcpy(buffer, &width, sizeof(width));
	std::memcpy(buffer + 2, &height, sizeof(height));

	if (bodyBytes > sizeof(buffer) - 4)
		bodyBytes = sizeof(buffer) - 4;

	for (size_t i = 0; i < bodyBytes; i++)
		buffer[4 + i] = (unsigned char)(i & 0xFF);

	WriteRawFile(buffer, 4 + bodyBytes);
}

void	RemoveTempFile()
{
	std::remove(kTempFile);
}

} // namespace

//----------------------------------------------------------------------
// IsInit and IsNotInit must be opposites.
//
// Both were written as "m_ppFilter == NULL", so IsInit answered the
// question backwards: it reported true for a filter with no storage and
// false once Init had allocated it.
//----------------------------------------------------------------------
TEST(CFilter, IsInitReportsWhetherStorageIsAllocated)
{
	CFilter	filter;

	CHECK(!filter.IsInit());
	CHECK(filter.IsNotInit());

	filter.Init(4, 4);

	CHECK(filter.IsInit());
	CHECK(!filter.IsNotInit());

	filter.Release();

	CHECK(!filter.IsInit());
	CHECK(filter.IsNotInit());
}

//----------------------------------------------------------------------
// A well formed file loads.
//----------------------------------------------------------------------
TEST(CFilter, LoadFromFileReadsAWellFormedFile)
{
	WriteFilterFile(4, 3, 4 * 3);

	CFilter		filter;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(filter.LoadFromFile(in));
	CHECK_EQ(4, filter.GetWidth());
	CHECK_EQ(3, filter.GetHeight());
	CHECK(filter.IsInit());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A height larger than the file must be rejected before it is used.
//
// LoadFromFile read the header straight into m_Width and m_Height and
// only then called Init(), which begins with Release(). Release frees
// m_ppFilter[i] for i below m_Height, but m_ppFilter still held the row
// count from the previous contents. A file declaring 1000 rows against
// a filter holding 4 therefore ran delete[] over 996 pointers read from
// past the end of the row array: a free driven entirely by file data.
//----------------------------------------------------------------------
TEST(CFilter, LoadFromFileRejectsHeightLargerThanTheFile)
{
	WriteFilterFile(4, 1000, 0);

	CFilter	filter;

	filter.Init(4, 4);

	CHECK(filter.IsInit());
	CHECK_EQ(4, filter.GetHeight());

	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!filter.LoadFromFile(in));

	// The rejected header must not have disturbed the existing filter.
	CHECK_EQ(4, filter.GetWidth());
	CHECK_EQ(4, filter.GetHeight());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A file whose body is shorter than the declared size is rejected.
//----------------------------------------------------------------------
TEST(CFilter, LoadFromFileRejectsTruncatedBody)
{
	// Declares 8 x 8 but carries only 10 of the 64 body bytes.
	WriteFilterFile(8, 8, 10);

	CFilter		filter;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!filter.LoadFromFile(in));

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A file too short to even hold the header is rejected.
//----------------------------------------------------------------------
TEST(CFilter, LoadFromFileRejectsTruncatedHeader)
{
	const unsigned char	singleByte = 0x04;

	WriteRawFile(&singleByte, 1);

	CFilter		filter;
	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!filter.LoadFromFile(in));
	CHECK(!filter.IsInit());

	RemoveTempFile();
}
