//----------------------------------------------------------------------
// MonotonicClock.h
//----------------------------------------------------------------------
//
// One monotonic clock for the whole client, and the seam that lets a
// test drive it. Now() is a steady_clock time point truncated to
// milliseconds; the 32-bit tick the client is written against wraps
// every 49.7 days, and a 64-bit millisecond rep does not.
//
// LegacyTicks() is NOT derived from Now(): with no test source it IS
// platform_get_ticks(), the same epoch timeGetTime() would have
// returned, so a class only half converted off the raw tick stays
// correct. The two therefore share no epoch, and the type system
// refuses to mix a TimePoint with a DWORD tick.
//
// SetTestSource() installs a function Now() calls in place of
// steady_clock::now(); prefer ScopedTestSource, which restores the
// previous source even when a check fails.
//
//----------------------------------------------------------------------

#ifndef __MONOTONIC_CLOCK_H__
#define __MONOTONIC_CLOCK_H__

// <chrono> ahead of Platform.h, which drags in <windows.h> without
// NOMINMAX.
#include <chrono>

#include "Platform.h"

namespace MonotonicClock {

//----------------------------------------------------------------------
// The clock, its duration and its time point. TimePoint is pinned to
// millisecond resolution, not steady_clock's native tick.
//----------------------------------------------------------------------
typedef std::chrono::steady_clock					Clock;
typedef std::chrono::milliseconds					Duration;
typedef std::chrono::time_point<Clock, Duration>	TimePoint;

//----------------------------------------------------------------------
// A replacement for steady_clock::now(), installed by a test.
//----------------------------------------------------------------------
typedef TimePoint (*SourceFn)();

//----------------------------------------------------------------------
// The current monotonic time, from the installed test source if there is
// one and from steady_clock otherwise.
//----------------------------------------------------------------------
TimePoint	Now();

//----------------------------------------------------------------------
// The legacy 32-bit millisecond tick: exactly what timeGetTime() /
// platform_get_ticks() returns, unless a test source is installed, in
// which case it is the low 32 bits of that source's millisecond count.
//----------------------------------------------------------------------
DWORD		LegacyTicks();

//----------------------------------------------------------------------
// Test seam. Passing NULL restores the real clock. Prefer
// ScopedTestSource below, which cannot leak an installed source out of a
// failing test.
//----------------------------------------------------------------------
void		SetTestSource(SourceFn pfn_source);
SourceFn	GetTestSource();

//----------------------------------------------------------------------
// Installs a test source for a scope and restores whatever was there
// before - including NULL, the real clock.
//----------------------------------------------------------------------
class ScopedTestSource
{
public:
	explicit ScopedTestSource(SourceFn pfn_source)
		: m_pfn_previous(GetTestSource())
	{
		SetTestSource(pfn_source);
	}

	~ScopedTestSource()
	{
		SetTestSource(m_pfn_previous);
	}

	ScopedTestSource(const ScopedTestSource&) = delete;
	ScopedTestSource& operator=(const ScopedTestSource&) = delete;

private:
	SourceFn	m_pfn_previous;
};

//----------------------------------------------------------------------
// Conversions for call sites that still speak in DWORD milliseconds.
//----------------------------------------------------------------------
inline Duration
Millis(DWORD dw_millisec)
{
	return Duration(static_cast<Duration::rep>(dw_millisec));
}

//----------------------------------------------------------------------
// A time point built from an absolute millisecond count. Only tests
// should need this; production code gets its time points from Now().
//----------------------------------------------------------------------
inline TimePoint
FromMillis(unsigned long long ull_millisec)
{
	return TimePoint(Duration(static_cast<Duration::rep>(ull_millisec)));
}

} // namespace MonotonicClock

#endif // __MONOTONIC_CLOCK_H__
