// Script transport reads without a live peer and assert ring contents and limits.
#include "test_framework.h"
#include "packet_stream_access.h"
#include "SocketAPI.h"
#include "SocketImpl.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace {
using Bytes = std::vector<unsigned char>;
constexpr uint BufferLimit = 16 * 1024 * 1024;

class ReceiveSocket : public SocketImpl
{
public:
	Bytes incoming;
	uint next = 0;
	uint calls = 0;
	uint maxRead = (std::numeric_limits<uint>::max)();
	uint backlog = 0;
	bool throwWouldBlock = false;

	uint receive(void* buf, uint len, uint) override
	{
		++calls;
		if (len == 0)
			throw ConnectException("zero-length receive misreported as disconnect");
		if (next == incoming.size() && throwWouldBlock)
			throw NonBlockingIOException("scripted would-block");
		const uint count = (std::min)({len, maxRead, static_cast<uint>(incoming.size() - next)});
		if (count != 0)
			std::memcpy(buf, incoming.data() + next, count);
		next += count;
		return count;
	}

	uint available() const override
	{
		return backlog != 0 ? backlog : static_cast<uint>(incoming.size() - next);
	}
};

struct Fixture
{
	ReceiveSocket* impl;
	Socket socket;
	SocketInputStream input;
	explicit Fixture(uint capacity = 8)
		: impl((EnsureSocketsInitialised(), new ReceiveSocket)), socket(impl), input(&socket, capacity) {}
};

Bytes Pattern(uint count, unsigned char first = 0)
{
	Bytes bytes(count);
	for (uint i = 0; i < count; ++i)
		bytes[i] = static_cast<unsigned char>(first + i);
	return bytes;
}

void CheckBytes(SocketInputStream& input, const Bytes& expected)
{
	CHECK_EQ(expected.size(), static_cast<size_t>(input.length()));
	Bytes actual(expected.size());
	if (!actual.empty())
		CHECK(input.peek(reinterpret_cast<char*>(actual.data()), static_cast<uint>(actual.size())));
	CHECK(expected == actual);
}

template<class F> bool Rejects(F action)
{
	try { action(); }
	catch (Throwable&) { return true; }
	return false;
}
}

TEST(SocketInputFill, FullRingWithoutBacklogDoesNotReceive)
{
	for (uint head = 0; head < 8; ++head)
	{
		Fixture f;
		const Bytes before = Pattern(7);
		SocketInputStreamTestAccess::Preload(f.input, before.data(), 7, head);
		bool failed = false;
		try { CHECK_EQ(0u, f.input.fill()); }
		catch (Throwable&) { failed = true; }
		CHECK(!failed);
		CHECK_EQ(0u, f.impl->calls);
		CheckBytes(f.input, before);
	}
}

TEST(SocketInputFill, EveryRingPositionPreservesQueuedAndReceivedBytes)
{
	for (uint head = 0; head < 8; ++head)
		for (uint length = 0; length < 8; ++length)
			for (uint added = 0; added < 13; ++added)
			{
				Fixture f;
				Bytes expected = Pattern(length, 10);
				SocketInputStreamTestAccess::Preload(f.input, expected.data(), length, head);
				f.impl->incoming = Pattern(added, 100);
				expected.insert(expected.end(), f.impl->incoming.begin(), f.impl->incoming.end());
				bool failed = false;
				try { CHECK_EQ(added, f.input.fill()); }
				catch (Throwable&) { failed = true; }
				CHECK(!failed);
				CheckBytes(f.input, expected);
			}
}

TEST(SocketInputFill, PartialReadAndWouldBlockKeepProgress)
{
	Fixture f;
	f.impl->incoming = Pattern(6);
	f.impl->maxRead = 2;
	CHECK_EQ(2u, f.input.fill());
	CHECK_EQ(2u, f.input.fill());
	CHECK_EQ(2u, f.input.fill());
	CheckBytes(f.input, f.impl->incoming);
	f.impl->throwWouldBlock = true;
	CHECK_EQ(0u, f.input.fill());

	Fixture wrapped;
	SocketInputStreamTestAccess::Preload(wrapped.input, nullptr, 0, 6);
	wrapped.impl->incoming = Pattern(2);
	wrapped.impl->throwWouldBlock = true;
	CHECK_EQ(2u, wrapped.input.fill());
	CheckBytes(wrapped.input, wrapped.impl->incoming);
}

TEST(SocketInputFill, GrowthLimitAndInvalidResizePreserveTheRing)
{
	Fixture f;
	const Bytes before = Pattern(5);
	SocketInputStreamTestAccess::Preload(f.input, before.data(), 5, 6);
	CHECK(Rejects([&] { f.input.resize(-3); })); // Five bytes still need an empty sentinel slot.
	CheckBytes(f.input, before);
	CHECK(Rejects([&] { f.input.resize(static_cast<int>(BufferLimit)); }));
	CheckBytes(f.input, before);
}

