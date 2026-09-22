#include "ShrineInfoManager.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <memory>
#include <string_view>

namespace {
constexpr size_t MaxLineBytes = 512;
constexpr size_t MaxLines = 16384;
constexpr int MaxEntries = RegenTowerInfo::MaxCount;

bool White(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

void SkipWhite(std::string_view& text)
{
	while (!text.empty() && White(text.front())) text.remove_prefix(1);
}

bool Number(std::string_view& text, int& value)
{
	SkipWhite(text);
	if (text.empty()) return false;
	if (text.front() == '+') {
		text.remove_prefix(1);
		if (text.empty() || text.front() < '0' || text.front() > '9') return false;
	}
	const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
	if (result.ec != std::errc{}) return false;
	text.remove_prefix(static_cast<size_t>(result.ptr - text.data()));
	return text.empty() || White(text.front()) || text.front() == ';';
}

bool EndOrComment(std::string_view text)
{
	SkipWhite(text);
	return text.empty() || text.front() == ';';
}
}

bool RegenTowerInfo::LoadFromLine(const char* line)
{
	if (line == nullptr || std::strlen(line) >= MaxLineBytes) return false;
	std::string_view text(line);
	RegenTowerInfo pending;
	if (!Number(text, pending.num) || !Number(text, pending.zoneID) ||
		!Number(text, pending.x) || !Number(text, pending.y) || !EndOrComment(text) ||
		!pending.IsValid()) return false;
	*this = pending;
	return true;
}

bool RegenTowerInfo::IsValid() const
{
	return num >= 0 && num < MaxCount && zoneID >= 71 && zoneID <= 73 &&
		x >= 0 && x < MapWidth && y >= 0 && y < MapHeight;
}

RegenTowerInfoManager::RegenTowerInfoManager()
{
}

bool RegenTowerInfoManager::LoadRegenTowerInfoLines(const RegenTowerLineReader& reader)
{
	if (reader.GetString == nullptr) return false;
	try {
		std::unique_ptr<RegenTowerInfo[]> pending;
		std::vector<bool> seen;
		int count = -1, complete = 0;
		size_t lines = 0, bytes = 0;
		std::array<char, MaxLineBytes + 1> buffer;
		for (;;) {
			buffer.fill(static_cast<char>(0xff));
			if (!reader.GetString(reader.context, buffer.data(), static_cast<int>(buffer.size()))) break;
			const auto end = std::find(buffer.begin(), buffer.end(), '\0');
			const size_t size = static_cast<size_t>(end - buffer.begin());
			// GetString consumes the whole source line even when clipping it. One
			// extra byte distinguishes a maximum valid line from a clipped prefix;
			// an unterminated callback buffer is rejected too.
			if (++lines > MaxLines || size >= MaxLineBytes || size + 1 > MaxTextBytes - bytes) return false;
			bytes += size + 1;
			std::string_view text(buffer.data(), size);
			SkipWhite(text);
			if (EndOrComment(text)) continue;
			if (text.front() == '*') {
				if (count != -1) return false;
				text.remove_prefix(1);
				if (!Number(text, count) || !EndOrComment(text) || count < 0 || count > MaxEntries) return false;
				if (count > 0) pending = std::make_unique<RegenTowerInfo[]>(count);
				seen.resize(static_cast<size_t>(count), false);
				continue;
			}
			RegenTowerInfo row;
			if (count < 0 || !row.LoadFromLine(buffer.data()) || row.num >= count ||
				seen[static_cast<size_t>(row.num)]) return false;
			pending[row.num] = row;
			seen[static_cast<size_t>(row.num)] = true;
			++complete;
		}
		if (count < 0 || complete != count) return false;
		std::unique_ptr<RegenTowerInfo[]> previous(m_pTypeInfo);
		m_pTypeInfo = pending.release();
		m_Size = count;
		return true;
	} catch (...) {
		return false;
	}
}
