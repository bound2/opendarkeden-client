#include "Client_PCH.h"
#include "MTimeItemManager.h"

MTimeItemManager		*g_pTimeItemManager = NULL;

// The current time, floored to the whole second this class counts in.
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

	// time is a lifetime, not a point in time.
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

// Seconds left on an item, or zero once its deadline has been reached.
std::chrono::seconds	MTimeItemManager::GetRemainingSeconds( TYPE_OBJECTID objectID )
{
	TIMEITEM_MAP::const_iterator c_itr = find( objectID );

	if( c_itr == end() )
		return std::chrono::seconds( 0 );

	// One clock read decides both the sign and the value.
	const std::chrono::seconds d_left = (*c_itr).second - NowInSeconds();

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

	return ( GetRemainingSeconds( objectID ).count() == 0 );
}
