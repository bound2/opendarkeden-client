#include "Client_PCH.h"
#include "MHelpMessageManager.h"
#include "RarFile.h"
#include "ResourceText.h"
#include "TextUtf8.h"
#include "VS_UI_filepath.h"

#include <array>
#include <charconv>

MHelpMessage::MHelpMessage() : m_messageType(MESSAGETYPE_NORMAL)
{
	for (int race = 0; race < RACE_MAX; ++race) {
		m_iSender[race] = -1;
		m_iLevelLow[race] = m_iLevelMax[race] = -1;
		m_iDomainLow[race] = m_iDomainMax[race] = -1;
		m_iAttrLow[race] = m_iAttrMax[race] = -1;
	}
}
MHelpMessage::~MHelpMessage() = default;

bool MHelpMessage::IsEligible(int race, int level, long long attributes) const
{
	if (race < 0 || race >= RACE_MAX) return false;
	const bool slayer = race == RACE_SLAYER;
	const int minimum = slayer ? m_iAttrLow[race] : m_iLevelLow[race];
	const int maximum = slayer ? m_iAttrMax[race] : m_iLevelMax[race];
	if (minimum == -1) return true;
	const long long value = slayer ? attributes : level;
	return minimum >= 0 && value >= minimum && value <= maximum;
}

namespace {
constexpr int MaxEntries = 65536;
constexpr std::string_view LevelTag = "[==Level 조건표==]";
bool Space(char c) { return c == ' ' || c == '\t' || c == '\r'; }
std::string_view Trim(std::string_view value)
{
	while (!value.empty() && Space(value.front())) value.remove_prefix(1);
	while (!value.empty() && Space(value.back())) value.remove_suffix(1);
	return value;
}
bool Integer(std::string_view value, int& result)
{
	value = Trim(value);
	if (value.empty()) return false;
	const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
	return parsed.ec == std::errc() && parsed.ptr == value.data() + value.size();
}
class Lines {
	std::string_view remaining;
public:
	explicit Lines(std::string_view value) : remaining(value) {}
	bool Next(std::string_view& line) {
		if (remaining.empty()) return false;
		const size_t end = remaining.find('\n');
		line = remaining.substr(0, end);
		remaining.remove_prefix(end == std::string_view::npos ? remaining.size() : end + 1);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		return true;
	}
	bool Tag(std::string_view& line) {
		while (Next(line)) {
			line = Trim(line);
			if (!line.empty() && line.front() != '#' && line.front() != ';') return true;
		}
		return false;
	}
};
bool Levels(std::string_view line, MHelpMessage& message, int race)
{
	std::array<int, 6> values;
	for (auto& value : values) {
		line = Trim(line);
		const size_t end = line.find_first_of(" \t\r");
		if (!Integer(line.substr(0, end), value)) return false;
		line.remove_prefix(end == std::string_view::npos ? line.size() : end);
	}
	if (!Trim(line).empty()) return false;
	for (size_t i = 0; i < values.size(); i += 2)
		if (values[i] < -1 || values[i + 1] < -1 ||
			(values[i] >= 0 && values[i + 1] < values[i])) return false;
	message.m_iLevelLow[race] = values[0]; message.m_iLevelMax[race] = values[1];
	message.m_iDomainLow[race] = values[2]; message.m_iDomainMax[race] = values[3];
	message.m_iAttrLow[race] = values[4]; message.m_iAttrMax[race] = values[5];
	return true;
}
bool Value(Lines& lines, MString& destination)
{
	std::string_view line;
	if (!lines.Next(line)) return false;
	while (!line.empty() && Space(line.front())) line.remove_prefix(1);
	destination = std::string(line).c_str();
	return true;
}
bool Parse(std::string_view text, std::vector<MString>& senders, std::vector<MHelpMessage>& messages)
{
	if (text.find('\0') != std::string_view::npos) return false;
	Lines lines(text);
	std::string_view line;
	int senderCount = 0, messageCount = 0;
	if (!lines.Next(line) || !Integer(line, senderCount) || senderCount < 0 || senderCount > MaxEntries ||
		!lines.Next(line) || Trim(line) != "[===Sender===]") return false;
	for (int i = 0; i < senderCount; ++i) {
		if (!lines.Next(line)) return false;
		senders.emplace_back(std::string(line).c_str());
	}
	if (!lines.Next(line) || !Integer(line, messageCount) || messageCount < 0 || messageCount > MaxEntries) return false;
	for (int i = 0; i < messageCount; ++i) {
		MHelpMessage message;
		if (!lines.Tag(line) || line != "[===KeyWord===]" || !Value(lines, message.m_strKeyword)) return false;
		unsigned globalFields = 0;
		for (int race = 0; race < RACE_MAX; ++race) {
			unsigned fields = 0;
			for (;;) {
				if (!lines.Tag(line)) return false;
				if (line == "[===MessageType===]") {
					if (race != 0 || (globalFields & 1) || !lines.Next(line) || !Integer(line, message.m_messageType) ||
						message.m_messageType < MHelpMessage::MESSAGETYPE_NORMAL ||
						message.m_messageType > MHelpMessage::MESSAGETYPE_SAFESECTOR_OPEN) return false;
					globalFields |= 1;
				} else if (line == "[===Event===]") {
					if (race != 0 || (globalFields & 2) || !Value(lines, message.m_strEvent)) return false;
					globalFields |= 2;
				} else if (line == "[==Title==]") {
					if ((fields & 1) || !Value(lines, message.m_strTitle[race])) return false;
					fields |= 1;
				} else if (line == "[==Sender==]") {
					if ((fields & 2) || !lines.Next(line) || !Integer(line, message.m_iSender[race]) ||
						message.m_iSender[race] < -1 || message.m_iSender[race] >= senderCount) return false;
					fields |= 2;
				} else if (line == LevelTag) {
					if ((fields & 4) || !lines.Next(line) || !Levels(line, message, race)) return false;
					fields |= 4;
				} else if (line == "[==Detail==]") {
					std::string detail;
					bool first = true, ended = false;
					while (lines.Next(line)) {
						if (Trim(line) == "{End}") { ended = true; break; }
						if (!first) detail += "\r\n";
						detail += line;
						first = false;
					}
					if (!ended) return false;
					message.m_strDetail[race] = detail.c_str();
					break;
				} else return false;
			}
		}
		messages.push_back(message);
	}
	return !lines.Tag(line);
}
bool ReadBounded(std::ifstream& file, std::string& text)
{
	std::array<char, 4096> buffer;
	while (file) {
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		const auto size = static_cast<size_t>(file.gcount());
		if (size > ResourceText::MaxFileBytes - text.size()) return false;
		text.append(buffer.data(), size);
	}
	return file.eof() && !file.bad();
}
}

