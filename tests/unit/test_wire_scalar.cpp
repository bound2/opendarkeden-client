//----------------------------------------------------------------------
// test_wire_scalar.cpp
//----------------------------------------------------------------------
//
// C++20 typed scalar serialization at the packet stream boundary.
// Supported fixed-width integers and scoped, exact-width-underlying-type enums
// retain the legacy little-endian bytes; ambiguous and non-scalar types must
// be rejected at compile time.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Socket.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "SocketImpl.h"
#include "WireScalar.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

enum class TestWireCode : std::uint16_t
{
	Value = 0xA1B2
};

enum ImplicitWireCode
{
	ImplicitZero,
	ImplicitOne
};

template <typename T>
concept StreamWireReadable = requires(SocketInputStream& stream, T& value)
{
	stream.readWire(value);
};

static_assert(packetwire::WireScalar<std::int8_t>);
static_assert(packetwire::WireScalar<std::uint8_t>);
static_assert(packetwire::WireScalar<std::int16_t>);
static_assert(packetwire::WireScalar<std::uint16_t>);
static_assert(packetwire::WireScalar<std::int32_t>);
static_assert(packetwire::WireScalar<std::uint32_t>);
static_assert(packetwire::WireScalar<std::int64_t>);
static_assert(packetwire::WireScalar<std::uint64_t>);
static_assert(packetwire::WireScalar<TestWireCode>);
static_assert(StreamWireReadable<std::uint32_t>);

static_assert(!packetwire::WireScalar<bool>);
static_assert(!packetwire::WireScalar<char>);
static_assert(!packetwire::WireScalar<ImplicitWireCode>);
static_assert(!packetwire::WireScalar<float>);
static_assert(!packetwire::WireScalar<double>);
static_assert(!packetwire::WireScalar<std::string>);
static_assert(!packetwire::WireScalar<void*>);
static_assert(!StreamWireReadable<const std::uint32_t>);

struct WireScalarFixture
{
	Socket			m_Socket;
	SocketInputStream	m_Input;
	SocketOutputStream	m_Output;

	WireScalarFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Input(&m_Socket, 32),
	  m_Output(&m_Socket, 32)
	{
	}
};

struct EncryptedWireScalarFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Input;
	SocketEncryptOutputStream	m_Output;

	EncryptedWireScalarFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Input(&m_Socket, 64),
	  m_Output(&m_Socket, 64)
	{
	}
};

template <typename T>
void CheckSignedWireValues(const std::array<T, 3>& values,
			   const std::array<unsigned char, sizeof(T) * 3>& expected)
{
	WireScalarFixture f;
	for (T value : values)
		f.m_Output.writeWire(value);

	const std::vector<unsigned char> actual =
		SocketOutputStreamTestAccess::Bytes(f.m_Output);
	CHECK_EQ(expected.size(), actual.size());
	if (actual.size() == expected.size())
	{
		for (std::size_t i = 0; i < expected.size(); i++)
			CHECK_EQ(expected[i], actual[i]);
	}

	SocketInputStreamTestAccess::Preload(f.m_Input, expected.data(),
		static_cast<unsigned int>(expected.size()));
	for (T expectedValue : values)
	{
		T actualValue = 0;
		f.m_Input.readWire(actualValue);
		CHECK_EQ(expectedValue, actualValue);
	}
	CHECK(f.m_Input.isEmpty());
}

void CheckHighBitEncryptionCode(uchar code,
				const std::array<unsigned char, 31>& expected)
{
	EncryptedWireScalarFixture f;
	const std::array<short, 3> shortValues = {
		(std::numeric_limits<short>::min)(), -1,
		(std::numeric_limits<short>::max)()
	};
	const std::array<int, 3> intValues = {
		(std::numeric_limits<int>::min)(), -1,
		(std::numeric_limits<int>::max)()
	};
	// The wire long is 32 bits whatever sizeof(long) is (LP64 makes it
	// 64), so the boundaries are int32_t's, not long's.
	const std::array<long, 3> longValues = {
		static_cast<long>((std::numeric_limits<std::int32_t>::min)()), -1,
		static_cast<long>((std::numeric_limits<std::int32_t>::max)())
	};

	f.m_Output.setEncryptCode(code);
	f.m_Output.writeEncrypt(static_cast<uchar>(0));
	for (short value : shortValues)
		f.m_Output.writeEncrypt(value);
	for (int value : intValues)
		f.m_Output.writeEncrypt(value);
	for (long value : longValues)
		f.m_Output.writeEncrypt(value);

	const std::vector<unsigned char> actual =
		SocketOutputStreamTestAccess::Bytes(f.m_Output);
	CHECK_EQ(expected.size(), actual.size());
	if (actual.size() == expected.size())
	{
		for (std::size_t i = 0; i < expected.size(); i++)
			CHECK_EQ(expected[i], actual[i]);
	}

	SocketInputStreamTestAccess::Preload(f.m_Input, expected.data(),
		static_cast<unsigned int>(expected.size()));
	f.m_Input.setEncryptCode(code);
	uchar byteValue = 1;
	f.m_Input.readEncrypt(byteValue);
	CHECK_EQ(0, byteValue);
	for (short expectedValue : shortValues)
	{
		short actualValue = 0;
		f.m_Input.readEncrypt(actualValue);
		CHECK_EQ(expectedValue, actualValue);
	}
	for (int expectedValue : intValues)
	{
		int actualValue = 0;
		f.m_Input.readEncrypt(actualValue);
		CHECK_EQ(expectedValue, actualValue);
	}
	for (long expectedValue : longValues)
	{
		long actualValue = 0;
		f.m_Input.readEncrypt(actualValue);
		CHECK_EQ(expectedValue, actualValue);
	}
	CHECK(f.m_Input.isEmpty());
}

} // namespace

