#include "StringReduction.h"
#include "TextUtf8.h"
#include <cstring>

namespace {
void ReduceUtf8(char* text, int maxBytes)
{
	if (!text || maxBytes <= 0) return;
	const size_t length = std::strlen(text);
	const size_t limit = static_cast<size_t>(maxBytes);
	if (length <= limit) return;
	const size_t markerBytes = limit >= 3 ? 3 : 0;
	const size_t prefix = TextSystem::Utf8PrefixBytes(
		std::string_view(text, length), limit - markerBytes);
	// prefix + markerBytes <= limit < length: no spare capacity is needed.
	if (markerBytes) std::memcpy(text + prefix, "...", markerBytes);
	text[prefix + markerBytes] = '\0';
}
}

void ReduceString(char* text, int maxBytes)
{
	ReduceUtf8(text, maxBytes);
}

void ReduceString2(char* text, int maxBytes)
{
	ReduceUtf8(text, maxBytes);
}

void ReduceString3(char* text, int maxBytes)
{
	ReduceUtf8(text, maxBytes);
}
