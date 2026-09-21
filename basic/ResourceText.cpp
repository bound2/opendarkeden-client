#include "ResourceText.h"
#include "TextEncoding.h"

namespace ResourceText {
namespace {
bool Space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
}

bool Decode(std::string_view bytes, std::string& output, bool xml)
{
	if (bytes.size() > MaxFileBytes) return false;
	auto encoding = TextEncoding::GetResourceEncoding();
	const bool bom = bytes.starts_with("\xEF\xBB\xBF");
	if (bom) {
		encoding = TextEncoding::Encoding::Utf8;
		bytes.remove_prefix(3);
	}
	if (xml) {
		size_t begin = 0;
		while (begin < bytes.size() && Space(bytes[begin])) ++begin;
		if (bytes.substr(begin).starts_with("<?xml") && begin + 5 < bytes.size() && Space(bytes[begin + 5])) {
			size_t at = begin + 5;
			bool foundEncoding = false;
			for (;;) {
				while (at < bytes.size() && Space(bytes[at])) ++at;
				if (bytes.substr(at).starts_with("?>")) { at += 2; break; }
				const size_t start = at;
				while (at < bytes.size() && bytes[at] >= 'a' && bytes[at] <= 'z') ++at;
				if (start == at) return false;
				const auto name = bytes.substr(start, at - start);
				if (name != "version" && name != "encoding" && name != "standalone") return false;
				while (at < bytes.size() && Space(bytes[at])) ++at;
				if (at == bytes.size() || bytes[at++] != '=') return false;
				while (at < bytes.size() && Space(bytes[at])) ++at;
				if (at == bytes.size() || (bytes[at] != '\'' && bytes[at] != '"')) return false;
				const char quote = bytes[at++];
				const size_t valueBegin = at;
				const size_t end = bytes.find(quote, at);
				if (end == std::string_view::npos) return false;
				const auto value = bytes.substr(valueBegin, end - valueBegin);
				at = end + 1;
				if (at < bytes.size() && !Space(bytes[at]) && !bytes.substr(at).starts_with("?>")) return false;
				if (name == "encoding") {
					if (foundEncoding || !TextEncoding::Parse(value, encoding) ||
						(bom && encoding != TextEncoding::Encoding::Utf8)) return false;
					foundEncoding = true;
				}
			}
			bytes.remove_prefix(at);
		}
	}
	return TextEncoding::Convert(bytes, encoding, TextEncoding::Encoding::Utf8,
		output, TextEncoding::InvalidInput::Replace);
}

} // namespace ResourceText
