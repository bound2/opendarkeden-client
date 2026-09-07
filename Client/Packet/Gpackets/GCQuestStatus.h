//////////////////////////////////////////////////////////////////////////////
// Filename    : GCMonsterKillQuestStatus.h 
// Written By  : elca@ewestsoft.com
// Description : 
// 기술이 성공했을때 보내는 패킷을 위한 클래스 정의
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_MONSTER_KILL_QUEST_STATUS_H__
#define __GC_MONSTER_KILL_QUEST_STATUS_H__

#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class GCMonsterKillQuestStatus;
// 게임서버에서 클라이언트로 자신의 기술이 성공을 알려주기 위한 클래스
//////////////////////////////////////////////////////////////////////////////

class GCQuestStatus : public Packet 
{
public:
	GCQuestStatus();
	~GCQuestStatus();
	
public:
    void read(SocketInputStream & iStream);
    void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_GC_QUEST_STATUS; }
	PacketSize_t getPacketSize() const noexcept { return szWORD + szWORD + szDWORD; }

#ifdef __DEBUG_OUTPUT__
	std::string getPacketName() const { return "GCQuestStatus"; }
	std::string toString() const;
#endif

public:
	WORD getQuestID() const noexcept { return m_QuestID; }
	void setQuestID(WORD e) noexcept { m_QuestID = e; }
	
	WORD getCurrentNum() const noexcept { return m_CurrentNum; }
	void setCurrentNul(WORD n) noexcept { m_CurrentNum = n; }

	DWORD getRemainTime() const noexcept { return m_Time; }
	void setRemainTime(DWORD d) noexcept { m_Time = d; }
	
private :
	WORD m_QuestID;
	WORD m_CurrentNum;
	DWORD m_Time;
};


//////////////////////////////////////////////////////////////////////////////
// class GCMonsterKillQuestStatusFactory;
//////////////////////////////////////////////////////////////////////////////

class GCQuestStatusFactory : public PacketFactory 
{
public :
	GCQuestStatusFactory() {}
	virtual ~GCQuestStatusFactory() {}
	
public:
	Packet* createPacket() { return new GCQuestStatus(); }
#ifdef __DEBUG_OUTPUT__
	std::string getPacketName() const { return "GCQuestStatus"; }
#endif
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_QUEST_STATUS; }
	PacketSize_t getPacketMaxSize() const noexcept { return szWORD + szWORD + szDWORD; }
};

//////////////////////////////////////////////////////////////////////////////
// class GCMonsterKillQuestStatusHandler;
//////////////////////////////////////////////////////////////////////////////

class GCQuestStatusHandler 
{
public:
	static void execute(GCQuestStatus* pGCQuestStatus, Player* pPlayer);

};

#endif
