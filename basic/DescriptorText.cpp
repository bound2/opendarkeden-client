#include "DescriptorText.h"
#include "TextWrap.h"

#include <charconv>
#include <utility>

namespace DescriptorText {
namespace {
bool Space(char byte)
{
	return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n';
}
}

bool ReadIndex(std::string_view text, int& index)
{
	while (!text.empty() && Space(text.front())) text.remove_prefix(1);
	while (!text.empty() && Space(text.back())) text.remove_suffix(1);
	if (text.empty()) return false;
	int parsed = 0;
	const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
	if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed < 0)
		return false;
	index = parsed;
	return true;
}

Layout::Layout(size_t columns, size_t maxRows, size_t maxBytes)
	: m_columns(columns), m_maxRows(maxRows), m_maxBytes(maxBytes), m_valid(columns != 0)
{
}

bool Layout::Add(std::string_view text, size_t indent)
{
	if (!m_valid) return false;
	if (m_rows.size() >= m_maxRows || indent > m_maxBytes - m_bytes ||
		text.size() > m_maxBytes - m_bytes - indent) {
		m_valid = false;
		return false;
	}
	std::string row(indent, ' ');
	row.append(text);
	m_bytes += row.size();
	m_rows.push_back(std::move(row));
	return true;
}

void Layout::AdvanceImage()
{
	if (m_imageRows != 0 && --m_imageRows == 0) m_imageInset = 0;
}

bool Layout::FlushImage()
{
	if (!m_valid) return false;
	if (m_imageRows > m_maxRows - m_rows.size() || m_imageRows > m_maxBytes - m_bytes) {
		m_valid = false;
		return false;
	}
	while (m_imageRows != 0) {
		// A required image spacer must survive trimming of empty text rows.
		if (!Add(" ")) return false;
		AdvanceImage();
	}
	return true;
}

bool Layout::BeginImage(size_t insetColumns, size_t heightRows)
{
	if (!FlushImage()) return false;
	m_imageInset = heightRows == 0 ? 0 : insetColumns;
	m_imageRows = heightRows;
	return true;
}

bool Layout::AppendHeader(std::string_view text)
{
	if (!Add(text)) return false;
	m_afterHeader = true;
	return true;
}

bool Layout::AppendLine(std::string_view text)
{
	if (!m_valid) return false;
	if (!text.empty() && text.front() == '\t') {
		m_imageInset = 0;
		return AppendHeader(text);
	}
	if (m_imageRows != 0 && m_imageInset == 0) {
		if (!FlushImage()) return false;
	} else if (m_afterHeader && !Add("")) {
		return false;
	}
	m_afterHeader = false;
	const auto colon = text.find(':');
	const size_t continuationIndent = colon != std::string_view::npos &&
		colon < m_columns / 2 && m_columns / 2 - colon >= 2 ? colon + 2 : 0;
	bool continued = false;
	do {
		const size_t textIndent = continued ? continuationIndent : 0;
		// Check subtraction before forming the combined image/text indent.
		if (m_imageRows != 0 && m_imageInset >= m_columns - textIndent) {
			if (!FlushImage()) return false;
		}
		const size_t indent = textIndent + (m_imageRows != 0 ? m_imageInset : 0);
		const auto row = TextSystem::NextUtf8Line(text, m_columns - indent,
			{.splitNewlines = false});
		if (!Add(row.text, indent)) return false;
		text.remove_prefix(row.consumed);
		AdvanceImage();
		continued = true;
	} while (!text.empty());
	return true;
}

bool Layout::Complete(bool takeTitle, std::vector<std::string>& rows, std::string& title,
	size_t* leadingRowsRemoved)
{
	if (!FlushImage()) return false;
	size_t begin = 0;
	if (takeTitle) {
		title = m_rows.empty() ? std::string{} : m_rows.front();
		if (!m_rows.empty()) ++begin;
	}
	while (begin < m_rows.size() && m_rows[begin].empty()) ++begin;
	while (m_rows.size() > begin && m_rows.back().empty()) m_rows.pop_back();
	m_rows.erase(m_rows.begin(), m_rows.begin() + begin);
	rows = std::move(m_rows);
	if (leadingRowsRemoved) *leadingRowsRemoved = begin;
	m_valid = false;
	return true;
}

} // namespace DescriptorText
