//////////////////////////////////////////////////////////////////////
// 
// Filename    : LCLoginOK.h 
// Written By  : Reiot
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __LC_LOGIN_OK_H__
#define __LC_LOGIN_OK_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class LCLoginOK;
//
// 로그인서버가 클라이언트에게 로그인 성공을 알려주는 패킷이다.
//
//////////////////////////////////////////////////////////////////////

class LCLoginOK : public Packet {

public:

    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_LC_LOGIN_OK; }
	
	// get packet body size
	// *OPTIMIZATION HINT*
	// const static LCLoginOKPacketSize 를 정의, 리턴하라.
	PacketSize_t getPacketSize() const;
	
	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName() const { return "LCLoginOK"; }

		// get packet's debug std::string
		std::string toString() const { return "LCLoginOK"; }
	#endif

	// get / set GoreLevel
	bool isAdult() const noexcept { return m_isAdult; }
	void setAdult(bool isAdult) noexcept { m_isAdult = isAdult; }

	bool isFamily() const noexcept { return m_bFamily; }
	void setFamily(bool isFamily) noexcept { m_bFamily = isFamily; }

	BYTE getStat() const noexcept { return m_Stat; }
	void setStat(BYTE Stat) noexcept { m_Stat = Stat; }

	WORD getLastDays() const noexcept { return m_LastDays; }
	void setLastDays( WORD day ) noexcept { m_LastDays = day; }

private :

	// 고어 레벨 : 현재 플레이어가 미성년자 인가?
	// true일 경우 성인
	// false 일 경우 미성년자
	bool m_isAdult;
	
	// Family 요금제인가?
	bool m_bFamily;

	// 서버의 상태
	BYTE m_Stat;

	WORD m_LastDays;
};


//////////////////////////////////////////////////////////////////////
//
// class LCLoginOKFactory;
//
// Factory for LCLoginOK
//
//////////////////////////////////////////////////////////////////////

class LCLoginOKFactory : public PacketFactory {

public:
	
	// create packet
	Packet* createPacket() { return new LCLoginOK(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName() const { return "LCLoginOK"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_LC_LOGIN_OK; }

	// get packet's max body size
	PacketSize_t getPacketMaxSize() const noexcept { return szBYTE + szBYTE + szBYTE + szWORD; }
	
};


//////////////////////////////////////////////////////////////////////
//
// class LCLoginOKHandler;
//
//////////////////////////////////////////////////////////////////////

class LCLoginOKHandler {

public:

	// execute packet's handler
	static void execute(LCLoginOK* pPacket, Player* pPlayer);

};

#endif
