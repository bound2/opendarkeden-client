#include "Client_PCH.h"
#include "CFileIndexTable.h"
#include <cstdint>
#include <limits>

bool CFileIndexTable::LoadFromFile(std::ifstream& indexFile)
{
	try {
		unsigned char header[2];
		if (!indexFile.read(reinterpret_cast<char*>(header), sizeof(header))) return false;
		const unsigned count = unsigned(header[0]) | (unsigned(header[1]) << 8);
		std::vector<long> pending(count);
		bool valid = true;
		for (auto& offset : pending) {
			unsigned char bytes[4];
			if (!indexFile.read(reinterpret_cast<char*>(bytes), sizeof(bytes))) return false;
			const std::uint32_t value = std::uint32_t(bytes[0]) |
				(std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16) |
				(std::uint32_t(bytes[3]) << 24);
			if (value < 2 || value > std::uint32_t((std::numeric_limits<std::int32_t>::max)()))
				valid = false;
			else
				offset = static_cast<long>(value);
		}
		if (!valid) return false;
		m_Index.swap(pending);
		return true;
	} catch (...) {
		return false;
	}
}

bool CFileIndexTable::TryGetOffset(std::size_t id, long& offset) const noexcept
{
	if (id >= m_Index.size()) return false;
	offset = m_Index[id];
	return true;
}


