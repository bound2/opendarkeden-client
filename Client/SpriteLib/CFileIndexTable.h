// Checked reader for a pack's 16-bit count and signed 32-bit file offsets.
#ifndef __CFILEINDEXTABLE_H__
#define __CFILEINDEXTABLE_H__

#include "../../basic/Platform.h"
#include <cstddef>
#include <fstream>
#include <vector>

class CFileIndexTable {
public:
	// Failed reloads preserve the previous table. Offset order and duplicates
	// are unrestricted; each offset must follow the pack's two-byte header.
	bool LoadFromFile(std::ifstream& indexFile);

	WORD GetSize() const { return static_cast<WORD>(m_Index.size()); }
	bool TryGetOffset(std::size_t id, long& offset) const noexcept;
	const long& operator[](std::size_t id) const { return m_Index.at(id); }
	void Release() { std::vector<long>().swap(m_Index); }

private:
	std::vector<long> m_Index;
};

#endif