TEST(WireScalar, WritesExactLittleEndianBytes)
{
	WireScalarFixture f;
	f.m_Output.writeWire(std::uint8_t{0x81});
	f.m_Output.writeWire(std::uint16_t{0xA2B3});
	f.m_Output.writeWire(std::uint32_t{0xC4D5E6F7});
	f.m_Output.writeWire(std::uint64_t{0x8899AABBCCDDEEFFULL});
	f.m_Output.writeWire(TestWireCode::Value);

	const std::vector<unsigned char> actual =
		SocketOutputStreamTestAccess::Bytes(f.m_Output);
	const unsigned char expected[] = {
		0x81,
		0xB3, 0xA2,
		0xF7, 0xE6, 0xD5, 0xC4,
		0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
		0xB2, 0xA1
	};

	CHECK_EQ(sizeof(expected), actual.size());
	if (actual.size() == sizeof(expected))
	{
		for (size_t i = 0; i < sizeof(expected); i++)
			CHECK_EQ(expected[i], actual[i]);
	}
}

TEST(WireScalar, ReadsFixedWidthAndEnumValues)
{
	WireScalarFixture f;
	const unsigned char bytes[] = {
		0x81,
		0xB3, 0xA2,
		0xF7, 0xE6, 0xD5, 0xC4,
		0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
		0xB2, 0xA1
	};
	SocketInputStreamTestAccess::Preload(f.m_Input, bytes, sizeof(bytes));

	std::uint8_t byteValue = 0;
	std::uint16_t shortValue = 0;
	std::uint32_t intValue = 0;
	std::uint64_t longValue = 0;
	TestWireCode enumValue = static_cast<TestWireCode>(0);
	f.m_Input.readWire(byteValue);
	f.m_Input.readWire(shortValue);
	f.m_Input.readWire(intValue);
	f.m_Input.readWire(longValue);
	f.m_Input.readWire(enumValue);

	CHECK_EQ(0x81, byteValue);
	CHECK_EQ(0xA2B3, shortValue);
	CHECK_EQ(0xC4D5E6F7, intValue);
	CHECK_EQ(0x8899AABBCCDDEEFFULL, longValue);
	CHECK(enumValue == TestWireCode::Value);
	CHECK(f.m_Input.isEmpty());
}

TEST(WireScalar, SignedBoundariesHaveExactBytesAndRoundTrip)
{
	CheckSignedWireValues<std::int8_t>(
		{ (std::numeric_limits<std::int8_t>::min)(), -1,
		  (std::numeric_limits<std::int8_t>::max)() },
		{ 0x80, 0xFF, 0x7F });
	CheckSignedWireValues<std::int16_t>(
		{ (std::numeric_limits<std::int16_t>::min)(), -1,
		  (std::numeric_limits<std::int16_t>::max)() },
		{ 0x00, 0x80, 0xFF, 0xFF, 0xFF, 0x7F });
	CheckSignedWireValues<std::int32_t>(
		{ (std::numeric_limits<std::int32_t>::min)(), -1,
		  (std::numeric_limits<std::int32_t>::max)() },
		{ 0x00, 0x00, 0x00, 0x80,
		  0xFF, 0xFF, 0xFF, 0xFF,
		  0xFF, 0xFF, 0xFF, 0x7F });
	CheckSignedWireValues<std::int64_t>(
		{ (std::numeric_limits<std::int64_t>::min)(), -1,
		  (std::numeric_limits<std::int64_t>::max)() },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
		  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F });
}

TEST(WireScalar, EncryptedAdaptersHandleHighBitCodes)
{
	CheckHighBitEncryptionCode(0x80,
		{ 0x80,
		  0x80, 0x80, 0x7F, 0xFF, 0x7F, 0x7F,
		  0x80, 0x00, 0x00, 0x80,
		  0x7F, 0xFF, 0xFF, 0xFF,
		  0x7F, 0xFF, 0xFF, 0x7F,
		  0x80, 0x00, 0x00, 0x80,
		  0x7F, 0xFF, 0xFF, 0xFF,
		  0x7F, 0xFF, 0xFF, 0x7F });
	CheckHighBitEncryptionCode(0xFF,
		{ 0xFF,
		  0xFF, 0x80, 0x00, 0xFF, 0x00, 0x7F,
		  0xFF, 0x00, 0x00, 0x80,
		  0x00, 0xFF, 0xFF, 0xFF,
		  0x00, 0xFF, 0xFF, 0x7F,
		  0xFF, 0x00, 0x00, 0x80,
		  0x00, 0xFF, 0xFF, 0xFF,
		  0x00, 0xFF, 0xFF, 0x7F });
}

TEST(WireScalar, UnderflowLeavesDestinationUnchanged)
{
	WireScalarFixture f;
	const unsigned char bytes[] = { 0x11, 0x22 };
	SocketInputStreamTestAccess::Preload(f.m_Input, bytes, sizeof(bytes));

	std::uint32_t value = 0xAABBCCDD;
	bool bThrew = false;
	try {
		f.m_Input.readWire(value);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}

	CHECK(bThrew);
	CHECK_EQ(0xAABBCCDD, value);
	CHECK_EQ(2, f.m_Input.length());
}
