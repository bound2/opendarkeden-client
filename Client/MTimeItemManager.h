//-----------------------------------------------------------------------------
// MTimeItemManager.H									- by sonee
//-----------------------------------------------------------------------------
// MTimeItemManager
//  - records the information for items that carry a time limit.
//  - 2003.04.04
//-----------------------------------------------------------------------------

#ifndef __TIME_ITEM_MANAGER_HEADER__
#define __TIME_ITEM_MANAGER_HEADER__

#pragma warning(disable:4786)

// MonotonicClock.h brings <chrono> and, through Platform.h, <windows.h>
// into every translation unit that includes this header; MTypeDef.h
// includes nothing, so nothing here orders against it. (MSVC's <chrono>
// parenthesises its min/max, so the macro order would not matter anyway.)
#include "MonotonicClock.h"
#include "MTypeDef.h"
#include <map>

//-----------------------------------------------------------------------------
// The deadline held for one timed item.
//
// It used to be a DWORD, and the note here used to say that it was a
// count of seconds rather than of milliseconds - which it was: the
// register stored timeGetTime()/1000 plus the item's lifetime. The unit
// was the smaller of that value's two problems. The tick behind it is 32
// bits of milliseconds, so it goes round every 49.7 days, and when it
// does, every deadline still held here reads as up to 49.7 days in the
// future instead of long past
// (docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md,
// modernization backlog priority 5).
//
// It is now an absolute point on MonotonicClock's clock, kept at the
// whole-second resolution this class has always counted in, with a
// 64-bit rep that has nothing to wrap. Nothing outside the class ever
// read it - the register is reached through IsExist, IsExpired, the four
// Get* accessors and the map's own clear() - so the type is free to say
// what it means.
//-----------------------------------------------------------------------------
typedef std::chrono::time_point<MonotonicClock::Clock, std::chrono::seconds>
												TIMEITEM_DEADLINE;

class MTimeItemManager : public std::map<TYPE_OBJECTID, TIMEITEM_DEADLINE>
{
public :
	typedef std::map<TYPE_OBJECTID, TIMEITEM_DEADLINE>	TIMEITEM_MAP;

	MTimeItemManager();
	~MTimeItemManager();

	bool	IsExist( TYPE_OBJECTID objectID );
	bool	IsExpired( TYPE_OBJECTID objectID );

	bool	AddTimeItem( TYPE_OBJECTID objectID, DWORD time );			// time is in seconds
	bool	RemoveTimeItem( TYPE_OBJECTID objectID );


	int		GetDay( TYPE_OBJECTID objectID );
	int		GetHour( TYPE_OBJECTID objectID );
	int		GetMinute( TYPE_OBJECTID objectID );
	int		GetSecond( TYPE_OBJECTID objectID );

private :
	// Seconds left on an item, or zero once its deadline has been reached.
	// The four Get* accessors each split this one answer up, and
	// IsExpired asks it directly. It stays a 64-bit count rather than
	// narrowing to the DWORD the lifetime arrived as: nothing outside the
	// class reads it, and a clock that moves backwards - the test source
	// can - would otherwise truncate.
	std::chrono::seconds	GetRemainingSeconds( TYPE_OBJECTID objectID );
};

extern MTimeItemManager		*g_pTimeItemManager;

#endif