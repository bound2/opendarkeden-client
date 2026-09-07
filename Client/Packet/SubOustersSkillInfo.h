//----------------------------------------------------------------------
//
// Filename    : SubOustersSkillInfo.h
// Written By  :
// Description :
//
//----------------------------------------------------------------------

#ifndef __SUB_OUSTERS_SKILL_INFO_H__
#define __SUB_OUSTERS_SKILL_INFO_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------

class SubOustersSkillInfo {

public :

	// read data from socket input stream
	void read (SocketInputStream & iStream);

	// write data to socket output stream
	void write (SocketOutputStream & oStream) const;

	// get size of object
	uint getSize () const noexcept { return szSkillType + szExpLevel + szTurn + szTurn; }
	// get max size of object
	static uint getMaxSize () noexcept { return szSkillType + szExpLevel + szTurn + szTurn; }

#ifdef __DEBUG_OUTPUT__
	// get debug string
	std::string toString () const;
#endif

public :

	// get / set SkillType
	SkillType_t getSkillType() const noexcept { return m_SkillType; }
	void setSkillType(SkillType_t SkillType) noexcept { m_SkillType = SkillType; }

	// get /set Skill ExpLevel
	ExpLevel_t getExpLevel() const noexcept { return m_ExpLevel; }
	void setExpLevel( ExpLevel_t ExpLevel ) noexcept { m_ExpLevel = ExpLevel; }

	// get / set Turn
	Turn_t getSkillTurn() const noexcept { return m_Interval ; }
	void setSkillTurn(Turn_t SkillTurn) noexcept { m_Interval = SkillTurn; }

	// get / set CastingTime
	Turn_t getCastingTime() const noexcept { return m_CastingTime; }
	void setCastingTime(Turn_t CastingTime) noexcept { m_CastingTime = CastingTime; }

private :

	// 스킬 타입
	SkillType_t m_SkillType;

	// 스킬 레벨
	ExpLevel_t m_ExpLevel;

	// 한번쓰고 다음에 쓸 딜레이
	Turn_t m_Interval;

	// 캐스팅 타임
	Turn_t m_CastingTime;

};

#endif
