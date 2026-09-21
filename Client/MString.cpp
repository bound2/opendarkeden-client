//--------------------------------------------------------------------------
// MString.cpp
//--------------------------------------------------------------------------

#include "Client_PCH.h"
#include <stdarg.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include "MString.h"
#include "TextEncoding.h"

//#include "DebugInfo.h"
//#define	new			DEBUG_NEW
//#define	delete		DEBUG_DELETE

//--------------------------------------------------------------------------
//
// constructor / destructor
//
//--------------------------------------------------------------------------
MString::MString()
{
	m_Length = 0;
	m_pString = NULL;
}

MString::MString(const MString& str)
{
	m_Length = 0;
	m_pString = NULL;
	*this = str;
}

MString::MString(const char* str)
{
	m_Length = 0;
	m_pString = NULL;
	*this = str;
}

MString::~MString()
{
	if (m_pString!=NULL)
	{
		delete [] m_pString;		
	}		
}

//--------------------------------------------------------------------------
//
// member functions
//
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
// Init( len )
//--------------------------------------------------------------------------
// Allocate writable storage and leave the string empty.
//--------------------------------------------------------------------------
void	
MString::Init(int len)
{
	if (len < 0)
		throw std::invalid_argument("MString capacity must not be negative");
	std::unique_ptr<char[]> replacement(new char[static_cast<size_t>(len) + 1]);
	replacement[0] = '\0';
	Release();
	m_pString = replacement.release();
}

//--------------------------------------------------------------------------
// Release
//--------------------------------------------------------------------------
// Release owned storage.
//--------------------------------------------------------------------------
void	
MString::Release()
{
	if (m_pString!=NULL)
	{
		delete [] m_pString;
		m_pString = NULL;
		m_Length = 0;
	}
}

//--------------------------------------------------------------------------
// Assign operator =
//--------------------------------------------------------------------------
void	
MString::operator = (const char* str)
{
	// The source may be our own buffer, including a suffix of it. Copy it
	// before releasing the old allocation; allocation failure preserves it.
	const size_t length = str != nullptr ? strlen(str) : 0;
	std::unique_ptr<char[]> replacement;
	if (length != 0)
	{
		replacement.reset(new char[length + 1]);
		memcpy(replacement.get(), str, length + 1);
	}
	delete [] m_pString;
	m_pString = replacement.release();
	m_Length = length;
}

//--------------------------------------------------------------------------
// Assign operator =
//--------------------------------------------------------------------------
void
MString::operator = (const MString& str)
{
	if (this == &str)
		return;
	std::unique_ptr<char[]> replacement;
	if (str.m_Length != 0)
	{
		replacement.reset(new char[str.m_Length + 1]);
		memcpy(replacement.get(), str.m_pString, str.m_Length + 1);
	}
	delete [] m_pString;
	m_pString = replacement.release();
	m_Length = str.m_Length;
}

//--------------------------------------------------------------------------
// Format
//--------------------------------------------------------------------------
// Build the string from a printf style format.
//--------------------------------------------------------------------------
void
MString::Format(const char* format, ...)
{
	// The scratch buffer is function local: a process wide static was shared by
	// every MString, so two Format calls interleaved through a logging or
	// drawing path overwrote each other's result.
	char		Buffer[MAX_BUFFER_LENGTH];
	va_list		vl;

	va_start(vl, format);
	int written = vsnprintf(Buffer, sizeof(Buffer), format, vl);
	va_end(vl);

	// vsnprintf returns the length the output *would* have had and NUL
	// terminates within sizeof(Buffer), so an over long expansion is truncated
	// rather than written past the end. A negative return is an encoding
	// error, where nothing usable was produced, so the result is made empty.
	if (written < 0)
	{
		Buffer[0] = '\0';
	}

	*this = Buffer;
}

//--------------------------------------------------------------------------
// Save To File
//--------------------------------------------------------------------------
void		
MString::SaveToFile(std::ofstream& file)
{
	std::string encoded;
	if (!TextEncoding::Convert(m_pString, m_Length, TextEncoding::Encoding::Utf8,
		TextEncoding::GetResourceEncoding(), encoded) || encoded.size() > 65536u) {
		file.setstate(std::ios::failbit);
		return;
	}
	const std::uint32_t length = static_cast<std::uint32_t>(encoded.size());
	const unsigned char prefix[4] = {
		static_cast<unsigned char>(length),
		static_cast<unsigned char>(length >> 8),
		static_cast<unsigned char>(length >> 16),
		static_cast<unsigned char>(length >> 24)
	};
	file.write(reinterpret_cast<const char*>(prefix), sizeof(prefix));
	if (length != 0)
		file.write(encoded.data(), static_cast<std::streamsize>(length));
}

//--------------------------------------------------------------------------
// Load From File
//--------------------------------------------------------------------------
void
MString::LoadFromFile(std::ifstream& file)
{
	unsigned char prefix[4]{};
	if (!file.read(reinterpret_cast<char*>(prefix), sizeof(prefix)))
		return;
	const std::uint32_t length = std::uint32_t(prefix[0]) |
		(std::uint32_t(prefix[1]) << 8) |
		(std::uint32_t(prefix[2]) << 16) |
		(std::uint32_t(prefix[3]) << 24);
	if (length > 65536u) {
		file.setstate(std::ios::failbit);
		return;
	}

	// Neither failed reads nor conversion/allocation failures publish a partial
	// value. In particular, never hand an unread tail to the encoding converter.
	auto input = std::make_unique<char[]>(static_cast<size_t>(length) + 1);
	if (length != 0 && !file.read(input.get(), length))
		return;
	input[length] = '\0';

	std::string decoded;
	if (!TextEncoding::Convert(input.get(), length, TextEncoding::GetResourceEncoding(),
		TextEncoding::Encoding::Utf8, decoded, TextEncoding::InvalidInput::Replace)) {
		file.setstate(std::ios::failbit);
		return;
	}
	auto converted = std::make_unique<char[]>(decoded.size() + 1);
	memcpy(converted.get(), decoded.c_str(), decoded.size() + 1);
	delete[] m_pString;
	m_pString = converted.release();
	m_Length = decoded.size();
}
