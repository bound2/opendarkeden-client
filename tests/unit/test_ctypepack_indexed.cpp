#include "test_framework.h"
#include "CTypePack.h"
#include "DebugLog.h"
#include "CSpritePack.h"

#include <cstdio>
#include <limits>
#include <utility>
#include <vector>

namespace {
constexpr const char* dataPath = "ctypepack_indexed_data_test.bin";
constexpr const char* indexPath = "ctypepack_indexed_index_test.bin";
struct Fixture {
	~Fixture() { std::remove(dataPath); std::remove(indexPath); }
};
void Write(const char* path, const std::vector<unsigned char>& bytes)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
class Element {
public:
	bool IsInit() const { return value != 0; }
	void Release() { value = 0; }
	bool SaveToFile(std::ofstream&) { return true; }
	bool LoadFromFile(std::ifstream& input) {
		++loads;
		char byte = 0;
		if (!input.get(byte) || byte == 0) return false;
		value = static_cast<unsigned char>(byte);
		return true;
	}
	int value = 0;
	int loads = 0;
};
template<class Pack> void CheckIndexedBounds()
{
	Fixture fixture;
	Pack pack;
	pack.Init(1);
	Write(dataPath, {1, 0, 42});
	Write(indexPath, {1, 0, 2, 0, 0, 0});
	CHECK(pack.LoadFromFileData(0, 0, dataPath, indexPath));
	CHECK_EQ(42, pack[0].value);
	CHECK_EQ(1, pack[0].loads);
	for (const auto& bytes : std::vector<std::vector<unsigned char>>{
		{}, {1}, {1, 0}, {1, 0, 2, 0, 0},
		{1, 0, 0, 0, 0, 0}, {1, 0, 1, 0, 0, 0},
		{1, 0, 3, 0, 0, 0}, {1, 0, 255, 255, 255, 255}}) {
		Write(indexPath, bytes);
		CHECK(!pack.LoadFromFileData(0, 0, dataPath, indexPath));
		CHECK_EQ(42, pack[0].value);
		CHECK_EQ(1, pack[0].loads);
	}
	Write(indexPath, {1, 0, 2, 0, 0, 0});
	CHECK(!pack.LoadFromFileData(0, -1, dataPath, indexPath));
	CHECK(!pack.LoadFromFileData(0, 1, dataPath, indexPath));
	CHECK(!pack.LoadFromFileData(-1, 0, dataPath, indexPath));
	CHECK(!pack.LoadFromFileData(1, 0, dataPath, indexPath));
	CHECK_EQ(1, pack[0].loads);
}
template<class Pack> void CheckDecodeFailure()
{
	Fixture fixture;
	Pack pack;
	pack.Init(1);
	Write(dataPath, {1, 0, 0});
	Write(indexPath, {1, 0, 2, 0, 0, 0});
	CHECK(!pack.LoadFromFileData(0, 0, dataPath, indexPath));
	CHECK_EQ(1, pack[0].loads);
}
}

TEST(CTypePackIndexed, RejectsIncompleteIndicesAndOffsetsOutsideTheData)
{
	CheckIndexedBounds<CTypePack<Element>>();
	CheckIndexedBounds<CTypePack2<Element, Element, Element>>();
}

TEST(CTypePackIndexed, PropagatesElementDecodeFailure)
{
	CheckDecodeFailure<CTypePack<Element>>();
	CheckDecodeFailure<CTypePack2<Element, Element, Element>>();
}

namespace {
template<class Pack> void CheckRunningInput()
{
	Fixture fixture;
	for (const auto& bytes : std::vector<std::vector<unsigned char>>{
		{}, {1}, {1, 0}, {2, 0, 2, 0, 0, 0},
		{1, 0, 0, 0, 0, 0}, {1, 0, 3, 0, 0, 0},
		{1, 0, 255, 255, 255, 255}}) {
		Pack pack;
		pack.Init(1);
		Write(dataPath, {1, 0, 42});
		Write(indexPath, bytes);
		// Running loads append 'i' to the data path.
		const std::string runningIndex = std::string(dataPath) + "i";
		Write(runningIndex.c_str(), bytes);
		CHECK(!pack.LoadFromFileRunning(dataPath));
		CHECK_EQ(1, pack.GetSize());
		std::remove(runningIndex.c_str());
	}
}
template<class Pack> void CheckRepeatedRejection()
{
	Fixture fixture;
	Write(dataPath, {2, 0, 0, 42});
	const std::string runningIndex = std::string(dataPath) + "i";
	Write(runningIndex.c_str(), {2, 0, 2, 0, 0, 0, 3, 0, 0, 0});
	{
		Pack pack;
		CHECK(pack.LoadFromFileRunning(dataPath));
		CHECK_EQ(0, pack.Get(0).value);
		CHECK_EQ(0, pack.Get(0).value);
		CHECK_EQ(42, pack.Get(1).value);
		CHECK_EQ(1, pack.Get(0).loads);
	}
	std::remove(runningIndex.c_str());
}
int rejectedLogs = 0;
void ObserveRejected(const LogSite&, LogLevel level) {
	if (level == LOG_LEVEL_ERROR) ++rejectedLogs;
}
}

