#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace DescriptorText {

// Resource tags contain a complete, nonnegative decimal index. Failure does
// not change the output. Leading/trailing ASCII whitespace is accepted.
bool ReadIndex(std::string_view text, int& index);

// Owns one candidate description. A failed operation invalidates it; Complete
// then leaves the caller's published rows/title unchanged. Bounds include
// inserted spaces and image rows, not only the input bytes.
class Layout {
public:
	explicit Layout(size_t columns, size_t maxRows = 65536, size_t maxBytes = 16 * 1024 * 1024);
	bool AppendHeader(std::string_view text);
	bool AppendLine(std::string_view text);
	bool BeginImage(size_t insetColumns, size_t heightRows);
	bool Complete(bool takeTitle, std::vector<std::string>& rows, std::string& title,
		size_t* leadingRowsRemoved = nullptr);
	size_t RowCount() const { return m_rows.size(); }

private:
	bool Add(std::string_view text, size_t indent = 0);
	bool FlushImage();
	void AdvanceImage();
	size_t m_columns, m_maxRows, m_maxBytes, m_bytes = 0;
	size_t m_imageInset = 0, m_imageRows = 0;
	bool m_valid, m_afterHeader = false;
	std::vector<std::string> m_rows;
};

} // namespace DescriptorText
