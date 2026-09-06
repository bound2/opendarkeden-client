//////////////////////////////////////////////////////////////////////////////
// Filename    : CLQueryCharacterName.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CLQueryCharacterName.h"

void CLQueryCharacterName::read (SocketInputStream & iStream) 
	 throw (ProtocolException , Error)
{
	__BEGIN_TRY

	// read player id
	BYTE szCharacterName;

	iStream.read(szCharacterName);

	if (szCharacterName == 0)
		throw InvalidProtocolException("szCharacterName == 0");

	if (szCharacterName > 20)
		throw InvalidProtocolException("too long CharacterName length");

	iStream.read(m_CharacterName , szCharacterName);

	__END_CATCH
}
		    
void CLQueryCharacterName::write (SocketOutputStream & oStream) const 
     throw (ProtocolException , Error)
{
	__BEGIN_TRY

	// write player id
	// The cap runs on the std::string's own size, BEFORE the narrowing
	// to the BYTE that goes on the wire: (BYTE)276 is 20, so a
	// 276-character name used to pass this test and then go out whole
	// behind a length byte claiming 20.
	if (m_CharacterName.size() > 20)
		throw InvalidProtocolException("too long CharacterName length");

	const BYTE szCharacterName = (BYTE)m_CharacterName.size();

	if (szCharacterName == 0)
		throw InvalidProtocolException("empty CharacterName");

	oStream.write(szCharacterName);

	oStream.write(std::span<const char>(m_CharacterName.data(), szCharacterName));

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
	std::string CLQueryCharacterName::toString () const
		throw ()
	{
		__BEGIN_TRY
			
		StringStream msg;
		msg << "CLQueryCharacterName("
			<< "CharacterName:" << m_CharacterName 
			<< ")";
		return msg.toString();
			
		__END_CATCH
	}
#endif