TEST(CTypePackIndexed, RunningLoadsRejectIncompleteAndOutOfRangeIndices)
{
	CheckRunningInput<CTypePack<Element>>();
	CheckRunningInput<CTypePack2<Element, Element, Element>>();
}

TEST(CTypePackIndexed, RejectedLazyRowsAreAttemptedOnceWithoutDroppingOtherRows)
{
	CheckRepeatedRejection<CTypePack<Element>>();
	CheckRepeatedRejection<CTypePack2<Element, Element, Element>>();
}

TEST(CTypePackIndexed, RejectedElementLoadsEmitAnErrorDiagnostic)
{
	rejectedLogs = 0;
	const auto previous = log_set_site_observer(ObserveRejected);
	CheckDecodeFailure<CTypePack<Element>>();
	CheckDecodeFailure<CTypePack2<Element, Element, Element>>();
	log_set_site_observer(previous);
	CHECK_EQ(2, rejectedLogs);
}

namespace {
template<class Pack> void CheckEagerFailure()
{
	Fixture fixture;
	for (const auto& bytes : std::vector<std::vector<unsigned char>>{{}, {1}, {1, 0, 0}}) {
		Pack pack;
		Write(dataPath, bytes);
		CHECK(!pack.LoadFromFile(dataPath));
	}
	Pack pack;
	pack.Init(1);
	Write(dataPath, {0, 0});
	CHECK(pack.LoadFromFile(dataPath));
	CHECK_EQ(0, pack.GetSize());
}
}
TEST(CTypePackIndexed, EagerLoadsPropagateIncompleteHeadersAndRejectedRows)
{
	CheckEagerFailure<CTypePack<Element>>();
	CheckEagerFailure<CTypePack2<Element, Element, Element>>();
}

TEST(CTypePackIndexed, SpriteSuccessCannotHideAFailedUnderlyingRead)
{
	Fixture fixture;
	Write(dataPath, {1, 0, 0});
	CTypePack<CSprite565> direct;
	CSpritePack polymorphic;
	CHECK(!direct.LoadFromFile(dataPath));
	CHECK(!polymorphic.LoadFromFile(dataPath));
}

namespace {
class StreamFailingElement : public Element {
public:
	bool LoadFromFile(std::ifstream& input) {
		const bool loaded = Element::LoadFromFile(input);
		if (!loaded) input.setstate(std::ios::failbit);
		return loaded;
	}
};
template<class Pack> void CheckReloadState()
{
	Fixture fixture;
	const std::string runningIndex = std::string(dataPath) + "i";
	Write(dataPath, {2, 0, 17, 42});
	Write(runningIndex.c_str(), {2, 0, 2, 0, 0, 0, 3, 0, 0, 0});
	{
		Pack pack;
		CHECK(pack.LoadFromFileRunning(dataPath));
		CHECK_EQ(17, pack.Get(0).value);
		pack.ReleasePart(0, 0);
		CHECK_EQ(17, pack.Get(0).value);
		CHECK_EQ(42, pack.Get(1).value);
		CHECK(!pack.LoadFromFileRunning(nullptr));
		CHECK_EQ(2, pack.GetSize());
		CHECK_EQ(42, pack.Get(1).value);
		Write(runningIndex.c_str(), {3, 0});
		CHECK(!pack.LoadFromFileRunning(dataPath));
		CHECK_EQ(42, pack.Get(1).value);
		Write(dataPath, {0, 0});
		Write(runningIndex.c_str(), {0, 0});
		CHECK(pack.LoadFromFileRunning(dataPath));
		CHECK_EQ(0, pack.GetSize());
	}
	std::remove(runningIndex.c_str());
}
}
TEST(CTypePackIndexed, LazyReadsRecoverFromStreamFailureForTheNextIndexedRow)
{
	CheckRepeatedRejection<CTypePack<StreamFailingElement>>();
	CheckRepeatedRejection<CTypePack2<StreamFailingElement, StreamFailingElement, StreamFailingElement>>();
}
TEST(CTypePackIndexed, LazyReloadAndEvictionKeepTheUniqueEntryCount)
{
	CheckReloadState<CTypePack<Element>>();
	CheckReloadState<CTypePack2<Element, Element, Element>>();
}

