//--------------------------------------------------------------------------
// MString.cpp
//--------------------------------------------------------------------------

#include "Client_PCH.h"
#include <stdarg.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include "MString.h"
#include "DebugLog.h"

#ifdef PLATFORM_POSIX
#include <iconv.h>
#endif

// Emscripten has iconv in its sysroot
#ifdef __EMSCRIPTEN__
#include <iconv.h>
#endif

// Forward declaration
static char* ConvertGBKToUTF8(const char* gbkStr, size_t gbkLen, size_t& outLen);

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
// Relase
//--------------------------------------------------------------------------
// memory에서 제거
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
	if (m_Length > UINT32_MAX) {
		file.setstate(std::ios::failbit);
		return;
	}
	const std::uint32_t length = static_cast<std::uint32_t>(m_Length);
	const unsigned char prefix[4] = {
		static_cast<unsigned char>(length),
		static_cast<unsigned char>(length >> 8),
		static_cast<unsigned char>(length >> 16),
		static_cast<unsigned char>(length >> 24)
	};
	file.write(reinterpret_cast<const char*>(prefix), sizeof(prefix));
	if (length != 0)
		file.write(m_pString, static_cast<std::streamsize>(length));
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

	size_t convertedLength = 0;
	std::unique_ptr<char[]> converted;
	if (length != 0)
		converted.reset(ConvertGBKToUTF8(input.get(), length, convertedLength));
	else
		converted = std::move(input);

	delete[] m_pString;
	m_pString = converted.release();
	m_Length = convertedLength;
}

//--------------------------------------------------------------------------
// Convert to UTF-8 if needed (internal helper)
// NOTE: Auto-conversion disabled - resource files should be pre-converted to UTF-8
//       The conversion functions are kept here for reference/future use if needed.
//--------------------------------------------------------------------------
namespace {
	// Check if string is valid UTF-8 (used for validation)
	bool IsValidUtf8(const char* str, size_t len)
	{
		for (size_t i = 0; i < len; )
		{
			unsigned char c = str[i];
			int seqLen;

			if (c < 0x80)
			{
				seqLen = 1;
			}
			else if ((c >> 5) == 0x6)
			{
				seqLen = 2;
			}
			else if ((c >> 4) == 0xE)
			{
				seqLen = 3;
			}
			else if ((c >> 3) == 0x1E)
			{
				seqLen = 4;
			}
			else
			{
				return false;
			}

			if (i + seqLen > len)
				return false;

			for (int j = 1; j < seqLen; j++)
			{
				if ((str[i + j] & 0xC0) != 0x80)
					return false;
			}

			i += seqLen;
		}
		return true;
	}
}

// Convert GBK to UTF-8 - used by LoadFromFile for runtime conversion
static char* ConvertGBKToUTF8(const char* gbkStr, size_t gbkLen, size_t& outLen)
{
#ifdef PLATFORM_POSIX
	// Always try GBK conversion - don't skip even if it looks like valid UTF-8
	// because GBK strings can sometimes pass UTF-8 validation (especially ASCII)

	iconv_t cd = iconv_open("UTF-8", "GBK");
	if (cd != (iconv_t)-1)
	{
		size_t inLen = gbkLen;
		size_t outBufLen = gbkLen * 4;  // Allocate enough space
		char* inBuf = const_cast<char*>(gbkStr);
		char* outBuf = new char[outBufLen];
		char* outStart = outBuf;
		memset(outBuf, 0, outBufLen);

		size_t result = iconv(cd, &inBuf, &inLen, &outBuf, &outBufLen);
		iconv_close(cd);

		if (result != (size_t)-1 && inLen == 0)
		{
			// Conversion successful
			size_t convertedLen = outBuf - outStart;
			outStart[convertedLen] = '\0';  // Ensure null termination
			outLen = convertedLen;  // Return the actual converted length
			return outStart;
		}
		delete[] outStart;
	}

	// Fallback: just copy the string (no conversion or failed conversion)
	outLen = gbkLen;
	char* result = new char[gbkLen + 1];
	memcpy(result, gbkStr, gbkLen);
	result[gbkLen] = '\0';
	return result;
#else
	// Non-Mac platform: just copy (assume UTF-8 or ASCII)
	outLen = gbkLen;
	char* result = new char[gbkLen + 1];
	memcpy(result, gbkStr, gbkLen);
	result[gbkLen] = '\0';
	return result;
#endif
}

#if 1  // Enabled - runtime conversion from GBK to UTF-8
namespace {
	// Convert encoding using iconv
	std::string ConvertEncoding(const char* str, size_t len, const char* fromEncoding)
	{
#ifdef PLATFORM_POSIX
		iconv_t cd = iconv_open("UTF-8", fromEncoding);
		if (cd == (iconv_t)-1)
			return std::string(str, len);

		size_t inLen = len;
		size_t outLen = len * 4;
		char* inBuf = const_cast<char*>(str);
		char* outBuf = new char[outLen];
		char* outStart = outBuf;
		memset(outBuf, 0, outLen);

		size_t result = iconv(cd, &inBuf, &inLen, &outBuf, &outLen);
		iconv_close(cd);

		if (result == (size_t)-1)
		{
			delete[] outStart;
			return std::string(str, len);
		}

		std::string converted(outStart, outBuf - outStart);
		delete[] outStart;
		return converted;
#else
		return std::string(str, len);
#endif
	}
}

void MString::ConvertToUTF8IfNeeded()
{
	if (m_pString == NULL || m_Length == 0)
		return;

	// Check if already UTF-8
	if (IsValidUtf8(m_pString, m_Length))
		return;

	// Try GBK first (Chinese encoding)
	std::string converted = ConvertEncoding(m_pString, m_Length, "GBK");
	if (converted != std::string(m_pString, m_Length))
	{
		// Successfully converted from GBK
		DEBUG_ADD_FORMAT("MString: Converted from GBK: '%.*s' -> '%s'",
		         (int)m_Length, m_pString, converted.c_str());
		delete[] m_pString;
		m_Length = converted.size();
		m_pString = new char[m_Length + 1];
		memcpy(m_pString, converted.c_str(), m_Length);
		m_pString[m_Length] = '\0';
		return;
	}

	// Try CP949 as fallback (Korean encoding)
	converted = ConvertEncoding(m_pString, m_Length, "CP949");
	if (converted != std::string(m_pString, m_Length))
	{
		DEBUG_ADD_FORMAT("MString: Converted from CP949: '%.*s' -> '%s'",
		         (int)m_Length, m_pString, converted.c_str());
		delete[] m_pString;
		m_Length = converted.size();
		m_pString = new char[m_Length + 1];
		memcpy(m_pString, converted.c_str(), m_Length);
		m_pString[m_Length] = '\0';
		return;
	}

	// Try EUC-KR as another fallback
	converted = ConvertEncoding(m_pString, m_Length, "EUC-KR");
	if (converted != std::string(m_pString, m_Length))
	{
		DEBUG_ADD_FORMAT("MString: Converted from EUC-KR: '%.*s' -> '%s'",
		         (int)m_Length, m_pString, converted.c_str());
		delete[] m_pString;
		m_Length = converted.size();
		m_pString = new char[m_Length + 1];
		memcpy(m_pString, converted.c_str(), m_Length);
		m_pString[m_Length] = '\0';
	}
	else
	{
		// Conversion failed - log the problematic string
		DEBUG_ADD_FORMAT("MString: Failed to convert encoding: '%.*s' (len=%zu)",
		         (int)m_Length, m_pString, m_Length);
	}
}
#endif
