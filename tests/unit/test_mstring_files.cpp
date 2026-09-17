#include "test_framework.h"
#include "MString.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
constexpr const char* path = "mstring_file_test.bin";
struct Fixture { ~Fixture() { std::remove(path); } };

std::vector<char> Record(std::uint32_t length, const std::string& text = {})
{
	std::vector<char> result;
	for (unsigned shift = 0; shift < 32; shift += 8) result.push_back(char(length >> shift));
	result.insert(result.end(), text.begin(), text.end());
	return result;
}

void Write(const std::vector<char>& bytes, std::size_t limit = SIZE_MAX)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), std::streamsize((std::min)(bytes.size(), limit)));
}

bool Is(const MString& value, const char* expected)
{
	return value.GetString() && std::strcmp(value.GetString(), expected) == 0;
}
}

TEST(MStringFiles, TruncatedLengthOrPayloadPreservesThePreviousValue)
{
	Fixture fixture;
	const auto bytes = Record(5, "hello");
	for (std::size_t count = 0; count < bytes.size(); ++count) {
		Write(bytes, count);
		std::ifstream in(path, std::ios::binary);
		MString value("kept");
		value.LoadFromFile(in);
		CHECK(in.fail());
		CHECK_EQ(4, value.GetLength());
		CHECK(Is(value, "kept"));
	}
}

TEST(MStringFiles, OversizedLengthsSetFailureWithoutConsumingAPayload)
{
	Fixture fixture;
	for (std::uint32_t length : {65537u, 0x80000000u, 0xffffffffu}) {
		Write(Record(length, "sentinel"));
		std::ifstream in(path, std::ios::binary);
		MString value("kept");
		value.LoadFromFile(in);
		CHECK(in.fail());
		CHECK(Is(value, "kept"));
		in.clear();
		CHECK_EQ(4, in.tellg());
	}
}

TEST(MStringFiles, EmptyAndNonemptyRecordsStayAligned)
{
	Fixture fixture;
	auto bytes = Record(0);
	const auto next = Record(3, "end");
	bytes.insert(bytes.end(), next.begin(), next.end());
	Write(bytes);
	std::ifstream in(path, std::ios::binary);
	MString value("previous");
	value.LoadFromFile(in);
	CHECK(bool(in));
	CHECK_EQ(0, value.GetLength());
	CHECK(Is(value, ""));
	CHECK_EQ(4, in.tellg());
	value.LoadFromFile(in);
	CHECK(bool(in));
	CHECK_EQ(3, value.GetLength());
	CHECK(Is(value, "end"));
	CHECK_EQ(11, in.tellg());
}

TEST(MStringFiles, MaximumAcceptedLengthRoundTripsWithAFourByteLittleEndianPrefix)
{
	Fixture fixture;
	const std::string text(65536, 'a');
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		MString value(text.c_str());
		value.SaveToFile(out);
		CHECK(bool(out));
	}
	{
		std::ifstream in(path, std::ios::binary);
		const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), {});
		CHECK(bytes == Record(65536, text));
	}
	std::ifstream in(path, std::ios::binary);
	MString loaded;
	loaded.LoadFromFile(in);
	CHECK(bool(in));
	CHECK_EQ(text.size(), loaded.GetLength());
	CHECK(Is(loaded, text.c_str()));
}

TEST(MStringFiles, AReadExceptionAlsoPreservesThePreviousValue)
{
	Fixture fixture;
	Write(Record(5, "x"));
	std::ifstream in(path, std::ios::binary);
	in.exceptions(std::ios::failbit | std::ios::badbit);
	MString value("kept");
	bool threw = false;
	try { value.LoadFromFile(in); }
	catch (const std::ios_base::failure&) { threw = true; }
	CHECK(threw);
	CHECK_EQ(4, value.GetLength());
	CHECK(Is(value, "kept"));
}
