////////////////////////////////////////////////////////////////////////////////
// Filename    : CGStashDeposit.h 
// Written By  : 김성민
// Description : 
////////////////////////////////////////////////////////////////////////////////

#ifndef __CG_STASH_DEPOSIT_H__
#define __CG_STASH_DEPOSIT_H__

#include "Packet.h"
#include "PacketFactory.h"

////////////////////////////////////////////////////////////////////////////////
//
// class CGStashDeposit;
//
////////////////////////////////////////////////////////////////////////////////

class CGStashDeposit : public Packet 
{
public :
    void read ( SocketInputStream & iStream );
    void write ( SocketOutputStream & oStream ) const;
	PacketID_t getPacketID () const noexcept { return PACKET_CG_STASH_DEPOSIT; }
	PacketSize_t getPacketSize () const noexcept { return szGold; }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName () const { return "CGStashDeposit"; }
		std::string toString () const;
	#endif
	
public :
	Gold_t getAmount(void) const noexcept { return m_Amount;}
	void setAmount(Gold_t amount) noexcept { m_Amount = amount;}

private :
	Gold_t m_Amount;
};


////////////////////////////////////////////////////////////////////////////////
//
// class CGStashDepositFactory;
//
////////////////////////////////////////////////////////////////////////////////
class CGStashDepositFactory : public PacketFactory {
public :
	Packet * createPacket () { return new CGStashDeposit(); }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName () const { return "CGStashDeposit"; }
	#endif

	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_STASH_DEPOSIT; }
	PacketSize_t getPacketMaxSize () const noexcept { return szGold; }

};

////////////////////////////////////////////////////////////////////////////////
//
// class CGStashDepositHandler;
//
////////////////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CGStashDepositHandler {
		
	public :

		// execute packet's handler
		static void execute ( CGStashDeposit * pPacket , Player * player );
	};
#endif
#endif
