#include "test_framework.h"
#include "MString.h"
#include "TextEncoding.h"
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
struct ResourceEncodingScope {
	TextEncoding::Encoding saved = TextEncoding::GetResourceEncoding();
	~ResourceEncodingScope() { TextEncoding::SetResourceEncoding(saved); }
};

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

TEST(MStringFiles, Cp949ResourceRecordsDecodeOnceAndRoundTripTheirBytes)
{
	Fixture fixture;
	const std::string encoded("\xC7\xD1\0\xC7\xD1", 5);
	const std::string utf8("\xED\x95\x9C\0\xED\x95\x9C", 7);
	Write(Record(5, encoded));
	MString value;
	{
		std::ifstream in(path, std::ios::binary);
		value.LoadFromFile(in);
		CHECK(bool(in));
	}
	CHECK_EQ(utf8.size(), value.GetLength());
	CHECK(std::string(value.GetString(), value.GetLength()) == utf8);
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		value.SaveToFile(out);
		CHECK(bool(out));
	}
	std::ifstream in(path, std::ios::binary);
	const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), {});
	CHECK(bytes == Record(5, encoded));
}

TEST(MStringFiles, Utf8ValuesAreEncodedToTheResourceCodePageOnSave)
{
	Fixture fixture;
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		MString value("\xED\x95\x9C");
		value.SaveToFile(out);
		CHECK(bool(out));
	}
	std::ifstream in(path, std::ios::binary);
	const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), {});
	CHECK(bytes == Record(2, "\xC7\xD1"));
}

TEST(MStringFiles, WriterRejectsUnreadableRecordsBeforeWritingThePrefix)
{
	Fixture fixture;
	for (const auto& text : {std::string(65537, 'a'), std::string("\xF0\x9F\x98\x80")}) {
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		MString value(text.c_str());
		value.SaveToFile(out);
		CHECK(out.fail());
		out.clear();
		CHECK_EQ(0, out.tellp());
	}
}

TEST(MStringFiles, ResourcePolicyControlsBothDirectionsIncludingUtf8Packs)
{
	Fixture fixture;
	ResourceEncodingScope scope;
	struct Vector { TextEncoding::Encoding encoding; const char* bytes; const char* utf8; };
	const Vector vectors[] = {
		{TextEncoding::Encoding::Gbk, "\xC4\xE3\xBA\xC3", "\xE4\xBD\xA0\xE5\xA5\xBD"},
		{TextEncoding::Encoding::Big5, "\xA7\x41\xA6\x6E", "\xE4\xBD\xA0\xE5\xA5\xBD"},
		{TextEncoding::Encoding::Utf8, "\xF0\x9F\x98\x80", "\xF0\x9F\x98\x80"}
	};
	for (const auto& v : vectors) {
		CHECK(TextEncoding::SetResourceEncoding(v.encoding));
		const auto record = Record(static_cast<std::uint32_t>(std::strlen(v.bytes)), v.bytes);
		Write(record);
		MString value;
		{
			std::ifstream in(path, std::ios::binary);
			value.LoadFromFile(in);
			CHECK(bool(in));
		}
		CHECK(Is(value, v.utf8));
		{
			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			value.SaveToFile(out);
			CHECK(bool(out));
		}
		std::ifstream in(path, std::ios::binary);
		const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), {});
		CHECK(bytes == record);
	}
}

TEST(MStringFiles, DamagedResourceTextDoesNotDiscardLaterTextOrMisalignRecords)
{
	Fixture fixture;
	auto bytes = Record(5, "\xC7\xD1\xFF!\xA1");
	const auto next = Record(3, "end");
	bytes.insert(bytes.end(), next.begin(), next.end());
	Write(bytes);
	std::ifstream in(path, std::ios::binary);
	MString value;
	value.LoadFromFile(in);
	CHECK(bool(in));
	CHECK(Is(value, "\xED\x95\x9C\xEF\xBF\xBD!\xEF\xBF\xBD"));
	CHECK_EQ(9, in.tellg());
	value.LoadFromFile(in);
	CHECK(bool(in));
	CHECK(Is(value, "end"));
	CHECK_EQ(bytes.size(), in.tellg());
}

TEST(MStringFiles, EncodedRecordLimitAllowsUtf8ExpansionInMemory)
{
	Fixture fixture;
	std::string korean, utf8;
	for (size_t i = 0; i < 32768; ++i) {
		korean += "\xC7\xD1";
		utf8 += "\xED\x95\x9C";
	}
	Write(Record(65536, korean));
	MString value;
	{
		std::ifstream in(path, std::ios::binary);
		value.LoadFromFile(in);
		CHECK(bool(in));
	}
	CHECK_EQ(98304, value.GetLength());
	CHECK(Is(value, utf8.c_str()));
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		value.SaveToFile(out);
		CHECK(bool(out));
	}
	std::ifstream in(path, std::ios::binary);
	const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), {});
	CHECK(bytes == Record(65536, korean));
}
