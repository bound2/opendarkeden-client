#include "Platform.h"
#include "SXml.h"
#include "ResourceText.h"

#include <charconv>
#include <cstdint>
#include <memory>
#include <string_view>

namespace {
bool Space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

bool XmlScalar(uint32_t value)
{
	return value == 9 || value == 10 || value == 13 ||
		(value >= 0x20 && value <= 0xD7FF) || (value >= 0xE000 && value <= 0xFFFD) ||
		(value >= 0x10000 && value <= 0x10FFFF);
}

void AppendUtf8(std::string& out, uint32_t value)
{
	if (value < 0x80) out += char(value);
	else if (value < 0x800) {
		out += char(0xC0 | (value >> 6)); out += char(0x80 | (value & 0x3F));
	} else if (value < 0x10000) {
		out += char(0xE0 | (value >> 12)); out += char(0x80 | ((value >> 6) & 0x3F)); out += char(0x80 | (value & 0x3F));
	} else {
		out += char(0xF0 | (value >> 18)); out += char(0x80 | ((value >> 12) & 0x3F));
		out += char(0x80 | ((value >> 6) & 0x3F)); out += char(0x80 | (value & 0x3F));
	}
}

bool DecodeEntities(std::string_view text, std::string& out)
{
	for (size_t at = 0; at < text.size();) {
		if (text[at] != '&') {
			if (static_cast<unsigned char>(text[at]) < 0x20 && !Space(text[at])) return false;
			out += text[at++];
			continue;
		}
		const size_t end = text.find(';', at + 1);
		if (end == std::string_view::npos) return false;
		auto entity = text.substr(at + 1, end - at - 1);
		if (entity == "amp") out += '&';
		else if (entity == "lt") out += '<';
		else if (entity == "gt") out += '>';
		else if (entity == "quot") out += '"';
		else if (entity == "apos") out += '\'';
		else if (entity.starts_with('#')) {
			entity.remove_prefix(1);
			int base = 10;
			if (entity.starts_with('x')) { entity.remove_prefix(1); base = 16; }
			if (entity.empty()) return false;
			uint32_t scalar = 0;
			const auto parsed = std::from_chars(entity.data(), entity.data() + entity.size(), scalar, base);
			if (parsed.ec != std::errc() || parsed.ptr != entity.data() + entity.size() || !XmlScalar(scalar)) return false;
			AppendUtf8(out, scalar);
		} else return false;
		at = end + 1;
	}
	return true;
}

class Reader {
public:
	Reader(std::string_view input, bool repeated) : text(input), repeatedNames(repeated) {}
	bool Document(XMLTree& root)
	{
		if (!Trivia() || !Element(root, 0) || !Trivia()) return false;
		return position == text.size();
	}

private:
	std::string_view text;
	size_t position = 0;
	size_t nodes = 0;
	bool repeatedNames;

