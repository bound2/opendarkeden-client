//--------------------------------------------------------------------------------
// RequestServerPlayerManager.cpp
//--------------------------------------------------------------------------------
#include "Client_PCH.h"
#include "RequestServerPlayerManager.h"
#include "WireHost.h"
#include "DebugLog.h"

#include <chrono>
#ifdef PLATFORM_POSIX
#include <sys/select.h>
#endif

//--------------------------------------------------------------------------------
// Global
//--------------------------------------------------------------------------------
RequestServerPlayerManager*	g_pRequestServerPlayerManager = NULL;

//--------------------------------------------------------------------------------
//
// constructor / destructor
//
//--------------------------------------------------------------------------------
RequestServerPlayerManager::RequestServerPlayerManager()
{
	m_pServerSocket		= NULL;
	m_bUpdating			= false;

	InitializeCriticalSection(&m_Lock);
}

RequestServerPlayerManager::~RequestServerPlayerManager()
{
	Release();

	DeleteCriticalSection(&m_Lock);
}

//--------------------------------------------------------------------------------
// Release
//--------------------------------------------------------------------------------
void
RequestServerPlayerManager::Release()
{
	// Assigning an empty jthread asks the running one to stop and joins it,
	// so the socket it accepts on outlives it.
	m_RequestThread = std::jthread();

	// Cleared before the delete, whose close may throw, so it never dangles.
	ServerSocket* pServerSocket = m_pServerSocket;
	m_pServerSocket = NULL;
	try {
		delete pServerSocket;
	} catch (Throwable&) {
	}

	Lock();

	RequestServerPlayer_LIST::iterator iPlayer = m_listRequestServerPlayer.begin();

	while (iPlayer != m_listRequestServerPlayer.end())
	{
		RequestServerPlayer* pPlayer = *iPlayer;

		try {
			delete pPlayer;
		} catch (Throwable&) {
		}

		iPlayer++;
	}

	m_listRequestServerPlayer.clear();

	Unlock();
}

//--------------------------------------------------------------------------------
// Add RequestServerPlayer
//--------------------------------------------------------------------------------
bool
RequestServerPlayerManager::AddRequestServerPlayer(RequestServerPlayer* pRequestServerPlayer)
{
	if (1)//g_Mode==MODE_GAME)
	{
		Lock();

		// 넘 많을 경우는 더 이상 요청을 안 받도록 해야한다.
		if (m_listRequestServerPlayer.size() < static_cast<RequestServerPlayer_LIST::size_type>(Wire::MaxRequestService()))
		{
			// 일단 list에 넣어둔다.
			m_listRequestServerPlayer.push_back( pRequestServerPlayer );

			Unlock();
			return true;
		}
		else
		{
			pRequestServerPlayer->disconnect( UNDISCONNECTED );
			delete pRequestServerPlayer;
		}
		
		Unlock();		
	}
	/*
	else
	{
		pRequestServerPlayer->disconnect( UNDISCONNECTED );
		delete pRequestServerPlayer;
	}
	*/

	return false;	
}

//--------------------------------------------------------------------------------
// Disconnect
//--------------------------------------------------------------------------------
void		
RequestServerPlayerManager::Disconnect(const char* pName)
{
	Lock();

	// A peer's own CRDisconnect asks for this from inside Update(), while
	// that peer's processCommand() is still reading its packets; freeing it
	// here freed the object the dispatch returns into. Update() drops it
	// once its loop is done.
	if (m_bUpdating)
	{
		m_PendingDisconnects.push_back(pName);
		Unlock();
		return;
	}


	RequestServerPlayer_LIST::iterator iPlayer = m_listRequestServerPlayer.begin();
		
	while (iPlayer != m_listRequestServerPlayer.end())
	{
		RequestServerPlayer* pPlayer = *iPlayer;

		// Drop the player of that name. It leaves the list, and this call's
		// hold on the lock is released, before it is disconnected and freed.
		if (pPlayer->getName()==pName)
		{
			m_listRequestServerPlayer.erase( iPlayer );
			Unlock();

			pPlayer->disconnect(UNDISCONNECTED);
			delete pPlayer;
			return;
		}
		
		iPlayer++;
	}


	Unlock();
}

//--------------------------------------------------------------------------------
// Broadcast
//--------------------------------------------------------------------------------
void		
RequestServerPlayerManager::Broadcast(Packet* pPacket)
{
	Lock();

	// 나에게 접속한 모든 player들에게 packet을 전송한다.
	RequestServerPlayer_LIST::iterator iPlayer = m_listRequestServerPlayer.begin();
		
	while (iPlayer != m_listRequestServerPlayer.end())
	{
		RequestServerPlayer* pPlayer = *iPlayer;

		pPlayer->sendPacket( pPacket );
		
		iPlayer++;
	}

	Unlock();
}


