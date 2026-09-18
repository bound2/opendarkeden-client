/*-----------------------------------------------------------------------------

	Timer2.h

	Timer version 2.

	2000.6.15. KJTINC

-----------------------------------------------------------------------------*/

#ifndef __TIMER2__
#define __TIMER2__

// MonotonicClock.h first: it includes <chrono> ahead of the <windows.h>
// that Basics.h drags in, and this project does not define NOMINMAX.
#include "MonotonicClock.h"
#include "Basics.h"

#include <map>
#include <optional>

typedef long timer_id_t;

#define INVALID_TID						-1
#define INVALID_INDEX					-1

//----------------------------------------------------------------------------
// Class Timer - Timer Manager.
//----------------------------------------------------------------------------
class C_TIMER2
{
private:
	// Test-only inspection of retained timer storage after repeated deletion.
	friend struct Timer2TestAccess;
	//
	// One timer.
	//
	// tp_prev and d_interval used to be a pair of DWORD millisecond
	// counts read off GetTickCount(). Their 64-bit millisecond rep has no
	// wrap for a "previous + delay" sum to carry past, and the typing
	// keeps a millisecond count apart from every other DWORD
	// (MonotonicClock.h). The public API still speaks DWORD milliseconds
	// and is unchanged.
	//
	struct S_TIMERUNIT
	{
		DWORD								bl_pause;	// being paused ?
		MonotonicClock::TimePoint		tp_prev;		// reference time: last fire, or last reset
		MonotonicClock::Duration		d_interval;	// how long between fires

		void				(*fp_proc)(void);	// method to execute
	};

	//
	// Timer queue.
	//
	// Keep storage only for live timers. IDs are never reused, so a stale
	// handle cannot pause/delete a later timer. The ordered map preserves
	// creation order while allowing callbacks to erase themselves or others.
	//
	// Some map implementations allocate a sentinel on construction. Delay
	// that allocation until Add(), where allocation failure is recoverable.
	std::optional<std::map<timer_id_t, S_TIMERUNIT>> m_timer_queue;
	timer_id_t m_next_tid = 0; // INVALID_TID once the ID range is exhausted.

	//
	// The one place the "is this a live timer?" test lives. Returns NULL
	// for an unknown or deleted id.
	//
	S_TIMERUNIT *	Find(timer_id_t tid);

public:
	C_TIMER2();
	~C_TIMER2();

	void	Execute();
	void	Refresh(timer_id_t tid);
	void	Pause(timer_id_t tid);
	void	Continue(timer_id_t tid);
	bool	Delete(timer_id_t &tid);
	void	ResetSpeed(timer_id_t tid, DWORD millisec);

	timer_id_t Add(DWORD dw_millisec, void (*fp_proc)(void));
};

extern C_TIMER2		gC_timer2;

#endif
