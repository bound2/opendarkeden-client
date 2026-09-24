#pragma once

#include <optional>
#include <string>

// An absent cell differs from an explicitly empty string. Replacement owns its
// input before releasing the previous value, including self/interior aliases.
class StringCell
{
	std::optional<std::string> m_string;

public:
	virtual ~StringCell() = default;
	void Release() { m_string.reset(); }
	void SetString(const char* text)
	{
		if (text) m_string = std::string(text);
	}
	const char* GetString() const { return m_string ? m_string->c_str() : nullptr; }
};