MHelpMessageManager& MHelpMessageManager::Instance()
{
	static MHelpMessageManager instance;
	static const bool loaded = instance.LoadHelpMessageRpk("helpmessage.txt");
	(void)loaded;
	return instance;
}
bool MHelpMessageManager::LoadUtf8(std::string_view text)
{
	std::vector<MString> senders;
	std::vector<MHelpMessage> messages;
	if (!Parse(text, senders, messages)) return false;
	m_SenderVector.swap(senders);
	m_MessageVector.swap(messages);
	return true;
}
bool MHelpMessageManager::LoadFromText(std::string_view encoded)
{
	std::string utf8;
	return ResourceText::Decode(encoded, utf8) && LoadUtf8(utf8);
}
bool MHelpMessageManager::LoadHelpMessageRpk(const char* filename)
{
	CRarFile file(RPK_HELP, RPK_PASSWORD);
	return file.OpenText(filename) && LoadUtf8(std::string_view(file.GetFilePointer(), file.GetRemainingSize()));
}
void MHelpMessageManager::LoadFromFile(std::ifstream& file)
{
	std::string bytes;
	if (!ReadBounded(file, bytes)) { file.setstate(std::ios::failbit); return; }
	file.clear(file.rdstate() & ~std::ios::failbit);
	if (!LoadFromText(bytes)) file.setstate(std::ios::failbit);
}
void MHelpMessageManager::LoadFromFile(const char* filename)
{
	if (!filename) return;
	std::ifstream file(filename, std::ios::binary);
	if (file) LoadFromFile(file);
}

bool MHelpMessageManager::Serialize(std::string& output) const
{
	if (m_SenderVector.size() > MaxEntries || m_MessageVector.size() > MaxEntries) return false;
	std::string bytes("\xEF\xBB\xBF");
	auto line = [&](std::string_view value) {
		const size_t remaining = ResourceText::MaxFileBytes - bytes.size();
		if (remaining < 2 || value.size() > remaining - 2 ||
			value.find_first_of("\r\n\0", 0, 3) != std::string_view::npos ||
			!TextSystem::IsValidUtf8(value.data(), value.size())) return false;
		bytes += value; bytes += "\r\n"; return true;
	};
	if (!line(std::to_string(m_SenderVector.size())) || !line("[===Sender===]")) return false;
	for (const auto& sender : m_SenderVector) if (!line(sender.GetString())) return false;
	if (!line(std::to_string(m_MessageVector.size()))) return false;
	for (const auto& message : m_MessageVector) {
		if (!line("[===KeyWord===]") || !line(message.m_strKeyword.GetString()) ||
			!line("[===MessageType===]") || !line(std::to_string(message.m_messageType)) ||
			!line("[===Event===]") || !line(message.m_strEvent.GetString())) return false;
		for (int race = 0; race < RACE_MAX; ++race) {
			const std::string levels = std::to_string(message.m_iLevelLow[race]) + " " + std::to_string(message.m_iLevelMax[race]) + " " +
				std::to_string(message.m_iDomainLow[race]) + " " + std::to_string(message.m_iDomainMax[race]) + " " +
				std::to_string(message.m_iAttrLow[race]) + " " + std::to_string(message.m_iAttrMax[race]);
			if (!line("[==Title==]") || !line(message.m_strTitle[race].GetString()) ||
				!line("[==Sender==]") || !line(std::to_string(message.m_iSender[race])) ||
				!line(LevelTag) || !line(levels) || !line("[==Detail==]")) return false;
			Lines details(message.m_strDetail[race].GetString());
			std::string_view part;
			while (details.Next(part)) if (Trim(part) == "{End}" || !line(part)) return false;
			const std::string_view detail(message.m_strDetail[race].GetString());
			if (!detail.empty() && detail.back() == '\n' && !line("")) return false;
			if (!line("{End}")) return false;
		}
	}
	output.swap(bytes);
	return true;
}
void MHelpMessageManager::SaveToFile(std::ofstream& file)
{
	std::string bytes;
	if (!Serialize(bytes)) { file.setstate(std::ios::failbit); return; }
	file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}
void MHelpMessageManager::SaveToFile(const char* filename)
{
	std::string bytes;
	if (!filename || !Serialize(bytes)) return;
	std::ofstream file(filename, std::ios::binary | std::ios::trunc);
	if (file) file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}
