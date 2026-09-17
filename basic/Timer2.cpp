/*-----------------------------------------------------------------------------

	Timer2.cpp

	Timer version 2 implementation.

	2000.6.15. KJTINC

-----------------------------------------------------------------------------*/

#include "timer2.h"
#include <limits>

//----------------------------------------------------------------------------
// This manager keeps the API it has always had - DWORD milliseconds in,
// timer_id_t out, INVALID_TID for "no timer" - and nothing outside this
// file changed. What changed is underneath: the reference time and the
// interval are a MonotonicClock::TimePoint and a
// MonotonicClock::Duration instead of two DWORD tick counts, so the
// elapsed test in Execute() is a comparison of typed durations and no
// longer depends on 32-bit unsigned arithmetic wrapping the same way on
// both sides. The firing rule is unchanged in every other respect: a
// timer fires when at least its interval has elapsed, and the reference
// time then becomes the current time of that Execute() call rather than
// previous + interval, so a late frame does not build up a debt of
// missed fires.
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Global instance
//----------------------------------------------------------------------------
C_TIMER2		gC_timer2;

//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------
C_TIMER2::C_TIMER2()
{
	// Nothing is allocated here. gC_timer2 is a global, and the realloc()'d
	// array this replaced grew lazily on the first Add(), where a failed
	// allocation is answered with INVALID_TID rather than a terminate()
	// during static initialisation.
}

//----------------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------------
C_TIMER2::~C_TIMER2()
{
	m_timer_queue.reset();
}

//----------------------------------------------------------------------------
// Find - the live timer with this id, or NULL (private)
//----------------------------------------------------------------------------
C_TIMER2::S_TIMERUNIT *
C_TIMER2::Find(timer_id_t tid)
{
	if (!m_timer_queue) return NULL;
	const auto found = m_timer_queue->find(tid);
	return found == m_timer_queue->end() ? NULL : &found->second;
}

//----------------------------------------------------------------------------
// Add - Add a new timer
//----------------------------------------------------------------------------
timer_id_t
C_TIMER2::Add(DWORD dw_millisec, void (*fp_proc)(void))
{
	if (m_next_tid == INVALID_TID) return INVALID_TID;
	const timer_id_t tid = m_next_tid;
	S_TIMERUNIT unit;
	unit.fp_proc = fp_proc;
	unit.d_interval = MonotonicClock::Millis(dw_millisec);
	unit.tp_prev = MonotonicClock::Now();
	unit.bl_pause = 1; // Start paused.

	try
	{
		if (!m_timer_queue) m_timer_queue.emplace();
		if (!m_timer_queue->emplace(tid, unit).second) return INVALID_TID;
	}
	catch (...)
	{
		// Preserve the legacy allocation-failure contract.
		if (m_timer_queue && m_timer_queue->empty()) m_timer_queue.reset();
		return INVALID_TID;
	}

	// Never wrap into a negative or previously issued ID. Failed insertion
	// does not consume an ID; deletion does not make an old ID available.
	m_next_tid = tid == (std::numeric_limits<timer_id_t>::max)() ? INVALID_TID : tid + 1;
	return tid;
}

//----------------------------------------------------------------------------
// Delete - Delete a timer
//----------------------------------------------------------------------------
bool
C_TIMER2::Delete(timer_id_t &tid)
{
	if (!m_timer_queue || m_timer_queue->erase(tid) == 0) return false;
	if (m_timer_queue->empty()) m_timer_queue.reset();
	tid = INVALID_TID;
	return true;
}

//----------------------------------------------------------------------------
// Execute - Execute all active timers
//----------------------------------------------------------------------------
void
C_TIMER2::Execute()
{
	const MonotonicClock::TimePoint tp_now = MonotonicClock::Now();
	timer_id_t previous = INVALID_TID;
	while (m_timer_queue)
	{
		const auto current = m_timer_queue->upper_bound(previous);
		if (current == m_timer_queue->end()) break;
		const timer_id_t tid = current->first;
		previous = tid;
		const S_TIMERUNIT unit = current->second;
		if (!unit.bl_pause && tp_now - unit.tp_prev >= unit.d_interval)
		{
			if (unit.fp_proc != NULL) unit.fp_proc();
			// The callback may have deleted this timer. Only update it if
			// the same ID is still live; replacement timers have fresh IDs.
			if (S_TIMERUNIT* live = Find(tid)) live->tp_prev = tp_now;
		}
		// Do not retain an iterator across a callback that can erase it.
		// Timers added during a callback are reached in this same pass,
		// as before, but Add() starts them paused.
	}
}

//----------------------------------------------------------------------------
// Refresh - Reset a timer's tick count
//----------------------------------------------------------------------------
void
C_TIMER2::Refresh(timer_id_t tid)
{
	S_TIMERUNIT* pUnit = Find(tid);
	if (pUnit != NULL)
	{
		pUnit->tp_prev = MonotonicClock::Now();
	}
}

//----------------------------------------------------------------------------
// Pause - Pause a timer
//----------------------------------------------------------------------------
void
C_TIMER2::Pause(timer_id_t tid)
{
	S_TIMERUNIT* pUnit = Find(tid);
	if (pUnit != NULL)
	{
		pUnit->bl_pause = 1;
	}
}

//----------------------------------------------------------------------------
// Continue - Resume a paused timer
//----------------------------------------------------------------------------
void
C_TIMER2::Continue(timer_id_t tid)
{
	S_TIMERUNIT* pUnit = Find(tid);
	if (pUnit != NULL)
	{
		pUnit->bl_pause = 0;
		pUnit->tp_prev = MonotonicClock::Now();
	}
}

//----------------------------------------------------------------------------
// ResetSpeed - Change a timer's interval
//----------------------------------------------------------------------------
void
C_TIMER2::ResetSpeed(timer_id_t tid, DWORD millisec)
{
	S_TIMERUNIT* pUnit = Find(tid);
	if (pUnit != NULL)
	{
		pUnit->d_interval = MonotonicClock::Millis(millisec);
		pUnit->tp_prev = MonotonicClock::Now();
	}
}
