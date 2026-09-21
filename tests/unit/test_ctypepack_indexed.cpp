#include "test_framework.h"
#include "CTypePack.h"

#include <cstdio>
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
