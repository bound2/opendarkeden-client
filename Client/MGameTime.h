//-----------------------------------------------------------------------------
// MGameTime.h
//-----------------------------------------------------------------------------
// Derives the current game time from a start time, at the ratio of game
// time to real time; the frame stamp is the start and the current time.
//
//-----------------------------------------------------------------------------

#ifndef __MGAMETIME_H__
#define	__MGAMETIME_H__

#include "MonotonicClock.h"

class MGameTime {
	public :
		MGameTime();
		~MGameTime();

		//-------------------------------------------------------------
		// the start time: y-m-d h:m:s
		//-------------------------------------------------------------
		void	SetStartTime(MonotonicClock::TimePoint time, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second);

		//-------------------------------------------------------------
		// how many times faster game time runs than real time
		//-------------------------------------------------------------
		void	SetTimeRatio(int ratio)		{ m_TimeRatio = ratio; }
		int		GetTimeRatio()				{ return m_TimeRatio; }

		//-------------------------------------------------------------
		// the current time
		//-------------------------------------------------------------
		void	SetCurrentTime(MonotonicClock::TimePoint time);

		//-------------------------------------------------------------
		// Get
		//-------------------------------------------------------------
		WORD	GetYear() const			{ return m_Year; }
		BYTE	GetMonth() const		{ return m_Month; }
		BYTE	GetDay() const			{ return m_Day; }
		BYTE	GetHour() const			{ return m_Hour; }
		BYTE	GetMinute() const		{ return m_Minute; }
		BYTE	GetSecond() const		{ return m_Second; }


	protected :
		//-------------------------------------------------------------
		// 기준 시간	
		//-------------------------------------------------------------
		// YYYY-MM-DD
		WORD		m_StartYear;
		BYTE		m_StartMonth;
		BYTE		m_StartDay;

		// HH:MM:SS
		BYTE		m_StartHour;
		BYTE		m_StartMinute;
		BYTE		m_StartSecond;

		//-------------------------------------------------------------
		// the frame stamp the start time was taken at
		//-------------------------------------------------------------
		MonotonicClock::TimePoint	m_StartTime;

		//-------------------------------------------------------------
		// 게임 시간과 실제 시간의 비율 : 실제시간*비율 = 게임시간
		//-------------------------------------------------------------
		int			m_TimeRatio;

		//-------------------------------------------------------------
		// 현재 시간
		//-------------------------------------------------------------
		// YYYY-MM-DD
		WORD		m_Year;
		BYTE		m_Month;
		BYTE		m_Day;

		// HH:MM:SS
		BYTE		m_Hour;
		BYTE		m_Minute;
		BYTE		m_Second;
};


extern MGameTime*	g_pGameTime;

#endif