namespace {
template<class Pack> void CheckPreloadRangeResult()
{
	Fixture fixture;
	const std::string runningIndex = std::string(dataPath) + "i";
	Write(dataPath, {3, 0, 0, 17, 42});
	Write(runningIndex.c_str(), {3, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0});
	{
		Pack pack;
		CHECK(pack.LoadFromFileRunning(dataPath));
		CHECK(!pack.LoadFromFilePart(0, 2));
		CHECK_EQ(17, pack.Get(1).value);
		CHECK_EQ(42, pack.Get(2).value);
		CHECK(!pack.LoadFromFilePart(0, 2));
		CHECK_EQ(1, pack.Get(0).loads);
		CHECK(pack.LoadFromFilePart(1, 2));
		for (const auto range : {std::pair{-1, 1}, std::pair{0, 3},
			std::pair{2, 1}, std::pair{65536, 65536}, std::pair{0, 65536}})
			CHECK(!pack.LoadFromFilePart(range.first, range.second));
		CHECK(pack.LoadFromFilePart(65535, 65535)); // absent-sprite sentinel
	}
	std::remove(runningIndex.c_str());
}

template<class Pack> void CheckPreloadSetResult()
{
	Fixture fixture;
	const std::string runningIndex = std::string(dataPath) + "i";
	Write(dataPath, {2, 0, 0, 42});
	Write(runningIndex.c_str(), {2, 0, 2, 0, 0, 0, 3, 0, 0, 0});
	{
		Pack pack;
		CHECK(pack.LoadFromFileRunning(dataPath));
		CSpriteSetManager ids;
		ids.Add(0); ids.Add(1); ids.Add(65535);
		CHECK(!pack.LoadFromFilePart(ids));
		CHECK_EQ(42, pack.Get(1).value);
		CHECK(!pack.LoadFromFilePart(ids));
		CHECK_EQ(1, pack.Get(0).loads);
		ids.Remove(0);
		CHECK(pack.LoadFromFilePart(ids));
		ids.Add(9);
		CHECK(!pack.LoadFromFilePart(ids));
		ids.Release();
		CHECK(pack.LoadFromFilePart(ids));
		pack.Release();
		CHECK(pack.LoadFromFilePart(ids));
		ids.Add(65535);
		CHECK(pack.LoadFromFilePart(ids));
		ids.Add(0);
		CHECK(!pack.LoadFromFilePart(ids));
	}
	std::remove(runningIndex.c_str());
}
}

TEST(CTypePackIndexed, RangePreloadsReportRejectionAndKeepFollowingRows)
{
	CheckPreloadRangeResult<CTypePack<StreamFailingElement>>();
	CheckPreloadRangeResult<CTypePack2<StreamFailingElement, StreamFailingElement, StreamFailingElement>>();
}

TEST(CTypePackIndexed, SetPreloadsReportRejectionAndIgnoreTheAbsentSpriteSentinel)
{
	CheckPreloadSetResult<CTypePack<StreamFailingElement>>();
	CheckPreloadSetResult<CTypePack2<StreamFailingElement, StreamFailingElement, StreamFailingElement>>();
}

TEST(CTypePackIndexed, FullSpriteSetsDoNotWrapTheirIterationCount)
{
	CSpriteSetManager ids;
	for (int id = 65535; id >= 0; --id) ids.Add(static_cast<WORD>(id));
	CTypePack<Element> first;
	CTypePack2<Element, Element, Element> second;
	first.Init(1); second.Init(1);
	CHECK(!first.LoadFromFilePart(ids));
	CHECK(!second.LoadFromFilePart(ids));
}

namespace {
template<class Pack> void CheckEmptySpritePreload()
{
	Fixture fixture;
	const std::string runningIndex = std::string(dataPath) + "i";
	Write(dataPath, {1, 0, 0, 0, 0, 0}); // one valid 0x0 sprite
	Write(runningIndex.c_str(), {1, 0, 2, 0, 0, 0});
	{
		Pack pack;
		CHECK(!pack.LoadFromFilePart(0, 0));
		CHECK(pack.LoadFromFilePart(65535, 65535));
		CHECK(pack.LoadFromFileRunning(dataPath));
		CHECK(pack.LoadFromFilePart(0, 0));
		CHECK_EQ(0, pack.Get(0).GetWidth());
		CHECK_EQ(0, pack.Get(0).GetHeight());
		CHECK(pack.LoadFromFilePart(0, 0));
		CSpriteSetManager ids;
		ids.Add(0);
		CHECK(pack.LoadFromFilePart(ids));
		CHECK(!pack.LoadFromFilePart((std::numeric_limits<int>::min)(), 0));
		CHECK(!pack.LoadFromFilePart(0, (std::numeric_limits<int>::max)()));
	}
	std::remove(runningIndex.c_str());
}
}

TEST(CTypePackIndexed, EmptySpritesAreSuccessfulPreloadsAfterTheStreamCloses)
{
	CheckEmptySpritePreload<CTypePack<CSprite555>>();
	CheckEmptySpritePreload<CTypePack<CSprite565>>();
	CheckEmptySpritePreload<CSpritePack>();
}
