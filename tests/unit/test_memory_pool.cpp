#include "MemoryPool.h"
#include "test_framework.h"
#include <cstring>

TEST(MemoryPool, KeepsLiveAllocationsDistinctAcrossChunksAndReuse)
{
	MemoryPool pool(32, 2);
	void* first = pool.Alloc();
	void* second = pool.Alloc();
	void* third = pool.Alloc();
	CHECK(first != second && first != third && second != third);
	std::memset(first, 0x12, 32);
	std::memset(second, 0x34, 32);
	std::memset(third, 0x56, 32);
	pool.Free(second);
	void* replacement = pool.Alloc();
	CHECK(replacement != first && replacement != third);
	CHECK_EQ(0x12, static_cast<unsigned char*>(first)[31]);
	CHECK_EQ(0x56, static_cast<unsigned char*>(third)[31]);
	pool.Free(first);
	pool.Free(third);
	pool.Free(replacement);
}
