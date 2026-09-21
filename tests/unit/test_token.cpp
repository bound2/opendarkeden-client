#include "test_framework.h"
#include "CToken.h"
#include <string>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<CToken>);
static_assert(!std::is_copy_assignable_v<CToken>);

TEST(Token, ProductionTokenizerPreservesTokensAndRemainingText)
{
	CToken token("  first, second and third");
	const char* first = token.GetToken(",");
	CHECK(first != nullptr);
	if (first) CHECK(std::string(first) == "first");
	const char* rest = token.GetEnd();
	CHECK(rest != nullptr);
	if (rest) CHECK(std::string(rest) == "second and third");
	CHECK(token.GetToken() == nullptr);
	CHECK(token.GetEnd() == nullptr);
}

TEST(Token, RepeatedNullResetsClearBothOwnedAndCurrentPointers)
{
	CToken token("first second");
	token.SetString(nullptr);
	CHECK(token.GetToken() == nullptr);
	CHECK(token.GetEnd() == nullptr);
	token.SetString(nullptr);
	CHECK(token.GetToken() == nullptr);
	token.SetString("fresh token");
	const char* first = token.GetToken();
	CHECK(first != nullptr);
	if (first) CHECK(std::string(first) == "fresh");
	token.SetString("");
	const char* empty = token.GetToken();
	CHECK(empty != nullptr);
	if (empty) CHECK(std::string(empty).empty());
	CHECK(token.GetToken() == nullptr);
}

TEST(Token, ResetCanCopyATokenBorrowedFromItsOwnBuffer)
{
	CToken token("first second");
	const char* first = token.GetToken();
	CHECK(first != nullptr);
	token.SetString(first);
	const char* copied = token.GetToken();
	CHECK(copied != nullptr);
	if (copied) CHECK(std::string(copied) == "first");
	CHECK(token.GetToken() == nullptr);
}

TEST(Token, ResetCanCopyAnInteriorTokenOrRemainingSuffix)
{
	CToken token("first second third");
	CHECK(token.GetToken() != nullptr);
	const char* second = token.GetToken();
	CHECK(second != nullptr);
	token.SetString(second);
	const char* copied = token.GetToken();
	CHECK(copied != nullptr);
	if (copied) CHECK(std::string(copied) == "second");
	token.SetString("skip   remaining text");
	CHECK(token.GetToken() != nullptr);
	const char* remaining = token.GetEnd();
	CHECK(remaining != nullptr);
	token.SetString(remaining ? remaining + 4 : nullptr);
	const char* suffix = token.GetEnd();
	CHECK(suffix != nullptr);
	if (suffix) CHECK(std::string(suffix) == "ining text");
}

TEST(Token, RepeatedAliasedResetsPreserveLongTokensAndEmptyTerminators)
{
	const std::string text(4096, 'x');
	CToken token(text.c_str());
	for (int i = 0; i < 100; ++i) {
		const char* borrowed = token.GetEnd();
		CHECK(borrowed != nullptr);
		token.SetString(borrowed);
		const char* copied = token.GetToken();
		CHECK(copied != nullptr);
		if (copied) CHECK(std::string(copied) == text);
		token.SetString(copied);
	}
	const char* borrowed = token.GetEnd();
	CHECK(borrowed != nullptr);
	token.SetString(borrowed ? borrowed + text.size() : nullptr);
	const char* empty = token.GetEnd();
	CHECK(empty != nullptr);
	if (empty) CHECK(std::string(empty).empty());
	CHECK(token.GetEnd() == nullptr);
}
