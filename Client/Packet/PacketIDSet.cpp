//----------------------------------------------------------------------
//
// Filename    : PacketIDSet.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

#include "Client_PCH.h"
// include files
#include "PacketIDSet.h"
#include "PacketAssert.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
PacketIDSet::PacketIDSet ( PlayerStatus playerStatus , PacketIDSetType packetIDSetType ) 
	throw ()
: m_PacketIDSetType(packetIDSetType), m_PlayerStatus(playerStatus)
{
}

//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
PacketIDSet::~PacketIDSet () 
	throw ()
{
	m_PacketIDSet.clear();
}

//----------------------------------------------------------------------
// add packet id to set
//----------------------------------------------------------------------
void PacketIDSet::addPacketID ( PacketID_t packetID ) 
	throw ( DuplicatedException )
{
	__BEGIN_TRY

	Assert( m_PacketIDSetType != PIST_NONE );
	Assert( m_PacketIDSetType != PIST_ANY );

	std::pair<PACKET_ID_SET::iterator,bool> p = m_PacketIDSet.insert( packetID );

	// 이미 같은 키가 존재한다는 소리다.
	if ( ! p.second )
		throw DuplicatedException();

	__END_CATCH
}

//----------------------------------------------------------------------
// delete packet id from set
//----------------------------------------------------------------------
void PacketIDSet::deletePacketID ( PacketID_t packetID ) 
	throw ( NoSuchElementException )
{
	__BEGIN_TRY

	PACKET_ID_SET::iterator itr = m_PacketIDSet.find( packetID );

	// Nothing to erase; erasing end() would be undefined.
	if ( itr == m_PacketIDSet.end() )
		throw NoSuchElementException();

	m_PacketIDSet.erase(itr);

	__END_CATCH
}

//----------------------------------------------------------------------
// has packet id ?
//----------------------------------------------------------------------
bool PacketIDSet::hasPacketID ( PacketID_t packetID ) const
	throw ( NoSuchElementException , IgnorePacketException )
{
	__BEGIN_TRY

	if ( m_PacketIDSetType == PIST_NORMAL ) {

		// The ordinary case: true only when the id is in the set.
		return m_PacketIDSet.contains( packetID );

	} else if ( m_PacketIDSetType == PIST_ANY ) {

		// 그 어떤 패킷도 허용된다.
		return true;

	} else if ( m_PacketIDSetType == PIST_IGNORE_EXCEPT ) {

		// True when the packet is in the set. A packet that is not in
		// it has to be ignored.
		if ( m_PacketIDSet.contains( packetID ) ) {
			return true;
		} else {
			throw IgnorePacketException();
		}
	}

	// case of PIST_NONE
	return false;

	__END_CATCH
}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
std::string PacketIDSet::toString () const
	throw ()
{
	StringStream msg;

	msg << "PacketIDSet("
		<< "PlayerStatus:" << (int)m_PlayerStatus 
		<< "PacketID:";

	for ( PACKET_ID_SET::const_iterator itr = m_PacketIDSet.begin() ;
		  itr != m_PacketIDSet.end() ;
		  itr ++ ) {

		msg << (*itr) << " ";
	}

	msg << ")";

	return msg.toString();
}
