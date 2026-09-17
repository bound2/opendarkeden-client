//----------------------------------------------------------------------
// SoundNode.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "SoundNode.h"

extern MonotonicClock::TimePoint	g_FrameNow;

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Set
//----------------------------------------------------------------------
void			
SOUND_NODE::Set(TYPE_SOUNDID sid, DWORD delay, int x, int y)
{
	m_PlayTime	= g_FrameNow + MonotonicClock::Millis(delay);
	m_SoundID	= sid;
	m_X			= x;
	m_Y			= y;
}
