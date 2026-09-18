//----------------------------------------------------------------------
// test_tarray.cpp
//----------------------------------------------------------------------
//
// Tests for the shared TArray template in basic/TArray.h.
//
// TArray owns a raw DataType* and frees it in its destructor, so it needs
// copy semantics that give each instance its own storage. These tests
// cover that ownership contract.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "basic/TArray.h"
#include "Client/framelib/TArray.h"
#include "Client/SpriteLib/TArray.h"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace {

//----------------------------------------------------------------------
// Smallest element type that satisfies TArray's file I/O requirement.
//----------------------------------------------------------------------
struct TestElement
{
	int	value;

	TestElement() : value(0) {}

	bool	SaveToFile(std::ofstream& file)
	{
		file.write((const char*)&value, sizeof(value));
		return true;
	}

	bool	LoadFromFile(std::ifstream& file)
	{
		file.read((char*)&value, sizeof(value));
		return true;
	}
};

const char* const	kTempFile = "tarray_loadfromfile_test.bin";

void	WriteRawFile(const void* data, size_t bytes)
{
	std::ofstream	out(kTempFile, std::ios::binary | std::ios::trunc);

	if (bytes > 0)
		out.write((const char*)data, (std::streamsize)bytes);
}

void	RemoveTempFile()
{
	std::remove(kTempFile);
}

} // namespace

//----------------------------------------------------------------------
// Copy construction must deep copy.
//
// TArray declares a destructor that runs delete[] on its buffer. Without
// a matching copy constructor the compiler supplies a member-wise one,
// which hands both instances the same pointer: mutating one is visible
// through the other, and the second destructor frees memory the first
// already released.
//----------------------------------------------------------------------
TEST(TArray, CopyConstructionProducesIndependentStorage)
{
	TArray<int, WORD>	original(3);

	original[0] = 10;
	original[1] = 20;
	original[2] = 30;

	TArray<int, WORD>	copy(original);

	CHECK_EQ(3, copy.GetSize());
	CHECK_EQ(10, copy[0]);
	CHECK_EQ(20, copy[1]);
	CHECK_EQ(30, copy[2]);

	// Writing through the copy must not disturb the original.
	copy[0] = 999;

	CHECK_EQ(10, original[0]);
	CHECK_EQ(999, copy[0]);

	// Both instances are destroyed here. With shared storage the second
	// destructor frees an already released buffer.
}

//----------------------------------------------------------------------
// Copying an empty array must not allocate or fault.
//----------------------------------------------------------------------
TEST(TArray, CopyConstructionHandlesEmptyArray)
{
	TArray<int, WORD>	original;

	CHECK_EQ(0, original.GetSize());

	TArray<int, WORD>	copy(original);

	CHECK_EQ(0, copy.GetSize());
}

//----------------------------------------------------------------------
// Appending within the range of SizeType concatenates both arrays.
//----------------------------------------------------------------------
TEST(TArray, AppendConcatenatesElements)
{
	TArray<int, BYTE>	target(3);

	target[0] = 1;
	target[1] = 2;
	target[2] = 3;

	TArray<int, BYTE>	source(2);

	source[0] = 4;
	source[1] = 5;

	target += source;

	CHECK_EQ(5, target.GetSize());
	CHECK_EQ(1, target[0]);
	CHECK_EQ(2, target[1]);
	CHECK_EQ(3, target[2]);
	CHECK_EQ(4, target[3]);
	CHECK_EQ(5, target[4]);

	// The source must be left untouched.
	CHECK_EQ(2, source.GetSize());
	CHECK_EQ(4, source[0]);
}

//----------------------------------------------------------------------
// Appending must never write more elements than it allocated.
//
// operator+= sized its new buffer with "SizeType newSize = m_Size +
// array.m_Size". For a narrow SizeType the sum is computed as an int and
// then truncated on assignment, so 200 + 100 allocates 44 elements while
// the copy loops still write all 300, running 256 elements past the end
// of the allocation.
//
// 300 cannot be represented in a BYTE at all, so the only safe outcome
// is to refuse the append and leave the target as it was.
//----------------------------------------------------------------------
TEST(TArray, AppendRefusesWhenCombinedSizeOverflowsSizeType)
{
	TArray<int, BYTE>	target(200);

	for (int i = 0; i < 200; i++)
		target[(BYTE)i] = i;

	TArray<int, BYTE>	source(100);

	for (int i = 0; i < 100; i++)
		source[(BYTE)i] = 1000 + i;

	target += source;

	CHECK_EQ(200, target.GetSize());

	// The original contents must still be intact.
	CHECK_EQ(0, target[0]);
	CHECK_EQ(100, target[100]);
	CHECK_EQ(199, target[(BYTE)199]);
}

//----------------------------------------------------------------------
// The boundary case that still fits: 255 elements in a BYTE.
//----------------------------------------------------------------------
TEST(TArray, AppendAcceptsLargestRepresentableSize)
{
	TArray<int, BYTE>	target(200);

	for (int i = 0; i < 200; i++)
		target[(BYTE)i] = i;

	TArray<int, BYTE>	source(55);

	for (int i = 0; i < 55; i++)
		source[(BYTE)i] = 1000 + i;

	target += source;

	CHECK_EQ(255, target.GetSize());
	CHECK_EQ(0, target[0]);
	CHECK_EQ(199, target[(BYTE)199]);
	CHECK_EQ(1000, target[(BYTE)200]);
	CHECK_EQ(1054, target[(BYTE)254]);
}

