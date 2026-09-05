#include "Client_PCH.h"
#include "MTimeItemManager.h"

//-----------------------------------------------------------------------------
// This register counts in whole seconds and always has: AddTimeItem is
// handed a lifetime in seconds, and the four accessors split a remaining
// count of seconds into days, hours, minutes and seconds. What moved is
// what those seconds are counted from.
//
// Every tick read in this file was timeGetTime()/1000. On this build
// Platform.h redefines timeGetTime() as platform_get_ticks(), so these
// sites were already reading a 1 ms counter and none of them was ever
// quantised to the ~15.6 ms step Win32 GetTickCount() advances in; the
// resolution is therefore unchanged by this move, and only the epoch and
// the width are (MonotonicClock.h, "Which tick is which").
//
// MonotonicClock::Now() is used rather than LegacyTicks() because the
// deadline is only ever compared with another read from this same file.
// It is not put in a packet, not handed to another class and not stored
// anywhere that outlives the process, so both sides of every comparison
// move epochs together and no half-converted state exists.
//
// The floor to a whole second is deliberate, and is why the deadline is
// a second-resolution time point rather than a millisecond one.
// AddTimeItem stored "the current whole second plus the lifetime", so an
// item added part-way through a second expires up to a second early and
// the countdown the UI draws steps on the clock's second boundary rather
// than on the item's. That is preserved exactly; making it millisecond
// accurate would move every countdown in the description panel.
//-----------------------------------------------------------------------------

MTimeItemManager		*g_pTimeItemManager = NULL;

//-----------------------------------------------------------------------------
// The current time, floored to the whole second this class counts in
//-----------------------------------------------------------------------------
static TIMEITEM_DEADLINE
NowInSeconds()
{
	return std::chrono::floor<std::chrono::seconds>( MonotonicClock::Now() );
}

MTimeItemManager::MTimeItemManager()
{
	clear();
}

MTimeItemManager::~MTimeItemManager()
{
	clear();
}

bool	MTimeItemManager::IsExist(TYPE_OBJECTID objectID)
{
	return contains( objectID );
}

bool	MTimeItemManager::AddTimeItem(TYPE_OBJECTID objectID, DWORD time)
{
	RemoveTimeItem( objectID );

	// time is a lifetime, not a point in time: GCTimeLimitItemInfo sends
	// "seconds left on this item", so it is added to the current second
	// rather than compared with it. A DWORD of seconds is 136 years, and
	// the 64-bit rep behind the deadline holds all of it - the old DWORD
	// sum wrapped instead, which turned a very long lifetime into an
	// item that was already expired.
	const std::chrono::seconds d_lifetime( (std::chrono::seconds::rep)time );

	insert( TIMEITEM_MAP::value_type( objectID, NowInSeconds() + d_lifetime ) );

	return true;
}

bool	MTimeItemManager::RemoveTimeItem(TYPE_OBJECTID objectID)
{
	if(IsExist( objectID ) )
	{
		TIMEITEM_MAP::iterator itr = find( objectID );
		erase( itr );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Seconds left on an item, or zero once its deadline has been reached
//-----------------------------------------------------------------------------
std::chrono::seconds	MTimeItemManager::GetRemainingSeconds( TYPE_OBJECTID objectID )
{
	TIMEITEM_MAP::const_iterator c_itr = find( objectID );

	if( c_itr == end() )
		return std::chrono::seconds( 0 );

	// One clock read decides both the sign and the value. Each old
	// accessor read the clock twice - once for "is the current second
	// past the deadline", once for the subtraction - and the second read
	// could land one second later than the first, so on the very second
	// an item expired the unsigned subtraction went round to 0xFFFFFFFF
	// and the description panel could show 49710 days for one frame.
	const std::chrono::seconds d_left = (*c_itr).second - NowInSeconds();

	// Past the deadline, and exactly on it. The old code tested only
	// "the current second is past the deadline" and let the equal case
	// fall through to a subtraction that returned zero, so the answer is
	// the same one; a signed difference just says it in one place.
	if( d_left.count() <= 0 )
		return std::chrono::seconds( 0 );

	return d_left;
}

int		MTimeItemManager::GetDay( TYPE_OBJECTID objectID )
{
	if(! IsExist( objectID ) )
		return -1;

	return (int)( GetRemainingSeconds( objectID ).count() / 60 / 60 / 24 );
}

int		MTimeItemManager::GetHour( TYPE_OBJECTID objectID )
{
	if(! IsExist( objectID ) )
		return -1;

	// The hours left after the whole days GetDay reports, which is what
	// the modulus is for.
	return (int)( ( GetRemainingSeconds( objectID ).count() / 60 / 60 ) % 24 );
}

int		MTimeItemManager::GetMinute(TYPE_OBJECTID objectID )
{
	if(! IsExist( objectID ) )
		return -1;

	return (int)( ( GetRemainingSeconds( objectID ).count() / 60 ) % 60 );
}

int		MTimeItemManager::GetSecond(TYPE_OBJECTID objectID )
{
	if(! IsExist ( objectID ) )
		return -1;

	return (int)( GetRemainingSeconds( objectID ).count() % 60 );
}

bool	MTimeItemManager::IsExpired( TYPE_OBJECTID objectID )
{
	if( !IsExist( objectID ) )
		return true;

	// This used to ask the four accessors whether every field was zero,
	// which is true of exactly one remaining count - zero - and read the
	// clock four times to decide it. One read cannot disagree with
	// itself.
	return ( GetRemainingSeconds( objectID ).count() == 0 );
}