	bool At(std::string_view token) const { return text.substr(position).starts_with(token); }
	void SkipSpace() { while (position < text.size() && Space(text[position])) ++position; }
	bool Comment()
	{
		const size_t end = text.find("--", position + 4);
		if (end == std::string_view::npos || !text.substr(end).starts_with("-->")) return false;
		position = end + 3;
		return true;
	}
	bool Instruction()
	{
		if (At("<?xml") && position + 5 < text.size() &&
			(Space(text[position + 5]) || text[position + 5] == '?')) return false;
		const size_t end = text.find("?>", position + 2);
		if (end == std::string_view::npos) return false;
		position = end + 2;
		return true;
	}
	bool Trivia()
	{
		for (;;) {
			SkipSpace();
			if (At("<!--")) { if (!Comment()) return false; }
			else if (At("<?")) { if (!Instruction()) return false; }
			else return true;
		}
	}
	bool Name(std::string& name)
	{
		const size_t begin = position;
		while (position < text.size()) {
			const auto c = static_cast<unsigned char>(text[position]);
			const bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0x80 || c == '_' || c == ':';
			if (!letter && !(position > begin && ((c >= '0' && c <= '9') || c == '-' || c == '.'))) break;
			++position;
		}
		if (begin == position) return false;
		name.assign(text.substr(begin, position - begin));
		return true;
	}
	bool Element(XMLTree& tree, size_t depth)
	{
		if (depth >= 64 || ++nodes > 100000 || !At("<")) return false;
		++position;
		std::string name;
		if (!Name(name)) return false;
		tree.SetName(name);
		for (;;) {
			const size_t beforeSpace = position;
			SkipSpace();
			if (At("/>")) { position += 2; return true; }
			if (At(">")) { ++position; break; }
			if (beforeSpace == position) return false;
			std::string key, value;
			if (!Name(key) || tree.GetAttribute(key)) return false;
			SkipSpace();
			if (!At("=")) return false;
			++position;
			SkipSpace();
			if (position == text.size() || (text[position] != '\'' && text[position] != '"')) return false;
			const char quote = text[position++];
			const size_t end = text.find(quote, position);
			if (end == std::string_view::npos) return false;
			const auto raw = text.substr(position, end - position);
			if (raw.find('<') != std::string_view::npos || !DecodeEntities(raw, value)) return false;
			if (++nodes > 100000) return false;
			tree.AddAttribute(key, value);
			position = end + 1;
		}
		std::string content;
		for (;;) {
			if (At("</")) {
				position += 2;
				std::string closing;
				if (!Name(closing) || closing != name) return false;
				SkipSpace();
				if (!At(">")) return false;
				++position;
				// The legacy tree stores trimmed text separately from its children.
				size_t first = 0, last = content.size();
				while (first < last && Space(content[first])) ++first;
				while (last > first && Space(content[last - 1])) --last;
				tree.SetText(content.substr(first, last - first));
				return true;
			}
			if (At("<!--")) { if (!Comment()) return false; continue; }
			if (At("<?")) { if (!Instruction()) return false; continue; }
			if (At("<![CDATA[")) {
				const size_t end = text.find("]]>", position + 9);
				if (end == std::string_view::npos) return false;
				content.append(text.substr(position + 9, end - position - 9));
				position = end + 3;
				continue;
			}
			if (At("<")) {
				auto child = std::make_unique<XMLTree>();
				if (!Element(*child, depth + 1) || (!repeatedNames && tree.GetChild(child->GetName()))) return false;
				tree.AddChildOnlyVector(child.get());
				child->SetParent(&tree);
				child.release();
				continue;
			}
			const size_t end = text.find('<', position);
			if (end == std::string_view::npos || !DecodeEntities(text.substr(position, end - position), content)) return false;
			position = end;
		}
	}
};
}

void XMLTree::AppendParsed(XMLTree& incoming, bool repeatedNames)
{
	// Prepare non-owning container copies before transferring ownership. If an
	// allocation fails, both trees retain their original owners and contents.
	auto attributes = m_AttributesMap;
	auto attributeOrder = m_AttributesVector;
	for (auto* attribute : incoming.m_AttributesVector) {
		if (attributes.emplace(attribute->GetName(), attribute).second) attributeOrder.push_back(attribute);
	}
	auto children = m_ChildrenMap;
	auto childOrder = m_ChildrenVector;
	for (auto* child : incoming.m_ChildrenVector) {
		if (repeatedNames || !children.contains(child->GetName())) {
			children.emplace(child->GetName(), child);
			childOrder.push_back(child);
		}
	}
	// Delete incoming duplicate attributes/children which were not adopted.
	for (auto* attribute : incoming.m_AttributesVector)
		if (attributes.at(attribute->GetName()) != attribute) delete attribute;
	for (auto* child : incoming.m_ChildrenVector)
		if (!repeatedNames && children.at(child->GetName()) != child) delete child;
	m_Name.swap(incoming.m_Name);
	if (!incoming.m_Text.empty()) m_Text.swap(incoming.m_Text);
	m_AttributesMap.swap(attributes);
	m_AttributesVector.swap(attributeOrder);
	m_ChildrenMap.swap(children);
	m_ChildrenVector.swap(childOrder);
	incoming.m_AttributesMap.clear();
	incoming.m_AttributesVector.clear();
	incoming.m_ChildrenMap.clear();
	incoming.m_ChildrenVector.clear();
	for (auto* child : m_ChildrenVector) child->SetParent(this);
}

char* XMLParser::parse(char* buffer, XMLTree* tree, bool repeatedNames)
{
	if (!buffer || !tree) return nullptr;
	size_t bytes = 0;
	while (bytes <= ResourceText::MaxFileBytes && buffer[bytes] != '\0') ++bytes;
	if (bytes > ResourceText::MaxFileBytes) return nullptr;
	std::string utf8;
	if (!ResourceText::Decode(std::string_view(buffer, bytes), utf8, true)) return nullptr;
	XMLTree parsed;
	if (!Reader(utf8, repeatedNames).Document(parsed)) return nullptr;
	tree->AppendParsed(parsed, repeatedNames);
	return buffer + bytes;
}
