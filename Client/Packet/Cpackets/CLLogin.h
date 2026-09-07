//////////////////////////////////////////////////////////////////////
// 
// Filename    : CLLogin.h 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CL_LOGIN_H__
#define __CL_LOGIN_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CLLogin;
//
// The first packet the client sends to the login server.
// The ID and password are encrypted.
//
//////////////////////////////////////////////////////////////////////

class CLLogin : public Packet {

public :
	
    // Reads data from the input stream (buffer) and initialises the packet.
    void read ( SocketInputStream & iStream );
		    
    // Sends the packet's binary image to the output stream (buffer).
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_CL_LOGIN; }
	
	// get packet's body size
	PacketSize_t getPacketSize () const;// { return szBYTE + m_ID.size() + szBYTE + m_Password.size(); }

#ifdef __DEBUG_OUTPUT__
	// get packet name
	std::string getPacketName () const { return "CLLogin"; }
	
	// get packet's debug string
	std::string toString () const;
#endif

public :

	// get/set player's id
	std::string getID () const { return m_ID; }
	void setID ( std::string id ) { m_ID = id; }

	// get/set player's password
	std::string getPassword () const { return m_Password; }
	void setPassword ( std::string password ) { m_Password = password; }

	const BYTE* getMacAddress() const noexcept { return m_MacAddress; }
	void setMacAddress( const BYTE* macAddress ) { memcpy( m_MacAddress, macAddress, 6 * sizeof(BYTE) ); }

	void SetLoginMode(BYTE n) { m_LoginMode = n;}

	// Netmarble accounts log in with a different wire layout (a 4-byte
	// ID length and no password). The sender sets this from the live
	// user information; the packet itself no longer reads the game
	// global, so it compiles into the packetwire library and can be
	// pinned by a test in either mode.
	bool isNetmarble () const noexcept { return m_bNetmarble; }
	void setNetmarble ( bool bNetmarble ) noexcept { m_bNetmarble = bNetmarble; }

	// A user-provided constructor forgoes value-initialisation's zero
	// fill, so the MAC is cleared here explicitly.
	CLLogin () : m_LoginMode(0) , m_bNetmarble(false) { memset( m_MacAddress, 0, sizeof(m_MacAddress) ); }

private :

	// player's id
	std::string m_ID;

	// player's password
	std::string m_Password;

	// Mac address
	BYTE m_MacAddress[6];

	// Login mode
	BYTE m_LoginMode;

	// Netmarble login layout (see setNetmarble)
	bool m_bNetmarble;

};


//////////////////////////////////////////////////////////////////////
//
// class CLLoginFactory;
//
// Factory for CLLogin
//
//////////////////////////////////////////////////////////////////////
class CLLoginFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new CLLogin(); }

	// get packet name
	std::string getPacketName () const { return "CLLogin"; }

	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CL_LOGIN; }

	// get packet's max body size
	// szID + ID(<=30) + szPassword + password(<=30, the server's cap; our
	// write() clamps at 20) + mac(6) + loginMode
	PacketSize_t getPacketMaxSize () const noexcept { return szBYTE + 30 + szBYTE + 30 + 6 + szBYTE; }

};


//////////////////////////////////////////////////////////////////////
//
// class CLLoginHandler;
//
//////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CLLoginHandler {

	public :

		// execute packet's handler
		static void execute ( CLLogin * pPacket , Player * pPlayer );

	};
#endif

#endif
