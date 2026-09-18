#include "MemoryPool.h"
#include "test_framework.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

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

TEST(MemoryPool, AlignsEveryOddSizedAllocation)
{
	MemoryPool pool(17, 3);
	for (int i = 0; i < 7; ++i)
	{
		void* memory = pool.Alloc();
		CHECK_EQ(0, reinterpret_cast<std::uintptr_t>(memory) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);
		std::memset(memory, i, 17);
	}
}

TEST(MemoryPool, FreeingOneSlotDoesNotInvalidateItsNeighbor)
{
	MemoryPool pool(32, 3);
	void* first = pool.Alloc();
	void* second = pool.Alloc();
	pool.Free(first);
	CHECK(!pool.IsAvailablePtr(first));
	CHECK(pool.IsAvailablePtr(second));
	CHECK(!pool.IsAvailablePtr(static_cast<unsigned char*>(second) + 1));
	pool.Free(second);
}

TEST(MemoryPool, RepeatedFreeCannotHandOutOneSlotTwice)
{
	MemoryPool pool(32, 2);
	void* first = pool.Alloc();
	pool.Free(first);
	pool.Free(first);
	void* second = pool.Alloc();
	void* third = pool.Alloc();
	CHECK(second != third);
}

TEST(MemoryPool, HonorsRequestedSizesLargerThanTheNominalBlock)
{
	MemoryPool pool(32, 2);
	const std::size_t sizes[] = {1, 17, 32, 33, 96, 4096, 65537};
	std::vector<unsigned char*> allocations;
	for (std::size_t size : sizes)
	{
		auto* memory = static_cast<unsigned char*>(pool.Alloc(size));
		std::memset(memory, static_cast<int>(allocations.size() + 1), size);
		allocations.push_back(memory);
	}
	for (std::size_t i = 0; i < allocations.size(); ++i)
	{
		bool intact = true;
		for (std::size_t j = 0; j < sizes[i]; ++j)
			intact = intact && allocations[i][j] == i + 1;
		CHECK(intact);
		CHECK(pool.IsAvailablePtr(allocations[i]));
		CHECK(pool.Free(allocations[i]));
	}
}

TEST(MemoryPool, HonorsExtendedAlignmentAcrossSizesAndReuse)
{
	MemoryPool pool(17, 3);
	for (std::size_t alignment : {16u, 32u, 64u, 256u, 4096u})
	{
		for (std::size_t size : {1u, 17u, 64u, 4097u})
		{
			for (int repeat = 0; repeat < 3; ++repeat)
			{
				void* memory = pool.Alloc(size, alignment);
				CHECK_EQ(0, reinterpret_cast<std::uintptr_t>(memory) % alignment);
				std::memset(memory, 0x73, size);
				CHECK(pool.Free(memory));
			}
		}
	}
}

TEST(MemoryPool, RejectsForeignInteriorAndAlreadyReleasedPointers)
{
	MemoryPool pool(32, 2);
	MemoryPool other(32, 2);
	void* memory = pool.Alloc();
	void* foreign = other.Alloc();
	int stack = 0;
	CHECK(pool.Free(nullptr));
	CHECK(!pool.Free(&stack));
	CHECK(!pool.Free(foreign));
	CHECK(!pool.Free(static_cast<unsigned char*>(memory) + 1));
	CHECK(pool.IsPtrInPool(memory));
	CHECK(pool.IsAvailablePtr(memory));
	CHECK(other.IsAvailablePtr(foreign));
	CHECK(pool.Free(memory));
	CHECK(!pool.Free(memory));
	CHECK(!pool.IsPtrInPool(memory));
	CHECK(!pool.IsAvailablePtr(memory));
	CHECK(other.Free(foreign));
}

TEST(MemoryPool, RejectsInvalidConfigurationAndAlignment)
{
	const std::size_t maximum = (std::numeric_limits<std::size_t>::max)();
	for (auto options : {std::pair<std::size_t, std::size_t>{0, 1}, {1, 0}, {maximum, 2}})
	{
		bool rejected = false;
		try { MemoryPool pool(options.first, options.second); }
		catch (const std::invalid_argument&) { rejected = true; }
		CHECK(rejected);
	}
	MemoryPool pool(32, 2);
	for (std::size_t alignment : {0u, 3u, 6u})
	{
		bool rejected = false;
		try { pool.Alloc(32, alignment); }
		catch (const std::invalid_argument&) { rejected = true; }
		CHECK(rejected);
	}
	void* first = pool.Alloc(0);
	void* second = pool.Alloc(0);
	CHECK(first != second);
	CHECK(pool.Free(first));
	CHECK(pool.Free(second));
}

TEST(MemoryPool, RejectsSizeRoundingOverflowWithoutLosingLiveObjects)
{
	MemoryPool pool(32, 2);
	auto* memory = static_cast<unsigned char*>(pool.Alloc());
	std::memset(memory, 0x7A, 32);
	bool rejected = false;
	try { pool.Alloc((std::numeric_limits<std::size_t>::max)()); }
	catch (const std::bad_alloc&) { rejected = true; }
	CHECK(rejected);
	CHECK_EQ(0x7A, memory[31]);
	CHECK(pool.IsAvailablePtr(memory));
	CHECK(pool.Free(memory));
}

TEST(MemoryPool, ReusesSmallSlotsWithoutDisturbingLargeLiveAllocations)
{
	MemoryPool pool(3, 2);
	auto* large = static_cast<unsigned char*>(pool.Alloc(1001, 128));
	std::memset(large, 0x61, 1001);
	CHECK(!pool.Free(large + 1));
	for (int round = 0; round < 50; ++round)
	{
		std::vector<unsigned char*> small;
		for (int i = 0; i < 7; ++i)
		{
			auto* memory = static_cast<unsigned char*>(pool.Alloc());
			std::memset(memory, i + 1, 3);
			small.push_back(memory);
		}
		for (std::size_t i = 0; i < small.size(); ++i)
		{
			CHECK_EQ(i + 1, small[i][0]);
			CHECK_EQ(i + 1, small[i][2]);
			CHECK(pool.Free(small[i]));
			CHECK(!pool.Free(small[i]));
		}
	}
	CHECK_EQ(0x61, large[0]);
	CHECK_EQ(0x61, large[1000]);
	CHECK(pool.Free(large));
	CHECK(!pool.Free(large));
}
