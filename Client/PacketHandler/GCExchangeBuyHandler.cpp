//////////////////////////////////////////////////////////////////////////////
// Filename    : GCExchangeBuyHandler.cpp
// Description : Client-side handler for GCExchangeBuy. Split out of
//               GCExchangeBuy.cpp (docs/RESTRUCTURING.md task 2.4): the
//               packet class compiles into the packetwire library, the
//               handler stays with the executable like every other one.
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "Gpackets/GCExchangeBuy.h"

#include "Player.h"

void GCExchangeBuyHandler::execute(GCExchangeBuy* pPacket, Player* pPlayer)
{
	__BEGIN_TRY

	// Client-side packet handling
	// This will be implemented to update the exchange UI with the buy result

	__END_CATCH
}
