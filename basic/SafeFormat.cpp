//----------------------------------------------------------------------
// SafeFormat.cpp
//----------------------------------------------------------------------
//
// See SafeFormat.h for why this exists. The rule the whole file
// implements is one sentence: a conversion is performed only when the
// next argument can satisfy it, and is copied out literally otherwise.
//
//----------------------------------------------------------------------

#include "SafeFormat.h"
#include "Platform.h"

#include <stdio.h>
#include <string.h>

namespace SafeFormat {

size_t Copy(char* destination, size_t capacity, const char* source)
{
	if (!destination || !capacity) return 0;
	size_t length = 0;
	if (source)
		while (length < capacity - 1 && source[length]) ++length;
	// Measure before writing so interior/self aliases remain valid.
	if (length) memmove(destination, source, length);
	destination[length] = '\0';
	return length;
}

size_t Append(char* destination, size_t capacity, const char* source)
{
	if (!destination || !capacity) return 0;
	size_t length = 0;
	while (length < capacity - 1 && destination[length]) ++length;
	return length + Copy(destination + length, capacity - length, source);
}

namespace {

//----------------------------------------------------------------------
// Caps.
//
// The output is bounded by the destination either way, so these do not
// prevent an overflow - they stop one conversion from eating the whole
// row. 32 is the number SanitizeGameStringTable() rejects at, for the
// same reason and against the same data; the real vocabulary of the
// built-in table is %d, %s and %02d, whose widest legitimate field is 2.
//----------------------------------------------------------------------
const int		MAX_FIELD_WIDTH		= 32;
const int		MAX_PRECISION		= 32;
const size_t	MAX_FLAGS			= 5;

// Preserve the accepted CRT flag vocabulary, including its text extensions.
#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_MACOS)
constexpr bool CRT_ZERO_PADS_TEXT = true;
#else
constexpr bool CRT_ZERO_PADS_TEXT = false;
#endif

// Digit runs are clamped here too, so a "%99999999999d" in the data
// cannot overflow the int the width is accumulated into.
const int		MAX_DIGITS_VALUE	= 100000;


//----------------------------------------------------------------------
// Append bytes that are not a conversion. Returns what really fit.
//----------------------------------------------------------------------
size_t
AppendLiteral(char* pDest, size_t nSize, size_t nOut,
			  const char* pFrom, size_t nLen)
{
	const size_t nRoom = nSize - 1 - nOut;

	if (nLen > nRoom)
	{
		nLen = nRoom;
	}

	memcpy(pDest + nOut, pFrom, nLen);

	return nLen;
}


//----------------------------------------------------------------------
// Can this argument satisfy this conversion?
//
// The integer conversions take either signedness, because a call site
// handing an unsigned short to a "%d" in the table is ordinary and safe.
// What is not safe is a conversion reading a different kind of thing than
// it was given - a char* as a double, or an int as a char*.
//----------------------------------------------------------------------
bool
Accepts(char cConversion, const Arg& arg)
{
	switch (cConversion)
	{
		case 'd': case 'i':
		case 'u': case 'o': case 'x': case 'X':
		case 'c':
			return arg.kind==ARG_SIGNED || arg.kind==ARG_UNSIGNED;

		case 's':
			return arg.kind==ARG_STRING;

		case 'e': case 'E':
		case 'f': case 'F':
		case 'g': case 'G':
		case 'a': case 'A':
			return arg.kind==ARG_DOUBLE;

		case 'p':
			return arg.kind==ARG_POINTER;

		// Everything else is refused by not being listed, 'n' and the
		// wide 'S'/'C' included. 'n' writes through its argument; 'S'
		// and 'C' read a char* as a wchar_t* and walk past its end.
		default:
			return false;
	}
}


//----------------------------------------------------------------------
// Emit validated values with literal CRT formats. Input flags and field sizes
// control bounded padding; they are never assembled into a CRT format string.
size_t AppendPadding(char* dest, size_t size, size_t out, char value, size_t count)
{
	const size_t room = size - 1 - out;
	if (count > room) count = room;
	memset(dest + out, value, count);
	return count;
}

size_t EmitField(char* dest, size_t size, size_t out, const char* prefix,
				 size_t prefixLength, const char* value, size_t length,
				 int width, bool left, bool zero)
{
	const size_t start = out;
	const size_t total = prefixLength + length;
	const size_t padding = width > 0 && static_cast<size_t>(width) > total
		? static_cast<size_t>(width) - total : 0;
	if (!left && !zero) out += AppendPadding(dest, size, out, ' ', padding);
	out += AppendLiteral(dest, size, out, prefix, prefixLength);
	if (!left && zero) out += AppendPadding(dest, size, out, '0', padding);
	out += AppendLiteral(dest, size, out, value, length);
	if (left) out += AppendPadding(dest, size, out, ' ', padding);
	return out - start;
}

// Float spelling (including non-finite values) stays with the platform CRT.
// Every conversion and flag combination below is a literal. A negative field
// width supplies left alignment; zero width leaves space padding to EmitField.
int EmitFloat(char* buffer, size_t capacity, double value, int width, int precision,
			  char conversion, bool plus, bool blank, bool alternate)
{
	switch (conversion) {
	case 'e':
		if (plus) return alternate ? snprintf(buffer, capacity, "%+#0*.*e", width, precision, value)
			: snprintf(buffer, capacity, "%+0*.*e", width, precision, value);
		if (blank) return alternate ? snprintf(buffer, capacity, "% #0*.*e", width, precision, value)
			: snprintf(buffer, capacity, "% 0*.*e", width, precision, value);
		return alternate ? snprintf(buffer, capacity, "%#0*.*e", width, precision, value)
			: snprintf(buffer, capacity, "%0*.*e", width, precision, value);
	case 'E':
		if (plus) return alternate ? snprintf(buffer, capacity, "%+#0*.*E", width, precision, value)
			: snprintf(buffer, capacity, "%+0*.*E", width, precision, value);
		if (blank) return alternate ? snprintf(buffer, capacity, "% #0*.*E", width, precision, value)
			: snprintf(buffer, capacity, "% 0*.*E", width, precision, value);
		return alternate ? snprintf(buffer, capacity, "%#0*.*E", width, precision, value)
			: snprintf(buffer, capacity, "%0*.*E", width, precision, value);
	case 'f':
		if (plus) return alternate ? snprintf(buffer, capacity, "%+#0*.*f", width, precision, value)
			: snprintf(buffer, capacity, "%+0*.*f", width, precision, value);
		if (blank) return alternate ? snprintf(buffer, capacity, "% #0*.*f", width, precision, value)
			: snprintf(buffer, capacity, "% 0*.*f", width, precision, value);
		return alternate ? snprintf(buffer, capacity, "%#0*.*f", width, precision, value)
			: snprintf(buffer, capacity, "%0*.*f", width, precision, value);
	case 'F':
		if (plus) return alternate ? snprintf(buffer, capacity, "%+#0*.*F", width, precision, value)
			: snprintf(buffer, capacity, "%+0*.*F", width, precision, value);
		if (blank) return alternate ? snprintf(buffer, capacity, "% #0*.*F", width, precision, value)
			: snprintf(buffer, capacity, "% 0*.*F", width, precision, value);
		return alternate ? snprintf(buffer, capacity, "%#0*.*F", width, precision, value)
			: snprintf(buffer, capacity, "%0*.*F", width, precision, value);
	case 'g':
		if (plus) return alternate ? snprintf(buffer, capacity, "%+#0*.*g", width, precision, value)
			: snprintf(buffer, capacity, "%+0*.*g", width, precision, value);
		if (blank) return alternate ? snprintf(buffer, capacity, "% #0*.*g", width, precision, value)
			: snprintf(buffer, capacity, "% 0*.*g", width, precision, value);
		return alternate ? snprintf(buffer, capacity, "%#0*.*g", width, precision, value)
			: snprintf(buffer, capacity, "%0*.*g", width, precision, value);
	case 'G':
		if (plus) return alternate ? snprintf(buffer, capacity, "%+#0*.*G", width, precision, value)
			: snprintf(buffer, capacity, "%+0*.*G", width, precision, value);
		if (blank) return alternate ? snprintf(buffer, capacity, "% #0*.*G", width, precision, value)
			: snprintf(buffer, capacity, "% 0*.*G", width, precision, value);
		return alternate ? snprintf(buffer, capacity, "%#0*.*G", width, precision, value)
			: snprintf(buffer, capacity, "%0*.*G", width, precision, value);
	case 'a':
		if (plus) return alternate ? snprintf(buffer, capacity, "%+#0*.*a", width, precision, value)
			: snprintf(buffer, capacity, "%+0*.*a", width, precision, value);
		if (blank) return alternate ? snprintf(buffer, capacity, "% #0*.*a", width, precision, value)
			: snprintf(buffer, capacity, "% 0*.*a", width, precision, value);
		return alternate ? snprintf(buffer, capacity, "%#0*.*a", width, precision, value)
			: snprintf(buffer, capacity, "%0*.*a", width, precision, value);
	case 'A':
		if (plus) return alternate ? snprintf(buffer, capacity, "%+#0*.*A", width, precision, value)
			: snprintf(buffer, capacity, "%+0*.*A", width, precision, value);
		if (blank) return alternate ? snprintf(buffer, capacity, "% #0*.*A", width, precision, value)
			: snprintf(buffer, capacity, "% 0*.*A", width, precision, value);
		return alternate ? snprintf(buffer, capacity, "%#0*.*A", width, precision, value)
			: snprintf(buffer, capacity, "%0*.*A", width, precision, value);
	default: return -1;
	}
}

size_t Emit(char* dest, size_t size, size_t out, const char* flags,
			int width, int precision, char conversion, const Arg& arg)
{
	if (width > MAX_FIELD_WIDTH) width = MAX_FIELD_WIDTH;
	if (precision > MAX_PRECISION) precision = MAX_PRECISION;
	const bool left = strchr(flags, '-') != NULL;
	const bool zero = strchr(flags, '0') != NULL;
	const bool alternate = strchr(flags, '#') != NULL;
	if (conversion == 's') {
		const char* value = arg.pString != NULL ? arg.pString : "";
		size_t length = 0;
		while ((precision < 0 || length < static_cast<size_t>(precision)) && value[length]) ++length;
		return EmitField(dest, size, out, "", 0, value, length, width, left, zero && CRT_ZERO_PADS_TEXT);
	}
	if (conversion == 'c') {
		const char value = static_cast<char>(arg.kind == ARG_SIGNED ? arg.sValue : arg.uValue);
		return EmitField(dest, size, out, "", 0, &value, 1, width, left, zero && CRT_ZERO_PADS_TEXT);
	}
	if (conversion == 'p') {
		// Keep the CRT's pointer spelling. Its supported decimal field width
		// is applied here, so no non-standard flags reach printf's %p.
		char buffer[64];
		const int count = snprintf(buffer, sizeof(buffer), "%p", arg.pPointer);
		if (count < 0) return 0;
		const size_t length = static_cast<size_t>(count) < sizeof(buffer)
			? static_cast<size_t>(count) : sizeof(buffer) - 1;
		// The Unix CRTs render hexadecimal pointers with a 0x prefix and
		// accept integer-style precision. MSVC's fixed-width pointer spelling
		// ignores precision and zero padding; GNU's textual null does too.
		const bool hex = length >= 2 && buffer[0] == '0' && buffer[1] == 'x';
		if (!hex) return EmitField(dest, size, out, "", 0, buffer, length, width, left, false);
		char digits[64];
		// BSD renders a null pointer with zero precision as the prefix alone.
		size_t digitsLength = arg.pPointer == NULL && precision == 0 ? 0 : length - 2;
		const size_t zeros = precision > 0 && static_cast<size_t>(precision) > digitsLength
			? static_cast<size_t>(precision) - digitsLength : 0;
		memset(digits, '0', zeros);
		memcpy(digits + zeros, buffer + 2, digitsLength);
		digitsLength += zeros;
		const char* prefix = "0x";
		size_t prefixLength = 2;
#if defined(__GLIBC__)
		// GNU treats non-null %p like a signed hexadecimal field; BSD and
		// UCRT ignore these sign flags. GNU's textual null returned above.
		if (strchr(flags, '+')) { prefix = "+0x"; prefixLength = 3; }
		else if (strchr(flags, ' ')) { prefix = " 0x"; prefixLength = 3; }
#endif
		return EmitField(dest, size, out, prefix, prefixLength, digits, digitsLength,
			width, left, zero && precision < 0);
	}

	// A double in fixed notation needs at most 309 integer digits, a sign,
	// the decimal point and the bounded 32 fractional digits. Integers are
	// smaller. This buffer never scales with untrusted field sizes.
	char buffer[512];
	int count = -1;
	bool signedValue = false;
	unsigned long long unsignedValue = arg.kind == ARG_UNSIGNED ? arg.uValue : 0;
	if (conversion == 'd' || conversion == 'i') {
		long long value = arg.kind == ARG_SIGNED ? arg.sValue : static_cast<long long>(arg.uValue);
		if (arg.nBytes <= sizeof(int)) value = static_cast<int>(value);
		count = snprintf(buffer, sizeof(buffer), "%.*lld", precision, value);
		signedValue = true;
	} else if (conversion == 'u' || conversion == 'o' || conversion == 'x' || conversion == 'X') {
		unsignedValue = arg.kind == ARG_UNSIGNED ? arg.uValue : static_cast<unsigned long long>(arg.sValue);
		if (arg.nBytes <= sizeof(int)) unsignedValue = static_cast<unsigned int>(unsignedValue);
		switch (conversion) {
		case 'u': count = snprintf(buffer, sizeof(buffer), "%.*llu", precision, unsignedValue); break;
		case 'o': count = snprintf(buffer, sizeof(buffer), "%.*llo", precision, unsignedValue); break;
		case 'x': count = snprintf(buffer, sizeof(buffer), "%.*llx", precision, unsignedValue); break;
		case 'X': count = snprintf(buffer, sizeof(buffer), "%.*llX", precision, unsignedValue); break;
		}
	} else {
		const int field = width < 0 ? 0 : width;
		count = EmitFloat(buffer, sizeof(buffer), arg.dValue,
			left ? -field : zero ? field : 0, precision, conversion,
			strchr(flags, '+') != NULL, strchr(flags, ' ') != NULL, alternate);
		if (count < 0) { dest[out] = '\0'; return 0; }
		const size_t length = static_cast<size_t>(count) < sizeof(buffer)
			? static_cast<size_t>(count) : sizeof(buffer) - 1;
		return EmitField(dest, size, out, "", 0, buffer, length, width, left, false);
	}
	if (count < 0) { dest[out] = '\0'; return 0; }
	size_t length = static_cast<size_t>(count);
	if (length >= sizeof(buffer)) length = sizeof(buffer) - 1;
	const char* value = buffer;
	char prefix[4] = {};
	size_t prefixLength = 0;
	if (signedValue) {
		if (length && *value == '-') {
			prefix[prefixLength++] = '-'; ++value; --length;
		} else if (strchr(flags, '+')) prefix[prefixLength++] = '+';
		else if (strchr(flags, ' ')) prefix[prefixLength++] = ' ';
	}
	if (alternate && conversion == 'o' && (!length || *value != '0')) {
		prefix[prefixLength++] = '0';
	} else if (alternate && unsignedValue && (conversion == 'x' || conversion == 'X')) {
		prefix[prefixLength++] = '0'; prefix[prefixLength++] = conversion;
	}
	const bool padZeros = zero && precision < 0;
	return EmitField(dest, size, out, prefix, prefixLength, value, length, width, left, padZeros);
}

} // anonymous namespace


//----------------------------------------------------------------------
// Format V
//----------------------------------------------------------------------
int
FormatV(char* pDest, size_t nSize, const char* pFormat,
		const Arg* pArgs, size_t nCount)
{
	if (pDest==NULL || nSize==0)
	{
		return 0;
	}

	pDest[0] = '\0';

	if (pFormat == NULL)
	{
		return 0;
	}

	if (pArgs == NULL)
	{
		nCount = 0;
	}

	size_t			nOut	= 0;
	size_t			nNext	= 0;
	const char*		p		= pFormat;

	while (*p!='\0' && nOut+1<nSize)
	{
		if (*p != '%')
		{
			nOut += AppendLiteral(pDest, nSize, nOut, p, 1);
			p++;
			continue;
		}

		// pSpec keeps the '%', so the whole specification can be copied
		// out unchanged when it turns out not to be performable.
		const char* const	pSpec	= p;
		const char*			q		= p + 1;

		if (*q == '%')
		{
			nOut += AppendLiteral(pDest, nSize, nOut, "%", 1);
			p = q + 1;
			continue;
		}

		//--------------------------------------------------------------
		// flags
		//--------------------------------------------------------------
		char	szFlags[MAX_FLAGS+1];
		size_t	nFlags	= 0;
		bool	bRefuse	= false;

		while (*q=='-' || *q=='+' || *q==' ' || *q=='0' || *q=='#')
		{
			if (nFlags < MAX_FLAGS)
			{
				szFlags[nFlags++] = *q;
			}
			else
			{
				// More flag characters than the vocabulary has. Nothing
				// legitimate looks like this.
				bRefuse = true;
			}

			q++;
		}

		szFlags[nFlags] = '\0';

		//--------------------------------------------------------------
		// width, then precision. A '*' takes it from an argument, which
		// changes how many arguments the specification consumes - the
		// one thing a data supplied format must not decide.
		//--------------------------------------------------------------
		int nWidth = -1;

		if (*q == '*')
		{
			bRefuse = true;
			q++;
		}
		else
		{
			while (*q>='0' && *q<='9')
			{
				if (nWidth < 0)
				{
					nWidth = 0;
				}

				if (nWidth < MAX_DIGITS_VALUE)
				{
					nWidth = nWidth*10 + (*q - '0');
				}

				q++;
			}
		}

		int nPrecision = -1;

		if (*q == '.')
		{
			q++;
			nPrecision = 0;

			if (*q == '*')
			{
				bRefuse = true;
				q++;
			}
			else
			{
				while (*q>='0' && *q<='9')
				{
					if (nPrecision < MAX_DIGITS_VALUE)
					{
						nPrecision = nPrecision*10 + (*q - '0');
					}

					q++;
				}
			}
		}

		//--------------------------------------------------------------
		// length modifier - parsed only so that it can be stepped over.
		// Emit selects the integer width from the tagged argument.
		//--------------------------------------------------------------
		if (*q=='h' || *q=='l')
		{
			const char cLength = *q;

			q++;

			if (*q == cLength)
			{
				q++;
			}
		}
		else if (*q=='j' || *q=='z' || *q=='t' || *q=='L' || *q=='w')
		{
			q++;
		}
		else if (*q == 'I')
		{
			q++;

			if ((q[0]=='6' && q[1]=='4') || (q[0]=='3' && q[1]=='2'))
			{
				q += 2;
			}
		}

		//--------------------------------------------------------------
		// conversion
		//--------------------------------------------------------------
		const char cConversion = *q;

		if (cConversion == '\0')
		{
			// The format ended inside a specification. What is left of
			// it is text, not an instruction.
			nOut += AppendLiteral(pDest, nSize, nOut, pSpec, (size_t)(q-pSpec));
			break;
		}

		q++;

		const Arg* const pArg = (nNext < nCount) ? &pArgs[nNext] : NULL;

		if (bRefuse || pArg==NULL || !Accepts(cConversion, *pArg))
		{
			// Copied out as written, and the argument is NOT consumed:
			// a specification the format got wrong must not push every
			// later one onto the wrong value.
			nOut += AppendLiteral(pDest, nSize, nOut, pSpec, (size_t)(q-pSpec));
			p = q;
			continue;
		}

		nOut += Emit(pDest, nSize, nOut, szFlags, nWidth, nPrecision, cConversion, *pArg);
		nNext++;
		p = q;
	}

	pDest[nOut] = '\0';

	return (int)nOut;
}

} // namespace SafeFormat
