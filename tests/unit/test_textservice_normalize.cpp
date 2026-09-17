//----------------------------------------------------------------------
// test_textservice_normalize.cpp
//----------------------------------------------------------------------
//
// Tests for TextService::NormalizeText in Client/TextSystem/TextService.cpp.
//
// TextService's layout and drawing paths call this function before decoding
// UTF-8. MString::LoadFromFile has a separate platform-dependent conversion;
// these tests do not exercise that loader. Two properties must hold here:
//
//   - text that is already valid UTF-8 comes back untouched, or a table that
//     has been converted ahead of time would be decoded a second time;
//   - CP949 is tried before the Chinese code pages, since the byte ranges
//     overlap and Korean data decoded as GBK yields plausible-looking but
//     wrong Chinese.
//
// The vectors below are real bytes from Data/Info/NPCScript.inf.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "TextService.h"

#include <string>

namespace {

//----------------------------------------------------------------------
// "그루버", the first speaker name in NPCScript.inf.
//----------------------------------------------------------------------
const char CP949_GRUBER[] = "\xB1\xD7\xB7\xE7\xB9\xF6";
const char UTF8_GRUBER[]  = "\xEA\xB7\xB8\xEB\xA3\xA8\xEB\xB2\x84";

//----------------------------------------------------------------------
// "한", a single syllable, to cover the shortest possible double-byte run.
//----------------------------------------------------------------------
const char CP949_HAN[] = "\xC7\xD1";
const char UTF8_HAN[]  = "\xED\x95\x9C";

std::string Normalize(const std::string& in)
{
	return TextSystem::TextService::NormalizeText(in);
}

} // namespace

//----------------------------------------------------------------------
// ASCII is already valid UTF-8 and must survive untouched. This is the
// common case: most of String.inf is plain English.
//----------------------------------------------------------------------
TEST(TextServiceNormalize, AsciiIsUnchanged)
{
	const std::string ascii = "Vrykolakas";

	CHECK(ascii == Normalize(ascii));
}

//----------------------------------------------------------------------
// An empty string must not be treated as a failed conversion.
//----------------------------------------------------------------------
TEST(TextServiceNormalize, EmptyStringIsUnchanged)
{
	CHECK(std::string() == Normalize(std::string()));
}

//----------------------------------------------------------------------
// Already-UTF-8 input must not be run through a legacy decoder. CP949
// would happily reinterpret these bytes, so this is the guard against
// double decoding a table that someone has already converted.
//----------------------------------------------------------------------
TEST(TextServiceNormalize, ValidUtf8IsNotDecodedTwice)
{
	const std::string utf8(UTF8_GRUBER);

	CHECK(utf8 == Normalize(utf8));
}

//----------------------------------------------------------------------
// The case the NPC dialogue actually hits.
//----------------------------------------------------------------------
TEST(TextServiceNormalize, Cp949KoreanBecomesUtf8)
{
	CHECK(std::string(UTF8_GRUBER) == Normalize(std::string(CP949_GRUBER)));
}

TEST(TextServiceNormalize, Cp949SingleSyllableBecomesUtf8)
{
	CHECK(std::string(UTF8_HAN) == Normalize(std::string(CP949_HAN)));
}

//----------------------------------------------------------------------
// Mixed ASCII and CP949 is what most .inf rows look like: a name or a
// format specifier next to Korean prose.
//----------------------------------------------------------------------
TEST(TextServiceNormalize, MixedAsciiAndCp949Converts)
{
	const std::string mixed = std::string("[") + CP949_HAN + "] %d";
	const std::string want  = std::string("[") + UTF8_HAN + "] %d";

	CHECK(want == Normalize(mixed));
}
