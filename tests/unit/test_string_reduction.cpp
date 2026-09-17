#include "test_framework.h"
#include "StringReduction.h"

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