//----------------------------------------------------------------------
// Assigning an array to itself must leave it alone.
//
// operator= starts with Init(array.m_Size), and Init releases the
// current buffer before allocating. When the source and the destination
// are the same object that release also destroys the source, so the copy
// loop that follows reads the freshly allocated, uninitialised memory
// and writes it back over itself. The contents are lost without any
// diagnostic.
//----------------------------------------------------------------------
TEST(TArray, SelfAssignmentPreservesContents)
{
	TArray<int, WORD>	array(3);

	array[0] = 7;
	array[1] = 8;
	array[2] = 9;

	// Routed through a reference so this reads as an ordinary assignment
	// to the compiler rather than a self-assignment it may warn about.
	const TArray<int, WORD>&	alias = array;

	array = alias;

	CHECK_EQ(3, array.GetSize());
	CHECK_EQ(7, array[0]);
	CHECK_EQ(8, array[1]);
	CHECK_EQ(9, array[2]);
}

//----------------------------------------------------------------------
// Ordinary assignment deep copies, so the two arrays stay independent.
//----------------------------------------------------------------------
TEST(TArray, AssignmentProducesIndependentStorage)
{
	TArray<int, WORD>	source(2);

	source[0] = 100;
	source[1] = 200;

	TArray<int, WORD>	destination(5);

	destination = source;

	CHECK_EQ(2, destination.GetSize());
	CHECK_EQ(100, destination[0]);
	CHECK_EQ(200, destination[1]);

	destination[0] = 555;

	CHECK_EQ(100, source[0]);
}

//----------------------------------------------------------------------
// A well formed file round trips.
//----------------------------------------------------------------------
TEST(TArray, LoadFromFileRoundTripsSavedContents)
{
	{
		TArray<TestElement, WORD>	source(3);

		source[0].value = 11;
		source[1].value = 22;
		source[2].value = 33;

		std::ofstream	out(kTempFile, std::ios::binary | std::ios::trunc);

		source.SaveToFile(out);
	}

	TArray<TestElement, WORD>	loaded;
	std::ifstream			in(kTempFile, std::ios::binary);

	CHECK(loaded.LoadFromFile(in));
	CHECK_EQ(3, loaded.GetSize());
	CHECK_EQ(11, loaded[0].value);
	CHECK_EQ(22, loaded[1].value);
	CHECK_EQ(33, loaded[2].value);

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A count the file cannot possibly hold must be rejected.
//
// The count is read straight out of the file and used as the allocation
// size with nothing checking it against the data that actually follows.
// A file claiming 1000 elements while carrying two allocates room for
// 1000, reads past the end of the data for the rest, and still reports
// success. Every element occupies at least one byte, so a count larger
// than the bytes remaining is corrupt by definition.
//----------------------------------------------------------------------
TEST(TArray, LoadFromFileRejectsCountLargerThanTheFile)
{
	unsigned char	buffer[2 + 8];
	const WORD	declaredCount	= 1000;
	const int	first		= 1;
	const int	second		= 2;

	std::memcpy(buffer, &declaredCount, sizeof(declaredCount));
	std::memcpy(buffer + 2, &first, sizeof(first));
	std::memcpy(buffer + 6, &second, sizeof(second));

	WriteRawFile(buffer, sizeof(buffer));

	TArray<TestElement, WORD>	loaded;
	std::ifstream			in(kTempFile, std::ios::binary);

	CHECK(!loaded.LoadFromFile(in));
	CHECK_EQ(0, loaded.GetSize());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A failed read must not fall back to the size the array already had.
//
// The count was read directly into m_Size, so when the read failed the
// member kept its previous value and the load continued with it,
// allocating that many elements and filling them from a dead stream
// before returning success.
//----------------------------------------------------------------------
TEST(TArray, LoadFromFileRejectsEmptyFileWithoutReusingPreviousSize)
{
	WriteRawFile(NULL, 0);

	TArray<TestElement, WORD>	loaded(5);

	loaded[0].value = 77;

	std::ifstream	in(kTempFile, std::ios::binary);

	CHECK(!loaded.LoadFromFile(in));
	CHECK_EQ(0, loaded.GetSize());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// A file holding only part of the count is a short read, not a count.
//----------------------------------------------------------------------
TEST(TArray, LoadFromFileRejectsTruncatedCount)
{
	const unsigned char	singleByte = 0x05;

	WriteRawFile(&singleByte, 1);

	TArray<TestElement, WORD>	loaded;
	std::ifstream			in(kTempFile, std::ios::binary);

	CHECK(!loaded.LoadFromFile(in));
	CHECK_EQ(0, loaded.GetSize());

	RemoveTempFile();
}

//----------------------------------------------------------------------
// The copy must survive the original being destroyed first, which is the
// case that turns shared ownership into a dangling read.
//----------------------------------------------------------------------
TEST(TArray, CopyOutlivesOriginal)
{
	TArray<int, WORD>	copy;

	{
		TArray<int, WORD>	original(2);

		original[0] = 41;
		original[1] = 42;

		TArray<int, WORD>	temporary(original);

		copy = temporary;
	}

	CHECK_EQ(2, copy.GetSize());
	CHECK_EQ(41, copy[0]);
	CHECK_EQ(42, copy[1]);
}
