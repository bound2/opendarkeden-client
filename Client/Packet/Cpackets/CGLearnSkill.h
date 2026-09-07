//////////////////////////////////////////////////////////////////////////////
// Filename    : CGLearnSkill.h 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_LEARN_SKILL_H__
#define __CG_LEARN_SKILL_H__

#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class CGLearnSkill;
//////////////////////////////////////////////////////////////////////////////

class CGLearnSkill : public Packet 
{
public:
	void read ( SocketInputStream & iStream );
	void write ( SocketOutputStream & oStream ) const;
	PacketID_t getPacketID () const noexcept { return PACKET_CG_LEARN_SKILL; }
	PacketSize_t getPacketSize () const noexcept { return szSkillType+szSkillDomainType; }

#ifdef __DEBUG_OUTPUT__
	std::string getPacketName () const { return "CGLearnSkill"; }
	std::string toString () const;
#endif

public:
	SkillType_t getSkillType() const noexcept  { return m_SkillType; }
	void setSkillType( SkillType_t SkillType ) noexcept { m_SkillType = SkillType; }

	SkillDomainType_t getSkillDomainType() const noexcept { return m_DomainType;}
	void setSkillDomainType( SkillDomainType_t DomainType) noexcept { m_DomainType = DomainType;}

private:
	SkillType_t       m_SkillType;  // 기술의 종류
	SkillDomainType_t m_DomainType; // 기술의 도메인
};

//////////////////////////////////////////////////////////////////////
// class CGLearnSkillFactory;
//////////////////////////////////////////////////////////////////////
class CGLearnSkillFactory : public PacketFactory 
{
public:
	Packet * createPacket () { return new CGLearnSkill(); }

	std::string getPacketName () const { return "CGLearnSkill"; }
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_LEARN_SKILL; }
	PacketSize_t getPacketMaxSize () const noexcept { return szSkillType+szSkillDomainType; }
};


//////////////////////////////////////////////////////////////////////
// class CGLearnSkillHandler;
//////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
class CGLearnSkillHandler 
{
public:
	static void execute ( CGLearnSkill * pCGLearnSkill , Player * pPlayer );
	static void executeSlayerSkill ( CGLearnSkill * pCGLearnSkill , Player * pPlayer );
	static void executeVampireSkill ( CGLearnSkill * pCGLearnSkill , Player * pPlayer );
};
#endif
#endif
