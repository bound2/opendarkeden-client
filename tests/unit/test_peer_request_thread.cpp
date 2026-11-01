//----------------------------------------------------------------------
// test_peer_request_thread.cpp
//----------------------------------------------------------------------
// RequestServerPlayerManager::Init starts the thread that accepts peers
// and Release stops it. Release must stop the thread and wait for it
// before it frees the socket the thread accepts on, and promptly: the
// Windows build waited two seconds on a blocking accept and then killed
// the thread with TerminateThread, and the other builds never waited.
//----------------------------------------------------------------------
#include "test_framework.h"
#include "packet_stream_access.h"
#include "RequestServerPlayer.h"
#include "RequestServerPlayerManager.h"
#include "ServerSocket.h"
#include "Socket.h"

#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <string.h>

#include <chrono>
#include <memory>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

// The listener's port and the peer count, read the way the thread writes them.
// Peers end their sessions before the manager frees them, as a game's do.
struct Manager : RequestServerPlayerManager
{
	~Manager() { EndSessions(); }
	void Release()
	{
		EndSessions();
		RequestServerPlayerManager::Release();
	}
	void EndSessions()
	{
		Lock();
		for (RequestServerPlayer* pPlayer : m_listRequestServerPlayer)
			pPlayer->setPlayerStatus(CPS_END_SESSION);
		Unlock();
	}
	uint Port()
	{
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
#ifdef _WIN32
		int len = (int)sizeof(addr);
#else
		socklen_t len = (socklen_t)sizeof(addr);
#endif
		if (getsockname(m_pServerSocket->getSOCKET(), (struct sockaddr*)&addr, &len) != 0)
			return 0;
		return ntohs(addr.sin_port);
	}
	int Peers()
	{
		Lock();
		const int count = GetSize();
		Unlock();
		return count;
	}
};

// Connects a peer and waits up to two seconds for the thread to take it.
std::unique_ptr<Socket> Connect(Manager& manager)
{
	const int before = manager.Peers();
	std::unique_ptr<Socket> client(new Socket("127.0.0.1", manager.Port()));
	client->connect();
	for (int i = 0; i < 200 && manager.Peers() == before; i++)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	return client;
}

Clock::duration TimeRelease(Manager& manager)
{
	const Clock::time_point start = Clock::now();
	manager.Release();
	return Clock::now() - start;
}

} // namespace

TEST(PeerRequestThread, TheThreadAcceptsAPeerAndReleaseStopsItPromptly)
{
	EnsureSocketsInitialised();
	Manager manager;
	manager.Init(0);
	CHECK(manager.Port() != 0);
	std::unique_ptr<Socket> peer = Connect(manager);
	CHECK_EQ(1, manager.Peers());

	CHECK(TimeRelease(manager) < std::chrono::seconds(1));
	CHECK_EQ(0, manager.Peers());
}

TEST(PeerRequestThread, ReleaseStopsAThreadWaitingWithNoPeerPromptly)
{
	EnsureSocketsInitialised();
	Manager manager;
	manager.Init(0);
	// Long enough for the thread to be waiting on the listener.
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	CHECK(TimeRelease(manager) < std::chrono::seconds(1));
}

TEST(PeerRequestThread, InitAfterReleaseStartsAThreadThatAcceptsAgain)
{
	EnsureSocketsInitialised();
	Manager manager;
	manager.Init(0);
	manager.Release();
	manager.Init(0);
	std::unique_ptr<Socket> peer = Connect(manager);
	CHECK_EQ(1, manager.Peers());
}

TEST(PeerRequestThread, InitAgainReplacesTheThreadAndItsListener)
{
	EnsureSocketsInitialised();
	Manager manager;
	manager.Init(0);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	manager.Init(0);
	std::unique_ptr<Socket> peer = Connect(manager);
	CHECK_EQ(1, manager.Peers());
	CHECK(TimeRelease(manager) < std::chrono::seconds(1));
}

TEST(PeerRequestThread, DestroyingTheManagerStopsItsThread)
{
	EnsureSocketsInitialised();
	std::unique_ptr<Manager> manager(new Manager);
	manager->Init(0);
	std::unique_ptr<Socket> peer = Connect(*manager);
	CHECK_EQ(1, manager->Peers());
	manager.reset();
	// A thread left running would now read the freed manager (ASan).
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
