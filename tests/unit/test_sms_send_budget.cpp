//----------------------------------------------------------------------
// test_sms_send_budget.cpp
//----------------------------------------------------------------------
//
// CGSMSSend's stated maximum holds the largest packet its writer
// accepts. The factory's getPacketMaxSize once counted the caller's
// number as MAX_RECEVIER_NUM (5) bytes instead of MAX_NUMBER_LENGTH
// (11), so write() could emit 113 bytes against a stated 108; the
// server budgets the caller at MAX_NUMBER_LENGTH, 114 in all, and so
// does the inventory line tests/wire-layout.txt pins now. The bytes on
// the wire do not change.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"		// EnsureSocketsInitialised

#include "Cpackets/CGSMSSend.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"

#include <string>

TEST(SMSSend, TheLargestPacketTheWriterAcceptsFitsTheStatedMaximum)
{
	CGSMSSend packet;
	for (int i = 0; i < MAX_RECEVIER_NUM; i++)
		packet.addString(std::string(MAX_NUMBER_LENGTH, (char)('0' + i)));
	packet.setCallerNumber(std::string(MAX_NUMBER_LENGTH, '9'));
	// write() refuses a message of MAX_MESSAGE_LENGTH bytes or more.
	packet.setMessage(std::string(MAX_MESSAGE_LENGTH - 1, 'm'));

	Socket socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream out(&socket);
	packet.write(out);

	// What write() emits is what getPacketSize() declares, and it fits.
	CHECK_EQ(packet.getPacketSize(), (PacketSize_t)out.length());
	CHECK(packet.getPacketSize() <= CGSMSSendFactory().getPacketMaxSize());
}
