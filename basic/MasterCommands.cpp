#include "MasterCommands.h"
#include "DataPath.h"
#include "ResourceText.h"
#include "TextWrap.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <utility>

namespace MasterCommands {
namespace {
std::string_view Trim(std::string_view text)
{
	while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
	while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
	return text;
}

class Planner {
public:
	const Reader& read;
	Limits limits;
	std::array<bool, 10> active{};
	size_t visited = 0, bytes = 0;
	std::vector<std::string> commands;

	bool Add(unsigned number, size_t depth)
	{
		if (number >= active.size() || active[number] || depth >= limits.maxDepth) return false;
		std::string raw, decoded;
		if (!read(number, raw) || raw.size() > limits.maxFileBytes ||
			raw.find('\0') != std::string::npos ||
			!ResourceText::Decode(raw, decoded, false, TextEncoding::InvalidInput::Reject) ||
			decoded.size() > limits.maxTotalBytes - bytes) return false;
		bytes += decoded.size();
		active[number] = true;
		std::string_view remaining = decoded;
		while (!remaining.empty()) {
			auto line = TextSystem::NextUtf8Line(remaining, remaining.size(), {.skipSeamSpace = false});
			remaining.remove_prefix(line.consumed);
			const auto command = Trim(line.text);
			if (command.empty()) continue;
			if (visited >= limits.maxCommands || command.size() > limits.maxCommandBytes) return false;
			++visited;
			unsigned nested = 0;
			if (Invocation(command, nested)) {
				if (!Add(nested, depth + 1)) return false;
			} else {
				commands.emplace_back(command);
			}
		}
		active[number] = false;
		return true;
	}
};
}

bool Invocation(std::string_view text, unsigned& number)
{
	text = Trim(text);
	if (text.size() != 5 || !text.starts_with("*mc") || (text[3] != ' ' && text[3] != '\t') ||
		text[4] < '0' || text[4] > '9') return false;
	number = static_cast<unsigned>(text[4] - '0');
	return true;
}

bool Expand(unsigned number, const Reader& read, std::vector<std::string>& output, Limits limits)
{
	if (!read) return false;
	Planner planner{read, limits, {}, 0, 0, {}};
	if (!planner.Add(number, 0)) return false;
	output = std::move(planner.commands);
	return true;
}

bool Load(unsigned number, const std::string& directory, std::vector<std::string>& output, Limits limits)
{
	const auto read = [&](unsigned index, std::string& bytes) {
		std::string path = directory;
		if (path.find('\0') != std::string::npos) return false;
		if (!path.empty() && path.back() != '/' && path.back() != '\\') path += '/';
		path += "MasterCommand" + std::to_string(index) + ".txt";
		std::ifstream file(Basic::NormalizeDataPath(path), std::ios::binary | std::ios::ate);
		if (!file) return false;
		const auto length = file.tellg();
		const auto maximum = (std::min)(limits.maxFileBytes, ResourceText::MaxFileBytes);
		if (length < 0 || static_cast<std::uintmax_t>(length) > maximum) return false;
		file.seekg(0);
		std::string candidate(static_cast<size_t>(length), '\0');
		if (!candidate.empty()) file.read(candidate.data(), static_cast<std::streamsize>(candidate.size()));
		if (!file) return false;
		const auto extra = file.peek();
		if (file.bad() || extra != std::char_traits<char>::eof()) return false;
		bytes = std::move(candidate);
		return true;
	};
	return Expand(number, read, output, limits);
}
}
