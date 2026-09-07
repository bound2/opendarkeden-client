//--------------------------------------------------------------------------------
// 
// Filename    : GCChangeWeather.h 
// Written By  : reiot
// 
//--------------------------------------------------------------------------------

#ifndef __GC_CHANGE_WEATHER_H__
#define __GC_CHANGE_WEATHER_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"


//--------------------------------------------------------------------------------
//
// class GCChangeWeather;
//
//--------------------------------------------------------------------------------

class GCChangeWeather : public Packet {

public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_CHANGE_WEATHER; }
	
	// get packet's body size
	// *OPTIMIZATION HINT*
	// const static GCChangeWeatherPacketSize 를 정의, 리턴하라.
	PacketSize_t getPacketSize () const noexcept { return szWeather + szWeatherLevel; }

	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "GCChangeWeather"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif

public :

	Weather getWeather () const noexcept { return m_Weather; }
	void setWeather ( Weather weather ) noexcept { m_Weather = weather; }

	WeatherLevel_t getWeatherLevel () const noexcept { return m_WeatherLevel; }
	void setWeatherLevel ( WeatherLevel_t weatherLevel ) noexcept { m_WeatherLevel = weatherLevel; }

public :

	Weather m_Weather;

	WeatherLevel_t m_WeatherLevel;

};


//--------------------------------------------------------------------------------
//
// class GCChangeWeatherFactory;
//
// Factory for GCChangeWeather
//
//--------------------------------------------------------------------------------

class GCChangeWeatherFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new GCChangeWeather(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCChangeWeather"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_CHANGE_WEATHER; }

	// get packet's max body size
	// *OPTIMIZATION HINT*
	// const static GCChangeWeatherPacketSize 를 정의, 리턴하라.
	PacketSize_t getPacketMaxSize () const noexcept { return szWeather + szWeatherLevel; }

};


//--------------------------------------------------------------------------------
//
// class GCChangeWeatherHandler;
//
//--------------------------------------------------------------------------------

class GCChangeWeatherHandler {

public :

	// execute packet's handler
	static void execute ( GCChangeWeather * pPacket , Player * pPlayer );

};

#endif
