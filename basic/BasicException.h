/*-----------------------------------------------------------------------------

	BasicException.h

	2000.8.25. KJTINC

-----------------------------------------------------------------------------*/

#ifndef __BASICEXCEPTION_H__
#define __BASICEXCEPTION_H__

#include <assert.h>
#include <cstddef>       /* NULL: MSVC's <assert.h> brings it, libstdc++'s does not */
#include <source_location>

#define _Error(code)						g_BasicException(code, NULL)
#define _ErrorStr(sz_error)			g_BasicException(INVALID_EXCEPTION_CODE, sz_error)
#define CheckMemAlloc(x)				if (!x) _Error(MEM_ALLOC)

enum EXCEPTION_CODE
{
	INVALID_EXCEPTION_CODE,

	MEM_ALLOC,		// memory allocation failed.
	FILE_OPEN,		// file open failed.
	FILE_LOAD,		// file load failed.
	FAILED_JOB,		// job failed.
	NULL_REF,		// null referenced.
};

/*-----------------------------------------------------------------------------

	ExceptionSite

	Where a diagnostic was raised. std::source_location cannot be built with a
	file and a line of one's own choosing, so this is the single shape that
	carries both what the C++20 entry point captured for itself and what the
	older (file, line) entry point was handed.

	The default constructor captures the CALLER's location: a
	std::source_location::current() written as a default argument is evaluated
	at the call site rather than here, which is what lets _Error and its
	siblings stop forwarding __FILE__ and __LINE__ by hand and still record the
	line of the macro use. The standard states that as recommended practice
	rather than a guarantee, and says nothing about the nested case used here,
	where this constructor is itself the default argument of g_BasicException.
	MSVC, Clang and GCC all report the outermost call site;
	tests/unit/test_source_location_diagnostics.cpp pins that against the test
	translation unit's own __FILE__ and __LINE__, and is what holds it.

-----------------------------------------------------------------------------*/
struct ExceptionSite
{
	const char*		file;
	unsigned long	line;
	const char*		function;	// NULL when only a file and a line were supplied

	ExceptionSite(const std::source_location& location = std::source_location::current())
		: file(location.file_name()),
		  line((unsigned long)location.line()),
		  function(location.function_name())
	{
	}

	ExceptionSite(const char* sz_file, unsigned long ul_line)
		: file(sz_file),
		  line(ul_line),
		  function(NULL)
	{
	}
};

/*-----------------------------------------------------------------------------

	Test seam.

	g_BasicException never returns in a shipped build - it asserts in Debug and
	exits in Release - so neither it nor anything that calls it can be exercised
	from a test binary. With a reporter installed, g_BasicException hands it the
	code, the message and the site and RETURNS instead of taking the fatal
	action. No shipped translation unit installs one, so the shipped path is
	what it always was; tests/unit/test_source_location_diagnostics.cpp is the
	only user.

-----------------------------------------------------------------------------*/
typedef void (*BasicExceptionReporter)(EXCEPTION_CODE code, const char* sz_error, const ExceptionSite& site);

void g_SetNewHandler();

// Installs a reporter and returns the one it replaced, so a caller can put
// back whatever it found.
BasicExceptionReporter g_SetBasicExceptionReporter(BasicExceptionReporter reporter);

// C++20 entry point. The caller's file, line and function arrive through the
// defaulted site, so no call site has to forward __FILE__ and __LINE__.
void g_BasicException(EXCEPTION_CODE code, const char* sz_error, const ExceptionSite& site = ExceptionSite());

// Compatibility entry point for the call sites - and, before this change, the
// macros above - that name a file and a line explicitly. It records exactly
// what it is given and forwards to the function above.
void g_BasicException(EXCEPTION_CODE code, const char* sz_error, const char* file, unsigned long line);

#endif
