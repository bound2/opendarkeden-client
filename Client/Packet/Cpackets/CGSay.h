//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGSay.h 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_SAY_H__
#define __CG_SAY_H__

// include files

//#ifdef __GAME_SERVER__
//#include "GamePlayer.h"
//#endif

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CGSay;
//
// 클라이언트가 서버에게 보내는 Say 패킷이다.
// 내부에 Say String 만을 데이타 필드로 가진다.
//
//////////////////////////////////////////////////////////////////////

class Player;
class Creature;
class GamePlayer;

class CGSay : public Packet {

public:

	// The longest message this packet carries; read() and write() both
	// throw above it rather than truncating.
	enum { MAX_MESSAGE_SIZE = 128 };

    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_CG_SAY; }
	
	// get packet's body size
	PacketSize_t getPacketSize() const { return szuint + szBYTE + m_Message.size(); }

#ifdef __DEBUG_OUTPUT__
	// get packet name
	std::string getPacketName() const { return "CGSay"; }
	
	// get packet's debug std::string
	std::string toString() const;
#endif

	// get/set text color
	uint getColor() const noexcept { return m_Color; }
	void setColor( uint color ) noexcept { m_Color = color; }

	// get/set chatting message
	const std::string& getMessage() const noexcept { return m_Message; }
	void setMessage(const std::string & msg) { m_Message = msg; }
	

private :
	
	// text color
	uint m_Color;

	// chatting message
	std::string m_Message;
	
};


//////////////////////////////////////////////////////////////////////
//
// class CGSayFactory;
//
// Factory for CGSay
//
//////////////////////////////////////////////////////////////////////

class CGSayFactory : public PacketFactory {

public:
	
	// create packet
	Packet* createPacket() { return new CGSay(); }

	// get packet name
	std::string getPacketName() const { return "CGSay"; }
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_SAY; }

	// get packet's max body size
	PacketSize_t getPacketMaxSize() const noexcept { return szuint + szBYTE + CGSay::MAX_MESSAGE_SIZE; }

};


#ifndef __GAME_CLIENT__
//////////////////////////////////////////////////////////////////////
//
// class CGSayHandler;
//
//////////////////////////////////////////////////////////////////////

class CGSayHandler {

public:

	// execute packet's handler
	static void execute(CGSay* pPacket, Player* pPlayer);

#ifdef __GAME_SERVER__

	static void opExecute( Creature* pCreature, GamePlayer* pPlayer, std::string msg, int i );

	// for guild test
	static void opzone( std::string msg, int i );
	static void opguild( std::string msg, int i );

	// 전쟁 시스템 관련 
	static void opcombat( GamePlayer* pPlayer, std::string msg, int i );

	// set 이벤트 아이템 확률
	static void opset( GamePlayer* pPlayer, std::string msg, int i );

	static void opview( GamePlayer* pPlayer, std::string msg, int i );

	// save
	static void opsave(GamePlayer* pPlayer, std::string msg, int i);

	// wall
	static void opwall(GamePlayer* pPlayer, std::string msg, int i);

	// Shutdown
	static void opshutdown(GamePlayer* pPlayer, std::string msg, int i);

	// kick
	static void opkick(GamePlayer* pPlayer, std::string msg, int i);

	// mute
	static void opmute(GamePlayer* pPlayer, std::string msg, int i);

	// freezing
	static void opfreezing(GamePlayer* pPlayer, std::string msg, int i);

	// deny
	static void opdeny(GamePlayer* pPlayer, std::string msg, int i);

	// info
	static void opinfo(GamePlayer* pPlayer, std::string msg, int i);

	// trace
	static void optrace(GamePlayer* pPlayer, std::string msg, int i);

	// warp
	static void opwarp(GamePlayer* pPlayer, std::string msg, int i);

	// create
	static void opcreate(GamePlayer* pPlayer, std::string msg, int i);

	// grant
	static void opgrant(GamePlayer* pPlayer, std::string msg, int i);

	// recall
	static void oprecall(GamePlayer* pPlayer, std::string msg, int i);

	// mrecall
	static void opmrecall(GamePlayer* pPlayer, std::string msg, int i);

	// user
	static void opuser(GamePlayer* pPlayer, std::string msg, int i);

	// summon
	static void opsummon(GamePlayer* pPlayer, std::string msg, int i);

	// notice 
	static void opnotice(GamePlayer* pPlayer, std::string msg, int i);

	// pay 
	static void oppay(GamePlayer* pPlayer, std::string msg, int i);

	// world 
	static void opworld(GamePlayer* pPlayer, std::string msg, int i, bool bSameWorldOnly);

	// command 
	static void opcommand(GamePlayer* pPlayer, std::string msg, int i);
#endif

};
#endif

#endif
