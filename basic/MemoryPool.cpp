#include "MemoryPool.h"
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t DefaultAlignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

std::size_t RoundSize(std::size_t size, std::size_t alignment)
{
	if (size > (std::numeric_limits<std::size_t>::max)() - (alignment - 1))
		throw std::bad_alloc();
	return (size + alignment - 1) & ~(alignment - 1);
}

struct DeleteChunk
{
	void operator()(std::byte* memory) const noexcept
	{
		::operator delete(memory, std::align_val_t(DefaultAlignment));
	}
};

} // namespace

struct MemoryPool::Chunk
{
	Chunk(std::size_t bytes, std::size_t count)
		: memory(static_cast<std::byte*>(::operator new(bytes, std::align_val_t(DefaultAlignment)))),
		  live(count, false)
	{
	}

	std::unique_ptr<std::byte, DeleteChunk> memory;
	std::vector<bool> live;
	std::size_t issued = 0;
};

MemoryPool::MemoryPool(std::size_t blockSize, std::size_t blockCount)
	: m_blockSize(blockSize), m_blockCount(blockCount), m_stride(0)
{
	if (blockSize == 0 || blockCount == 0 ||
		blockCount > (std::numeric_limits<std::size_t>::max)() / blockSize)
		throw std::invalid_argument("Invalid memory pool dimensions");
	m_stride = RoundSize(blockSize < sizeof(FreeSlot) ? sizeof(FreeSlot) : blockSize, DefaultAlignment);
	if (blockCount > (std::numeric_limits<std::size_t>::max)() / m_stride)
		throw std::invalid_argument("Memory pool chunk size overflows");
}

MemoryPool::~MemoryPool()
{
	for (const auto& allocation : m_largeAllocations)
		::operator delete(allocation.first, std::align_val_t(allocation.second));
}

void* MemoryPool::Alloc()
{
	return Alloc(m_blockSize);
}

void* MemoryPool::Alloc(std::size_t size, std::size_t alignment)
{
	if (alignment == 0 || (alignment & (alignment - 1)) != 0)
		throw std::invalid_argument("Memory pool alignment must be a power of two");
	if (size == 0)
		size = 1;
	if (size > m_blockSize || alignment > DefaultAlignment)
	{
		if (alignment < DefaultAlignment)
			alignment = DefaultAlignment;
		void* memory = ::operator new(RoundSize(size, alignment), std::align_val_t(alignment));
		try
		{
			m_largeAllocations.emplace(memory, alignment);
		}
		catch (...)
		{
			::operator delete(memory, std::align_val_t(alignment));
			throw;
		}
		return memory;
	}

	if (m_free != nullptr)
	{
		FreeSlot* memory = m_free;
		m_free = memory->next;
		std::size_t index = 0;
		FindChunk(memory, index)->live[index] = true;
		return memory;
	}

	if (m_chunks.empty() || m_chunks.back()->issued == m_blockCount)
		m_chunks.push_back(std::make_unique<Chunk>(m_stride * m_blockCount, m_blockCount));
	Chunk& chunk = *m_chunks.back();
	const std::size_t index = chunk.issued++;
	chunk.live[index] = true;
	return chunk.memory.get() + index * m_stride;
}

MemoryPool::Chunk* MemoryPool::FindChunk(void* memory, std::size_t& index) const noexcept
{
	const auto address = reinterpret_cast<std::uintptr_t>(memory);
	for (const auto& chunk : m_chunks)
	{
		const auto begin = reinterpret_cast<std::uintptr_t>(chunk->memory.get());
		if (address < begin)
			continue;
		const auto offset = address - begin;
		if (offset < chunk->issued * m_stride && offset % m_stride == 0)
		{
			index = offset / m_stride;
			return chunk.get();
		}
	}
	return nullptr;
}

bool MemoryPool::Free(void* memory) noexcept
{
	if (memory == nullptr)
		return true;
	std::size_t index = 0;
	if (Chunk* chunk = FindChunk(memory, index))
	{
		if (!chunk->live[index])
			return false;
		chunk->live[index] = false;
		m_free = ::new (memory) FreeSlot{m_free};
		return true;
	}
	const auto found = m_largeAllocations.find(memory);
	if (found == m_largeAllocations.end())
		return false;
	const std::size_t alignment = found->second;
	m_largeAllocations.erase(found);
	::operator delete(memory, std::align_val_t(alignment));
	return true;
}

bool MemoryPool::IsPtrInPool(void* memory) const noexcept
{
	std::size_t index = 0;
	if (const Chunk* chunk = FindChunk(memory, index))
		return chunk->live[index];
	return m_largeAllocations.find(memory) != m_largeAllocations.end();
}

bool MemoryPool::IsAvailablePtr(void* memory) const noexcept
{
	return IsPtrInPool(memory);
}