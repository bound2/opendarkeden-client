//----------------------------------------------------------------------
// test_wire_bool_char.cpp
//----------------------------------------------------------------------
//
// The bool and char overloads of the four socket streams - the plain
// read(bool&)/read(char&) and write(bool)/write(char), and the
// encrypting readEncrypt/writeEncrypt pair - at the byte level. They
// are the last stream scalars outside the typed readWire/writeWire
// layer: `char` is not a fixed-width integer type, and `bool` has a
// wire transform of its own in the encrypter (a flip for every code
// above 128, the identity below). What goes on the wire for a valid
// bool and for every char value, under every code, is pinned here
// before the overloads move off their casts.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "Socket.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "SocketImpl.h"

#include <vector>

namespace {

// The plain overloads are the base classes'; the encrypt streams
// inherit them unchanged, so one pair of streams covers all four.
struct BoolCharFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Input;
	SocketEncryptOutputStream	m_Output;

	explicit BoolCharFixture(uchar code)
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Input(&m_Socket, 64),
	  m_Output(&m_Socket, 64)
	{
		m_Input.setEncryptCode(code);
		m_Output.setEncryptCode(code);
	}

	std::vector<unsigned char> Bytes()
	{
		return SocketOutputStreamTestAccess::Bytes(m_Output);
	}

	void Preload(const std::vector<unsigned char>& bytes)
	{
		SocketInputStreamTestAccess::Preload(m_Input, bytes.empty() ? NULL : &bytes[0],
						     (unsigned int)bytes.size());
	}
};

// Codes below 129 leave a bool alone; 129 and above flip it. The char
// transform is an XOR with the code byte at every code.
const uchar	kCodes[] = { 0, 1, 2, 3, 4, 5, 128, 129, 200, 255 };
const size_t	kCodeCount = sizeof(kCodes) / sizeof(kCodes[0]);

} // namespace

//----------------------------------------------------------------------
// The plain overloads: one byte, the value itself
//----------------------------------------------------------------------
TEST(WireBoolChar, PlainBoolIsOneByteHoldingZeroOrOne)
{
	BoolCharFixture f(0);
	f.m_Output.write(true);
	f.m_Output.write(false);
	const std::vector<unsigned char> bytes = f.Bytes();
	CHECK_EQ((size_t)2, bytes.size());
	CHECK_EQ(0x01, bytes[0]);
	CHECK_EQ(0x00, bytes[1]);

	f.Preload(bytes);
	bool a = false, b = true;
	CHECK_EQ(1u, f.m_Input.read(a));
	CHECK_EQ(1u, f.m_Input.read(b));
	CHECK_EQ(true, a);
	CHECK_EQ(false, b);
	CHECK(f.m_Input.isEmpty());
}

TEST(WireBoolChar, PlainCharIsOneByteAtEveryValue)
{
	for (int v = 0; v < 256; v++)
	{
		BoolCharFixture f(0);
		f.m_Output.write((char)v);
		const std::vector<unsigned char> bytes = f.Bytes();
		CHECK_EQ((size_t)1, bytes.size());
		CHECK_EQ((unsigned char)v, bytes[0]);

		f.Preload(bytes);
		char c = (char)~v;
		CHECK_EQ(1u, f.m_Input.read(c));
		CHECK_EQ((char)v, c);
		CHECK(f.m_Input.isEmpty());
	}
}

// The plain overloads never consult the code: the same bytes and the
// same values under every code.
TEST(WireBoolChar, PlainOverloadsIgnoreTheEncryptCode)
{
	for (size_t i = 0; i < kCodeCount; i++)
	{
		BoolCharFixture f(kCodes[i]);
		f.m_Output.write(true);
		f.m_Output.write((char)0xA5);
		const std::vector<unsigned char> bytes = f.Bytes();
		CHECK_EQ((size_t)2, bytes.size());
		CHECK_EQ(0x01, bytes[0]);
		CHECK_EQ(0xA5, bytes[1]);

		f.Preload(bytes);
		bool b = false;
		char c = 0;
		f.m_Input.read(b);
		f.m_Input.read(c);
		CHECK_EQ(true, b);
		CHECK_EQ((char)0xA5, c);
	}
}