//--------------------------------------------------------------------------------
// ProcessMode
//--------------------------------------------------------------------------------
void
RequestServerPlayerManager::ProcessMode(RequestServerPlayer* pPlayer)
{
	(void)pPlayer;
	// Upstream left this entire body commented out: a periodic
	// position broadcast that was never finished. Deleted with the
	// file's move into packetwire, because the dead lines named
	// g_pPlayer and g_CurrentTime and a library source naming an
	// executable global is what ratchet R4 exists to flag. Recover it
	// from history if the feature is ever wanted.
}


//--------------------------------------------------------------------------------
// Update
//--------------------------------------------------------------------------------
void
RequestServerPlayerManager::Update()
{
	Lock();

	if (m_listRequestServerPlayer.empty())
	{
		Unlock();
		return;
	}

	m_bUpdating = true;
	try {

		RequestServerPlayer_LIST::iterator iPlayer = m_listRequestServerPlayer.begin();

		while (iPlayer != m_listRequestServerPlayer.end())
		{
			RequestServerPlayer* pPlayer = *iPlayer;

			try {

				if (!pPlayer->getSocket()->isValid())
				{
					throw SocketException("sock error");
				}

				ProcessMode( pPlayer );

				pPlayer->processInput();
				pPlayer->processCommand();
				pPlayer->processOutput();

			} catch (NonBlockingIOException& t) {

				// 무시..
				DEBUG_ADD_ERR( t.toString().c_str() );

			} catch (Throwable &t) 	{

				DEBUG_ADD_ERR( t.toString().c_str() );


				// exception이 나면 무조건 잘라버린다. --;
				pPlayer->disconnect(UNDISCONNECTED);
				delete pPlayer;

				RequestServerPlayer_LIST::iterator iTemp = iPlayer;
				iPlayer++;

				m_listRequestServerPlayer.erase( iTemp );

				continue;
			}

			iPlayer++;
		}

	} catch (Throwable&t) {
		DEBUG_ADD_ERR( t.toString().c_str() );
	}

	m_bUpdating = false;

	// The disconnects the dispatch asked for, now that no peer is mid-read.
	std::vector<std::string> pending;
	pending.swap(m_PendingDisconnects);
	for (const std::string& name : pending)
	{
		// Outside the loop's own catches: one failure must not escape
		// Update() or keep the rest from being dropped.
		try {
			Disconnect(name.c_str());
		} catch (Throwable& t) {
			DEBUG_ADD_ERR(t.toString().c_str());
		}
	}

	Unlock();
}

//--------------------------------------------------------------------------------
// Init
//--------------------------------------------------------------------------------
void
RequestServerPlayerManager::Init(int port)
{
	// A thread an earlier Init started is stopped and joined before its
	// socket goes, as in Release.
	m_RequestThread = std::jthread();

	// Cleared before the delete, whose close may throw, and before the new
	// socket, whose bind may: Release must never see a freed one.
	ServerSocket* pServerSocket = m_pServerSocket;
	m_pServerSocket = NULL;
	delete pServerSocket;

	m_pServerSocket = new ServerSocket( port );

	// WaitRequest() waits at most 10 ms, so the thread sees a stop request
	// within one wait; a blocking accept could only be ended by killing it.
	m_pServerSocket->setNonBlocking();

	m_RequestThread = std::jthread([this](std::stop_token stop) {
		while (!stop.stop_requested())
			WaitRequest();
	});
}

//--------------------------------------------------------------------------------
// Wait Request
//--------------------------------------------------------------------------------
void
RequestServerPlayerManager::WaitRequest()
{
	// Check if socket is still valid (may be NULL during shutdown)
	if (m_pServerSocket == NULL)
	{
		return;
	}

	// Up to 10 ms for a peer to arrive. accept() is taken only for one: on an
	// empty queue it would throw a would-block exception every poll.
	const SOCKET listener = m_pServerSocket->getSOCKET();
	fd_set pending;
	FD_ZERO(&pending);
	FD_SET(listener, &pending);
	struct timeval wait = { 0, 10 * 1000 };
	const int ready = select((int)listener + 1, &pending, NULL, NULL, &wait);
	if (ready < 0)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));	// no spin on an error
	if (ready <= 0)
		return;

	Socket* pSocket = m_pServerSocket->accept();

	// accept() returns NULL if no connection or socket closed
	if (pSocket == NULL)
	{
		return;
	}

	// request에 등록
	RequestServerPlayer* pRequestServerPlayer = new RequestServerPlayer( pSocket );

	pRequestServerPlayer->setPlayerStatus( CPS_REQUEST_SERVER_BEGIN_SESSION );

	pSocket->setNonBlocking();

	if (AddRequestServerPlayer( pRequestServerPlayer ))
	{
		// g_pDebugMessage에 lock걸어야 한다. - -;
		//DEBUG_ADD_FORMAT("[Request] New Connection from %s:%d", pSocket->getHost().c_str(), pSocket->getPort());
	}
}
