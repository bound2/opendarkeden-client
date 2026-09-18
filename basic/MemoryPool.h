#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <unordered_map>
#include <vector>

// Single-threaded object storage. Larger objects and extended alignments use
// separately owned allocations. The pool must outlive its objects; destruction
// releases any remaining raw allocations.
class MemoryPool
{
public:
	MemoryPool(std::size_t blockSize, std::size_t blockCount);
	~MemoryPool();
	MemoryPool(const MemoryPool&) = delete;
	MemoryPool& operator=(const MemoryPool&) = delete;

	void* Alloc();
	void* Alloc(std::size_t size, std::size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__);
	// Null is a no-op. Other pointers must be an exact, live allocation from
	// this pool; a rejected free leaves all allocations unchanged.
	bool Free(void* memory) noexcept;
	bool IsPtrInPool(void* memory) const noexcept;
	bool IsAvailablePtr(void* memory) const noexcept;

private:
	struct Chunk;
	struct FreeSlot { FreeSlot* next; };
	Chunk* FindChunk(void* memory, std::size_t& index) const noexcept;

	std::size_t m_blockSize;
	std::size_t m_blockCount;
	std::size_t m_stride;
	std::vector<std::unique_ptr<Chunk>> m_chunks;
	std::unordered_map<void*, std::size_t> m_largeAllocations;
	FreeSlot* m_free = nullptr;
};
