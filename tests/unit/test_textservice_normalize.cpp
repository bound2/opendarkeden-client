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

struct CacheScope {
	explicit CacheScope(TextSystem::NormalizationCacheLimits limits = {})
	{
		TextSystem::TextService::ResetNormalizationCache(limits);
	}
	~CacheScope() { TextSystem::TextService::ResetNormalizationCache(); }
};

class EmptyTarget : public TextSystem::RenderTarget {
public:
	void* GetNative(TextSystem::NativeTargetType) const override { return nullptr; }
	int GetWidth() const override { return 200; }
	int GetHeight() const override { return 40; }
};

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

TEST(TextServiceNormalize, RepeatedLabelsAndRejectedInputAreComputedOnce)
{
	CacheScope cache;
	const std::string label = std::string("[") + CP949_GRUBER + "]";
	const std::string expected = std::string("[") + UTF8_GRUBER + "]";
	const std::string malformed("\x81");
	for (int i = 0; i < 20; ++i) {
		CHECK(expected == Normalize(label));
		CHECK(malformed == Normalize(malformed));
	}
	const auto stats = TextSystem::TextService::GetNormalizationCacheStats();
	CHECK_EQ(2, stats.computations);
	CHECK_EQ(38, stats.hits);
	CHECK_EQ(2, stats.entries);
}

TEST(TextServiceNormalize, CacheKeysOwnAllBytesAndEvictTheLeastRecentLabel)
{
	CacheScope cache({2, 4096});
	std::string first("a\0b", 3), second("a\0c", 3);
	CHECK(first == Normalize(first));
	CHECK(second == Normalize(second));
	CHECK(first == Normalize(first));
	CHECK(std::string("third") == Normalize("third"));
	first[2] = 'z';
	CHECK(std::string("a\0b", 3) == Normalize(std::string("a\0b", 3)));
	CHECK_EQ(3, TextSystem::TextService::GetNormalizationCacheStats().computations);
	CHECK(second == Normalize(second));
	CHECK_EQ(4, TextSystem::TextService::GetNormalizationCacheStats().computations);
	CHECK_EQ(2, TextSystem::TextService::GetNormalizationCacheStats().entries);
}

TEST(TextServiceNormalize, RetainedStorageIsBoundedAndOversizedInputsBypassCache)
{
	CacheScope cache({32, 96});
	for (int i = 0; i < 20; ++i) {
		const std::string input(40, static_cast<char>('a' + i));
		CHECK(input == Normalize(input));
		const auto stats = TextSystem::TextService::GetNormalizationCacheStats();
		CHECK(stats.entries <= 32);
		CHECK(stats.storageBytes <= 96);
	}
	TextSystem::TextService::ResetNormalizationCache({32, 96});
	CHECK(std::string("hot") == Normalize("hot"));
	const std::string huge(10000, 'x');
	CHECK(huge == Normalize(huge));
	CHECK(std::string("hot") == Normalize("hot"));
	CHECK_EQ(2, TextSystem::TextService::GetNormalizationCacheStats().computations);
	CHECK_EQ(1, TextSystem::TextService::GetNormalizationCacheStats().entries);
	CHECK_EQ(1, TextSystem::TextService::GetNormalizationCacheStats().hits);
	TextSystem::TextService::ResetNormalizationCache({0, 96});
	CHECK(std::string("same") == Normalize("same"));
	CHECK(std::string("same") == Normalize("same"));
	CHECK_EQ(2, TextSystem::TextService::GetNormalizationCacheStats().computations);
	CHECK_EQ(0, TextSystem::TextService::GetNormalizationCacheStats().entries);
}

TEST(TextServiceNormalize, MeasurementWrappingAndDrawingShareNormalizedLabels)
{
	CacheScope cache;
	auto& service = TextSystem::TextService::Get();
	const auto style = service.GetDefaultStyle();
	CHECK(style.font.IsValid());
	EmptyTarget target;
	const std::string label = std::string("[") + CP949_GRUBER + "]";
	const std::string expected = std::string("[") + UTF8_GRUBER + "]";
	for (int i = 0; i < 3; ++i) {
		const auto metrics = service.MeasureText(label, style);
		CHECK(metrics.width > 0);
		CHECK(metrics.height > 0);
		const auto lines = service.WrapText(label, style, 0);
		CHECK_EQ(1, lines.size());
		if (!lines.empty()) CHECK(expected == lines.front());
		service.DrawLine(target, label, 0, 0, 0, style);
	}
	CHECK_EQ(1, TextSystem::TextService::GetNormalizationCacheStats().computations);
	CHECK_EQ(8, TextSystem::TextService::GetNormalizationCacheStats().hits);
}
