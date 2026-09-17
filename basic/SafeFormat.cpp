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

#include <stdio.h>
#include <string.h>

namespace SafeFormat {

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
// Rebuild the specification from the parts this file parsed.
//
// The length modifier the entry supplied is discarded and replaced by
// the one the argument's real width calls for. That is what makes a
// "%ld" against a 32 bit argument print correctly instead of reading
// eight bytes, and what turns a "%ls" into a narrow print of the char*
// that was really passed rather than a wchar_t* scan of it.
//----------------------------------------------------------------------
void
BuildSpec(char* pSpec, size_t nSpec, const char* pFlags, int nWidth,
		  int nPrecision, char cConversion, const Arg& arg)
{
	char* w = pSpec;

	*w++ = '%';

	for (const char* f=pFlags; *f!='\0'; f++)
	{
		*w++ = *f;
	}

	if (nWidth >= 0)
	{
		if (nWidth > MAX_FIELD_WIDTH)
		{
			nWidth = MAX_FIELD_WIDTH;
		}

		w += snprintf(w, nSpec - (w - pSpec), "%d", nWidth);
	}

	if (nPrecision >= 0)
	{
		if (nPrecision > MAX_PRECISION)
		{
			nPrecision = MAX_PRECISION;
		}

		w += snprintf(w, nSpec - (w - pSpec), ".%d", nPrecision);
	}

	const bool bIsInteger = (cConversion=='d' || cConversion=='i'
						  || cConversion=='u' || cConversion=='o'
						  || cConversion=='x' || cConversion=='X');

	if (bIsInteger && arg.nBytes > sizeof(int))
	{
		*w++ = 'l';
		*w++ = 'l';
	}

	*w++ = cConversion;
	*w   = '\0';
}


//----------------------------------------------------------------------
// Perform one conversion into what is left of the destination.
//
// The width a value is issued at follows the width the caller declared,
// not the widest type available: printf("%x", -1) prints ffffffff for an
// int and sixteen f for a long long, and this must not change what a
// call site already printed correctly.
//----------------------------------------------------------------------
size_t
Emit(char* pDest, size_t nSize, size_t nOut, const char* pSpec,
	 char cConversion, const Arg& arg)
{
	// The caller guarantees room for at least one byte plus a terminator.
	const size_t	nRoom	= nSize - nOut;
	char* const		pAt		= pDest + nOut;

	int n = -1;

	switch (cConversion)
	{
		case 'd': case 'i':
		{
			const long long value = (arg.kind==ARG_SIGNED)
								  ? arg.sValue
								  : (long long)arg.uValue;

			n = (arg.nBytes > sizeof(int))
			  ? snprintf(pAt, nRoom, pSpec, value)
			  : snprintf(pAt, nRoom, pSpec, (int)value);
			break;
		}

		case 'u': case 'o': case 'x': case 'X':
		{
			const unsigned long long value = (arg.kind==ARG_UNSIGNED)
										   ? arg.uValue
										   : (unsigned long long)arg.sValue;

			n = (arg.nBytes > sizeof(int))
			  ? snprintf(pAt, nRoom, pSpec, value)
			  : snprintf(pAt, nRoom, pSpec, (unsigned int)value);
			break;
		}

		case 'c':
		{
			const long long value = (arg.kind==ARG_SIGNED)
								  ? arg.sValue
								  : (long long)arg.uValue;

			n = snprintf(pAt, nRoom, pSpec, (int)value);
			break;
		}

		case 's':
		{
			// A NULL here is a call site bug rather than hostile data.
			// It prints as nothing, which is what GetGameString() does
			// with an id it cannot resolve; the CRT's "(null)" would put
			// a word in the user interface that means nothing to a
			// player.
			n = snprintf(pAt, nRoom, pSpec,
						 arg.pString!=NULL ? arg.pString : "");
			break;
		}

		case 'p':
		{
			n = snprintf(pAt, nRoom, pSpec, arg.pPointer);
			break;
		}

		default:
		{
			n = snprintf(pAt, nRoom, pSpec, arg.dValue);
			break;
		}
	}

	// A negative return is an encoding error: the contents are
	// unspecified, so nothing is kept from it.
	if (n < 0)
	{
		*pAt = '\0';
		return 0;
	}

	// snprintf reports the length it would have written. What was really
	// stored is that, or the room there was.
	if ((size_t)n >= nRoom)
	{
		return nRoom - 1;
	}

	return (size_t)n;
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
		// BuildSpec issues the one the argument really needs.
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

		char szSpec[32];
		BuildSpec(szSpec, sizeof(szSpec), szFlags, nWidth, nPrecision, cConversion, *pArg);

		nOut += Emit(pDest, nSize, nOut, szSpec, cConversion, *pArg);
		nNext++;
		p = q;
	}

	pDest[nOut] = '\0';

	return (int)nOut;
}

} // namespace SafeFormat
