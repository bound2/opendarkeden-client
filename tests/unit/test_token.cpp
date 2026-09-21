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