//----------------------------------------------------------------------
// The encrypt overloads: the transform, applied once each way
//----------------------------------------------------------------------
TEST(WireBoolChar, EncryptedBoolFlipsAboveCode128AndRoundTrips)
{
	for (size_t i = 0; i < kCodeCount; i++)
	{
		const bool flips = kCodes[i] > 128;
		BoolCharFixture f(kCodes[i]);
		f.m_Output.writeEncrypt(true);
		f.m_Output.writeEncrypt(false);
		const std::vector<unsigned char> bytes = f.Bytes();
		CHECK_EQ((size_t)2, bytes.size());
		CHECK_EQ(flips ? 0x00 : 0x01, bytes[0]);
		CHECK_EQ(flips ? 0x01 : 0x00, bytes[1]);

		f.Preload(bytes);
		bool a = false, b = true;
		f.m_Input.readEncrypt(a);
		f.m_Input.readEncrypt(b);
		CHECK_EQ(true, a);
		CHECK_EQ(false, b);
		CHECK(f.m_Input.isEmpty());
	}
}

TEST(WireBoolChar, EncryptedCharIsXorWithTheCodeAndRoundTrips)
{
	for (size_t i = 0; i < kCodeCount; i++)
	{
		for (int v = 0; v < 256; v += 17)
		{
			BoolCharFixture f(kCodes[i]);
			f.m_Output.writeEncrypt((char)v);
			const std::vector<unsigned char> bytes = f.Bytes();
			CHECK_EQ((size_t)1, bytes.size());
			CHECK_EQ((unsigned char)(v ^ kCodes[i]), bytes[0]);

			f.Preload(bytes);
			char c = 0;
			f.m_Input.readEncrypt(c);
			CHECK_EQ((char)v, c);
		}
	}
}

//----------------------------------------------------------------------
// An empty stream refuses both, whole
//----------------------------------------------------------------------
TEST(WireBoolChar, ReadingFromAnEmptyStreamUnderflows)
{
	BoolCharFixture f(0);
	bool b = true;
	char c = 'x';
	bool bThrew = false;
	try {
		f.m_Input.read(b);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);
	CHECK_EQ(true, b);

	bThrew = false;
	try {
		f.m_Input.read(c);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);
	CHECK_EQ('x', c);

	bThrew = false;
	try {
		f.m_Input.readEncrypt(b);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);
	CHECK_EQ(true, b);

	bThrew = false;
	try {
		f.m_Input.readEncrypt(c);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);
	CHECK_EQ('x', c);
}

//----------------------------------------------------------------------
// A wire byte that is neither 0 nor 1 becomes a well-formed bool
// (code-health review, Medium: "Packets read raw wire bytes directly
// into bool members, producing invalid bool representations")
//----------------------------------------------------------------------

// The old read(bool&) copied the byte into the bool's storage, so 0x02
// or 0xFF made a bool that was neither true nor false - undefined to
// branch on, and a trap under Clang's -fsanitize=bool. Every non-zero
// byte is true now, on the plain and the encrypting read, and the
// bytes 0 and 1 mean what they always meant.
TEST(WireBoolChar, ANonZeroWireByteReadsAsTrueOnBothReads)
{
	const unsigned char values[] = { 0x02, 0x7F, 0x80, 0xFF };

	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
	{
		BoolCharFixture f(0);
		std::vector<unsigned char> bytes;
		bytes.push_back(values[i]);
		bytes.push_back(0x00);
		f.Preload(bytes);
		bool a = false, b = true;
		f.m_Input.read(a);
		f.m_Input.read(b);
		// Compared as the storage byte, never loaded as a bool: an
		// invalid representation cannot pass by being "truthy", and the
		// test does not itself perform the load it exists to prevent (a
		// red run under Clang's -fsanitize=bool would abort on that
		// load rather than fail).
		CHECK_EQ(1, (int)*reinterpret_cast<const unsigned char*>(&a));
		CHECK_EQ(0, (int)*reinterpret_cast<const unsigned char*>(&b));
	}

	// The encrypting read, under a flipping and a non-flipping code: a
	// non-zero byte is true before the flip, so it flips to false
	// above 128, exactly as the byte 1 does.
	for (size_t i = 0; i < kCodeCount; i++)
	{
		const bool flips = kCodes[i] > 128;
		BoolCharFixture f(kCodes[i]);
		std::vector<unsigned char> bytes;
		bytes.push_back(0xFF);
		bytes.push_back(0x01);
		f.Preload(bytes);
		bool a = false, b = false;
		f.m_Input.readEncrypt(a);
		f.m_Input.readEncrypt(b);
		CHECK_EQ(flips ? 0 : 1, (int)*reinterpret_cast<const unsigned char*>(&a));
		CHECK_EQ(flips ? 0 : 1, (int)*reinterpret_cast<const unsigned char*>(&b));
	}
}
