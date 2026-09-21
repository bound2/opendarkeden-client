#include "test_framework.h"
#include "StringReduction.h"
#include "TextUtf8.h"

#include <cstring>
#include <limits>
#include <memory>
#include <string>

namespace {
using Reducer = void (*)(char*, int);
constexpr Reducer reducers[] = {ReduceString, ReduceString2, ReduceString3};
}

TEST(StringReduction, ShortStringsNeedNoSpareCapacity)
{
	for (const auto reduce : reducers)
	{
		for (int length = 0; length <= 40; ++length)
		{
			const std::string original(length, 'x');
			for (const int limit : {length, length + 1, length + 2, 100,
				std::numeric_limits<int>::max()})
			{
				auto text = std::make_unique<char[]>(original.size() + 1);
				std::memcpy(text.get(), original.c_str(), original.size() + 1);
				reduce(text.get(), limit);
				CHECK(original == text.get());
			}
		}
	}
}

TEST(StringReduction, WritesStayInsideTheOriginalString)
{
	for (const auto reduce : reducers)
	{
		for (int length = 0; length <= 40; ++length)
		{
			for (int limit = -3; limit <= 43; ++limit)
			{
				std::string storage(length + 3, '#');
				std::memset(storage.data() + 1, 'x', length);
				storage[length + 1] = '\0';
				reduce(storage.data() + 1, limit);
				CHECK_EQ('#', storage.front());
				CHECK_EQ('#', storage.back());
				CHECK(std::memchr(storage.data() + 1, '\0', length + 1) != nullptr);
			}
		}
	}
}

TEST(StringReduction, ExistingByteTruncationAndNonPositiveLimits)
{
	for (const auto reduce : reducers)
	{
		reduce(nullptr, 36);
		for (const int limit : {std::numeric_limits<int>::min(), -1, 0})
		{
			char text[] = "unchanged";
			reduce(text, limit);
			CHECK(std::string("unchanged") == text);
		}
		char text[] = "abcdefghijklmnop";
		reduce(text, 9);
		CHECK(std::string("abcdef...") == text);
	}
}

TEST(StringReduction, DanglingHighByteDoesNotScanPastTerminator)
{
	for (const auto reduce : reducers)
	{
		auto text = std::make_unique<char[]>(2);
		text[0] = static_cast<char>(0x81);
		text[1] = '\0';
		reduce(text.get(), 36);
		CHECK_EQ(0x81, static_cast<unsigned char>(text[0]));
		CHECK_EQ('\0', text[1]);
	}
}

TEST(StringReduction, TruncationPreservesCompleteUtf8Scalars)
{
	const std::string text = "a\xC3\xA9\xF0\x9F\x99\x82Z";
	const std::string expected[] = {
		text, "a", "a", "...", "a...", "a...", "a\xC3\xA9...", "a\xC3\xA9...", text
	};
	for (const auto reduce : reducers) {
		for (int limit = 0; limit <= 8; ++limit) {
			auto copy = std::make_unique<char[]>(text.size() + 1);
			std::memcpy(copy.get(), text.c_str(), text.size() + 1);
			reduce(copy.get(), limit);
			CHECK(std::string(copy.get()) == expected[limit]);
			CHECK(TextSystem::IsValidUtf8(copy.get(), std::strlen(copy.get())));
		}
	}
}

TEST(StringReduction, MultibyteFirstScalarsAndSmallLimitsCannotSplitCharacters)
{
	const std::string text = "\xEA\xB0\x80\xEB\x82\x98\xEB\x8B\xA4";
	const std::string expected[] = {
		text, "", "", "...", "...", "...", "\xEA\xB0\x80...",
		"\xEA\xB0\x80...", "\xEA\xB0\x80...", text
	};
	for (const auto reduce : reducers) {
		for (int limit = 0; limit <= 9; ++limit) {
			std::string storage = text;
			reduce(storage.data(), limit);
			CHECK(std::string(storage.c_str()) == expected[limit]);
			CHECK(TextSystem::IsValidUtf8(storage.c_str(), std::strlen(storage.c_str())));
		}
	}
}