TEST(SocketInputFill, DrainedLargeRingReturnsToDefaultCapacity)
{
	Fixture f(DefaultSocketInputBufferSize);
	f.impl->incoming = Pattern(DefaultSocketInputBufferSize * 3);
	CHECK_EQ(static_cast<uint>(f.impl->incoming.size()), f.input.fill());
	CHECK(f.input.capacity() > DefaultSocketInputBufferSize);
	f.input.skip(f.input.length());
	CHECK_EQ(0u, f.input.fill());
	CHECK_EQ(DefaultSocketInputBufferSize, f.input.capacity());
}

TEST(SocketInputFill, OutputResizeAlsoReservesTheSentinelSlot)
{
	Fixture f;
	SocketOutputStream output(&f.socket, 8);
	const Bytes expected = Pattern(5);
	output.write(reinterpret_cast<const char*>(expected.data()), 5);
	CHECK(Rejects([&] { output.resize(-3); }));
	CHECK_EQ(8, output.capacity());
	if (output.capacity() > static_cast<int>(expected.size()))
		CHECK(expected == SocketOutputStreamTestAccess::Bytes(output));
}

TEST(SocketInputFill, OversizedBacklogIsRejectedBeforeAnotherReceive)
{
	for (uint backlog : {BufferLimit, (std::numeric_limits<uint>::max)()})
	{
		Fixture f;
		const Bytes before = Pattern(7);
		SocketInputStreamTestAccess::Preload(f.input, before.data(), 7, 3);
		f.impl->backlog = backlog;
		CHECK(Rejects([&] { f.input.fill(); }));
		CHECK_EQ(8u, f.input.capacity());
		CHECK_EQ(0u, f.impl->calls);
		CheckBytes(f.input, before);
	}
}

TEST(SocketInputFill, ExactGrowthCeilingWorksAndDrainingReleasesStorage)
{
	Fixture f;
	Bytes expected = Pattern(7);
	SocketInputStreamTestAccess::Preload(f.input, expected.data(), 7, 3);
	f.impl->incoming = Pattern(BufferLimit - 8, 17);
	expected.insert(expected.end(), f.impl->incoming.begin(), f.impl->incoming.end());
	CHECK_EQ(BufferLimit - 8, f.input.fill());
	CHECK_EQ(BufferLimit, f.input.capacity());
	CheckBytes(f.input, expected);
	f.impl->incoming = Pattern(1, 99);
	f.impl->next = 0;
	CHECK(Rejects([&] { f.input.fill(); }));
	CHECK_EQ(0u, f.impl->next);
	CheckBytes(f.input, expected);
	f.input.skip(f.input.length());
	CHECK_EQ(1u, f.input.fill());
	CHECK_EQ(8u, f.input.capacity());
	CheckBytes(f.input, f.impl->incoming);
}

TEST(SocketInputFill, ResizeRejectsUnderflowAndPreservesWrappedBytes)
{
	Fixture f;
	const Bytes expected = Pattern(5);
	SocketInputStreamTestAccess::Preload(f.input, expected.data(), 5, 6);
	for (int delta : {-9, -8, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
	{
		CHECK(Rejects([&] { f.input.resize(delta); }));
		CHECK_EQ(8u, f.input.capacity());
		CheckBytes(f.input, expected);
	}
	f.input.resize(0);
	f.input.resize(-2);
	CHECK_EQ(6u, f.input.capacity());
	CheckBytes(f.input, expected);
	f.input.resize(10);
	CHECK_EQ(16u, f.input.capacity());
	CheckBytes(f.input, expected);

	SocketOutputStream output(&f.socket, 8);
	output.write(reinterpret_cast<const char*>(expected.data()), 5);
	for (int delta : {-9, -8, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
	{
		CHECK(Rejects([&] { output.resize(delta); }));
		CHECK_EQ(8, output.capacity());
		CHECK(expected == SocketOutputStreamTestAccess::Bytes(output));
	}
	output.resize(0);
	output.resize(-2);
	CHECK_EQ(6, output.capacity());
	CHECK(expected == SocketOutputStreamTestAccess::Bytes(output));
}

TEST(SocketInputFill, ConstructorsRejectInvalidCapacityAndEmptyReceiveIsANoop)
{
	EnsureSocketsInitialised();
	Socket socket;
	for (uint capacity : {0u, 1u, BufferLimit + 1, (std::numeric_limits<uint>::max)()})
		CHECK(Rejects([&] { SocketInputStream input(&socket, capacity); }));
	for (uint capacity : {0u, 1u, (std::numeric_limits<uint>::max)()})
		CHECK(Rejects([&] { SocketOutputStream output(&socket, capacity); }));
	// An invalid descriptor proves the zero-byte operation makes no system call.
	CHECK_EQ(0u, SocketAPI::recv_ex(INVALID_SOCKET, nullptr, 0, 0));
}
