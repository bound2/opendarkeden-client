//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGTryJoinGuild.h 
// Written By  :
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_TRY_JOIN_GUILD_H__
#define __CG_TRY_JOIN_GUILD_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CGTryJoinGuild;
//
//////////////////////////////////////////////////////////////////////

class CGTryJoinGuild : public Packet
{
public:
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_CG_TRY_JOIN_GUILD; }
	
	// get packet's body size
	PacketSize_t getPacketSize() const noexcept { return szGuildID + szGuildMemberRank; }

#ifdef __DEBUG_OUTPUT__
	// get packet name
	std::string getPacketName() const { return "CGTryJoinGuild"; }

	// get packet's debug std::string
	std::string toString() const;
#endif

	// get/set Guild ID
	GuildID_t getGuildID() const noexcept { return m_GuildID; }
	void setGuildID( GuildID_t GuildID ) noexcept { m_GuildID = GuildID; }

	// get/set Guild Member Rank
	GuildMemberRank_t getGuildMemberRank() const noexcept { return m_GuildMemberRank; }
	void setGuildMemberRank( GuildMemberRank_t GuildMemberRank ) noexcept { m_GuildMemberRank = GuildMemberRank; }


private :

	// Guild ID
	GuildID_t m_GuildID;

	// Guild Member Rank
	GuildMemberRank_t m_GuildMemberRank;
	
};


//////////////////////////////////////////////////////////////////////
//
// class CGTryJoinGuildFactory;
//
// Factory for CGTryJoinGuild
//
//////////////////////////////////////////////////////////////////////

class CGTryJoinGuildFactory : public PacketFactory {

public:
	
	// constructor
	CGTryJoinGuildFactory() {}
	
	// destructor
	virtual ~CGTryJoinGuildFactory() {}

	
public:
	
	// create packet
	Packet* createPacket() { return new CGTryJoinGuild(); }

	// get packet name
	std::string getPacketName() const { return "CGTryJoinGuild"; }
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_TRY_JOIN_GUILD; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szGuildID + szGuildMemberRank; }
};

#ifndef __GAME_CLIENT__
//////////////////////////////////////////////////////////////////////
//
// class CGTryJoinGuildHandler;
//
//////////////////////////////////////////////////////////////////////

class CGTryJoinGuildHandler {

public:

	// execute packet's handler
	static void execute(CGTryJoinGuild* pCGTryJoinGuild, Player* pPlayer);

};
#endif

#endif
