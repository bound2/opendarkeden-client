// SocketImpl::close() runs twice on every player socket: Player's destructor
// closes the socket and then deletes it, and ~SocketImpl closes again. The
// second close must not release the descriptor number a second time, because
// by then the number may belong to another socket or file.
#include "test_framework.h"
#include "packet_stream_access.h"
#include "SocketAPI.h"
#include "SocketImpl.h"
#include "WebSocketTransport.h"

namespace {
bool IsOpen(const SocketImpl& socket)
{
	try {
		socket.getLinger();
		return true;
	} catch (Throwable&) {
		return false;
	}
}
}

TEST(SocketImpl, CloseInvalidatesTheDescriptor)
{
	EnsureSocketsInitialised();
	// Web mode has no descriptor; the transport probe covers that path.
	if (NetworkTransport::UsesWebSocket()) return;
	SocketImpl socket("127.0.0.1", 9);
	socket.create();
	CHECK(socket.isValid());
	socket.close();
	CHECK(!socket.isValid());
	CHECK(socket.getSOCKET() == INVALID_SOCKET);
	bool closedTwice = true;
	try { socket.close(); } catch (Throwable&) { closedTwice = false; }
	CHECK(closedTwice);
}

TEST(SocketImpl, SecondCloseLeavesAReusedDescriptorOpen)
{
	EnsureSocketsInitialised();
	if (NetworkTransport::UsesWebSocket()) return;
	SocketImpl first("127.0.0.1", 9);
	first.create();
	first.close();
	// POSIX hands out the lowest free descriptor, so this normally receives
	// the number just released; Windows often reuses the handle as well.
	SocketImpl second("127.0.0.1", 9);
	second.create();
	CHECK(IsOpen(second));
	try { first.close(); } catch (Throwable&) {}
	CHECK(IsOpen(second));
}
