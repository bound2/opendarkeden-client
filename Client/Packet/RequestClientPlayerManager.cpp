//--------------------------------------------------------------------------------
// RequestClientPlayerManager.cpp
//--------------------------------------------------------------------------------

#include "Client_PCH.h"
#include "RequestClientPlayerManager.h"
#include "WireHost.h"
#include "DebugLog.h"

// What this file used to reach past the wire layer for - the logged-in
// character (its name, world and race), the whisper queue, and the two
// managers a failed connection is reported to - is behind WireHost now
// (docs/RESTRUCTURING.md task 5.1, fifth slice). The in-game test was
// already there.

// Platform-specific threading includes. Off Windows the Win32 thread
// names this file uses (TerminateThread, GetExitCodeThread,
// GetCurrentThread, SetThreadPriority, CloseHandle, STILL_ACTIVE and
// the THREAD_PRIORITY_* constants) come from Platform.h's shim; this
// file used to carry a private pthread-flavoured copy of them, applied
// to a thread that platform_thread_create had made as an SDL_Thread*.
#ifdef PLATFORM_WINDOWS
	#include <windows.h>
	#include <process.h>
#elif defined(PLATFORM_POSIX)
	#include <unistd.h>
#endif

#include "Rpackets/CRConnect.h"
#include "Rpackets/CRWhisper.h"
#include "Rpackets/CRRequest.h"

//--------------------------------------------------------------------------------
// Global
//--------------------------------------------------------------------------------
RequestClientPlayerManager*	g_pRequestClientPlayerManager = NULL;

LONG					RequestConnectionThreadProc(LPVOID lpParameter);

//--------------------------------------------------------------------------------
//
// constructor / destructor
//
//--------------------------------------------------------------------------------
RequestClientPlayerManager::RequestClientPlayerManager()
{
	InitializeCriticalSection(&m_Lock);
}

RequestClientPlayerManager::~RequestClientPlayerManager()
{
	Release();

	DeleteCriticalSection(&m_Lock);
}

//--------------------------------------------------------------------------------
// Release
//--------------------------------------------------------------------------------
void
RequestClientPlayerManager::Release()
{
	Lock();

	//------------------------------------------------------------------------
	// thread
	//------------------------------------------------------------------------
	HANDLE_LIST::iterator iHandle = m_listConnectionThread.begin();

	while (iHandle != m_listConnectionThread.end())
	{
		HANDLE handle = *iHandle;

		TerminateThread( handle, 0 );
		CloseHandle( handle );

		iHandle ++;
	}

	m_listConnectionThread.clear();

	//------------------------------------------------------------------------
	// m_mapRequestClientPlayer
	//------------------------------------------------------------------------
	REQUESTCLIENTPLAYER_MAP::iterator iPlayer = m_mapRequestClientPlayer.begin();
		
	while (iPlayer != m_mapRequestClientPlayer.end())
	{
		RequestClientPlayer* pPlayer = iPlayer->second;

		try {
			delete pPlayer;
		} catch (Throwable&) {
		}
		
		iPlayer++;
	}

	m_mapRequestClientPlayer.clear();

	//------------------------------------------------------------------------
	// m_mapConnectionInfo
	//------------------------------------------------------------------------
	CONNECTION_INFO_MAP::iterator iInfo = m_mapConnectionInfo.begin();
		
	while (iInfo != m_mapConnectionInfo.end())
	{	
		CONNECTION_INFO* pInfo = iInfo->second;

		delete pInfo;

		iInfo++;
	}

	m_mapConnectionInfo.clear();
		

	Unlock();
}

//----------------------------------------------------------------------
// Remove Terminated Thread
//----------------------------------------------------------------------
void		
RequestClientPlayerManager::RemoveTerminatedThread()
{
	HANDLE_LIST::iterator iHandle = m_listConnectionThread.begin();

	DWORD exitCode;
		
	while (iHandle != m_listConnectionThread.end())
	{
		HANDLE handle = *iHandle;

		if (GetExitCodeThread(handle, &exitCode))
		{
			if (exitCode!=STILL_ACTIVE)
			{
				CloseHandle( handle );

				HANDLE_LIST::iterator iTemp = iHandle ++;

				m_listConnectionThread.erase( iTemp );

				continue;
			}
		}

		iHandle ++;
	}
}

//----------------------------------------------------------------------
// Connect
//----------------------------------------------------------------------
// IP의 컴퓨터에 Name이란 캐릭터에게 접속한다.
//
// 게임 돌아가는데 지장을 주지 않기 위해서
// thread를 하나 생성해서 접속을 시도한다.
// 
// 접속이 실패하면? 어떻하면 될까.. - -;;
//----------------------------------------------------------------------
void		
RequestClientPlayerManager::Connect(const char* pIP, const char* pRequestName, REQUEST_CLIENT_MODE requestMode)
{
	// 넘 많을 경우는 더 이상 접속을 안하도록 해야한다.
	if (m_mapRequestClientPlayer.size() >= Wire::MaxRequestService())
	{
		return;
	}

	if (HasConnection(pRequestName)
		|| HasTryingConnection(pRequestName))
	{
		return;
	}

	// Every peer listens on the same port. Upstream left a lookup of
	// the per-user port commented out here; it was never live.
	int port = 9650;

	// Build one and hand it to the thread.
	CONNECTION_INFO* pInfo = new CONNECTION_INFO;
	pInfo->name			= pRequestName;
	pInfo->ip			= pIP;
	pInfo->port			= port;
	pInfo->requestMode	= requestMode;

	DWORD dwChildThreadID;	// 의미 없당 -- ;


	//------------------------------------------------------------
	// 접속 시도
	//------------------------------------------------------------
	Lock();		// 같이 lock쓴다. - -;

	m_mapConnectionInfo[pInfo->name] = pInfo;
	
#ifdef PLATFORM_WINDOWS
	HANDLE hConnectionThread = CreateThread(NULL,
									0,	// default stack size
									(LPTHREAD_START_ROUTINE)RequestConnectionThreadProc,
									pInfo,
									NULL,
									&dwChildThreadID);

	// priority는 낮게
	SetThreadPriority(hConnectionThread, THREAD_PRIORITY_LOWEST);	
#else
	// Non-Windows: Use platform_thread_create
	HANDLE hConnectionThread = (HANDLE)platform_thread_create(
		(platform_thread_func_t)RequestConnectionThreadProc,
		pInfo
	);
	(void)dwChildThreadID; // unused on non-Windows
#endif

	// 나중에 지울 수 있게 추가해둔다.
	m_listConnectionThread.push_back( hConnectionThread );

	Unlock();
}

//----------------------------------------------------------------------
// Send Packet
//----------------------------------------------------------------------
bool		
RequestClientPlayerManager::SendPacket(const char* pName, Packet* pPacket)
{
	Lock();

	REQUESTCLIENTPLAYER_MAP::const_iterator iPlayer = m_mapRequestClientPlayer.find( std::string(pName) );
		
	if (iPlayer != m_mapRequestClientPlayer.end())
	{
		RequestClientPlayer* pPlayer = iPlayer->second;

		pPlayer->sendPacket( pPacket );

		Unlock();
		return true;
	}

	Unlock();
	return false;
	
}

//----------------------------------------------------------------------
// Has Connection
//----------------------------------------------------------------------
bool	
RequestClientPlayerManager::HasConnection(const char* pRequestName)
{
	Lock();

	std::string name = pRequestName;

	// 연결된 경우
	REQUESTCLIENTPLAYER_MAP::const_iterator iPlayer = m_mapRequestClientPlayer.find( name );
		
	if (iPlayer != m_mapRequestClientPlayer.end())
	{
		Unlock();		
		return true;		
	}
	
	Unlock();
	return false;	
}

//----------------------------------------------------------------------
// Has TryingConnection
//----------------------------------------------------------------------
bool	
RequestClientPlayerManager::HasTryingConnection(const char* pRequestName)
{
	Lock();

	std::string name = pRequestName;

	// 접속 시도 중인 경우
	CONNECTION_INFO_MAP::const_iterator iInfo = m_mapConnectionInfo.find( name );
		
	if (iInfo != m_mapConnectionInfo.end())
	{	
		Unlock();
		return true;
	}

	Unlock();
	return false;	
}

//----------------------------------------------------------------------
// Disconnect
//----------------------------------------------------------------------
void		
RequestClientPlayerManager::Disconnect(const char* pRequestName)
{
	Lock();

	REQUESTCLIENTPLAYER_MAP::iterator iPlayer = m_mapRequestClientPlayer.find( std::string(pRequestName) );
		
	if (iPlayer != m_mapRequestClientPlayer.end())
	{
		RequestClientPlayer* pPlayer = iPlayer->second;

		try {

			m_mapRequestClientPlayer.erase( iPlayer );
			delete pPlayer;				

		} catch (Throwable& t)	{
			DEBUG_ADD_ERR( t.toString().c_str() );
		}

		Unlock();
		return;		
	}

	Unlock();
}

//--------------------------------------------------------------------------------
// Add RequestClientPlayer
//--------------------------------------------------------------------------------
bool
RequestClientPlayerManager::AddRequestClientPlayer(RequestClientPlayer* pRequestClientPlayer)
{
	Lock();	// 구차나서 같은 락을.. - -;

	bool bAdd = false;

	std::string serverName = pRequestClientPlayer->getRequestServerName();

	if (Wire::InGameMode())
	{
		// Too many connections should refuse further ones; the test
		// is commented out upstream and Connect() enforces the limit.
		//if (m_mapRequestClientPlayer.size() < Wire::MaxRequestService())
		{
			//------------------------------------------------------------
			// Put it in the list.
			//------------------------------------------------------------
			m_mapRequestClientPlayer[pRequestClientPlayer->getRequestServerName()] = pRequestClientPlayer;

			pRequestClientPlayer->setPlayerStatus( CPS_REQUEST_CLIENT_BEGIN_SESSION );

			bAdd = true;
		}
	}
	else
	{
		pRequestClientPlayer->disconnect( UNDISCONNECTED );
		delete pRequestClientPlayer;
	}
	
	
	CONNECTION_INFO_MAP::iterator iInfo = m_mapConnectionInfo.find( serverName );
		
	if (iInfo != m_mapConnectionInfo.end())
	{	
		CONNECTION_INFO* pInfo = iInfo->second;

		delete pInfo;
		m_mapConnectionInfo.erase( iInfo );
	}

	Unlock();

	return bAdd;	
}

//--------------------------------------------------------------------------------
// Add RequestClientPlayer
//--------------------------------------------------------------------------------
bool
RequestClientPlayerManager::RemoveRequestClientPlayer(const char* pRequestName)
{
	Lock();

	REQUESTCLIENTPLAYER_MAP::iterator iPlayer = m_mapRequestClientPlayer.find( std::string(pRequestName) );
		
	if (iPlayer != m_mapRequestClientPlayer.end())
	{
		RequestClientPlayer* pPlayer = iPlayer->second;

		try {

			pPlayer->disconnect(UNDISCONNECTED);
			delete pPlayer;

		} catch (Throwable& t) {
			DEBUG_ADD_ERR( t.toString().c_str() );
		}

		Unlock();
		return true;
	}	

	Unlock();

	return false;
}

//--------------------------------------------------------------------------------
// Remove ConnectionInfo
//--------------------------------------------------------------------------------
void
RequestClientPlayerManager::RemoveConnectionInfo(const char* pName)
{
	Lock();

	CONNECTION_INFO_MAP::iterator iInfo = m_mapConnectionInfo.find( pName );

	if (iInfo != m_mapConnectionInfo.end())
	{	
		CONNECTION_INFO* pInfo = iInfo->second;

		delete pInfo;

		m_mapConnectionInfo.erase( iInfo );
	}

	Unlock();
}

//--------------------------------------------------------------------------------
// Process Mode
//--------------------------------------------------------------------------------
// What the request mode asks for: the first packet on the connection.
//--------------------------------------------------------------------------------
void
RequestClientPlayerManager::ProcessMode(RequestClientPlayer* pRequestClientPlayer)
{
	//------------------------------------------------------------
	// The first request packet
	//------------------------------------------------------------
	switch (pRequestClientPlayer->getRequestMode())
	{
		//------------------------------------------------------------
		// Keep the connection open for whatever follows.
		//------------------------------------------------------------
		case REQUEST_CLIENT_MODE_NULL :
		{
			if (pRequestClientPlayer->getPlayerStatus()==CPS_REQUEST_CLIENT_BEGIN_SESSION)
			{
				CRConnect _CRConnect;
				_CRConnect.setRequestServerName( pRequestClientPlayer->getRequestServerName().c_str() );
				_CRConnect.setRequestClientName( Wire::CharacterName().c_str() );

				// Wait for the connect to be acknowledged.
				pRequestClientPlayer->setPlayerStatus( CPS_REQUEST_CLIENT_AFTER_SENDING_CONNECT );

				pRequestClientPlayer->sendPacket( &_CRConnect );
			}
		}
		break;

		//------------------------------------------------------------
		// Sending a whisper.
		//------------------------------------------------------------
		case REQUEST_CLIENT_MODE_WHISPER :
		{
			if (pRequestClientPlayer->getPlayerStatus()==CPS_REQUEST_CLIENT_BEGIN_SESSION)
			{
				const std::string& requestServerName = pRequestClientPlayer->getRequestServerName();

				// Anything waiting for this peer?
				if (Wire::HasWhisperMessage( requestServerName ))
				{
					const std::list<WHISPER_MESSAGE>* pMessageList = Wire::GetWhisperMessages( requestServerName );

					if (pMessageList)
					{
						// Build the CRWhisper and send it.
						CRWhisper _CRWhisper;

						_CRWhisper.setName( Wire::CharacterName() );
						_CRWhisper.setTargetName( requestServerName );

						_CRWhisper.setRace( Wire::CharacterRace() );

						_CRWhisper.setWorldID( Wire::CharacterWorldID() );

						std::list<WHISPER_MESSAGE>::const_iterator iMessage = pMessageList->begin();

						// Every message
						while (iMessage != pMessageList->end())
						{
							_CRWhisper.addMessage ( *iMessage );

							iMessage ++;
						}

						pRequestClientPlayer->sendPacket( &_CRWhisper );

						pRequestClientPlayer->setPlayerStatus( CPS_REQUEST_CLIENT_NORMAL );
					}

					Wire::RemoveWhisperMessage( requestServerName );
				}
			}
		}
		break;

		//------------------------------------------------------------
		// Profile을 요청할 때..
		//------------------------------------------------------------
		case REQUEST_CLIENT_MODE_PROFILE :
		{
			if (pRequestClientPlayer->getPlayerStatus()==CPS_REQUEST_CLIENT_BEGIN_SESSION)
			{
				const char* pRequestServerName = pRequestClientPlayer->getRequestServerName().c_str();

				CRRequest crRequest;
				crRequest.setCode( CR_REQUEST_FILE_PROFILE );
				crRequest.setRequestName( pRequestServerName );

				pRequestClientPlayer->sendPacket( &crRequest );

				pRequestClientPlayer->setPlayerStatus( CPS_REQUEST_CLIENT_NORMAL );
			}
		}
		break;
	}
}

//--------------------------------------------------------------------------------
// Update
//--------------------------------------------------------------------------------
void
RequestClientPlayerManager::Update()
{
	Lock();

	if (m_mapRequestClientPlayer.empty())
	{
		Unlock();
		return;
	}

	RemoveTerminatedThread();

	try {

		REQUESTCLIENTPLAYER_MAP::iterator iPlayer = m_mapRequestClientPlayer.begin();

		while (iPlayer != m_mapRequestClientPlayer.end())
		{
			RequestClientPlayer* pPlayer = iPlayer->second;

			if(pPlayer != NULL)
			{
				try {

					if (!pPlayer->getSocket()->isValid())
					{
						throw SocketException("sock error");
					}

					ProcessMode(pPlayer);

					pPlayer->processInput();
					pPlayer->processCommand();
					pPlayer->processOutput();

				} catch (Throwable &t) 	{

					DEBUG_ADD_ERR( t.toString().c_str() );

					// Any exception drops the connection. (Upstream
					// also meant to drop the peer's connection to us
					// here, and left that commented out.)
					pPlayer->disconnect(UNDISCONNECTED);
					delete pPlayer;

					REQUESTCLIENTPLAYER_MAP::iterator iTemp = iPlayer;
					iPlayer++;

					m_mapRequestClientPlayer.erase( iTemp );

					continue;
				}
			}

			iPlayer++;
		}

	} catch (Throwable&t) {
		DEBUG_ADD_ERR( t.toString().c_str() );
	}

	Unlock();
}

//--------------------------------------------------------------------------------
// RequestConnectionThreadProc
//--------------------------------------------------------------------------------
LONG					
RequestConnectionThreadProc(LPVOID lpParameter)
{
	CONNECTION_INFO* pInfo = (CONNECTION_INFO*)lpParameter;
	
	HANDLE hConnectionThread = GetCurrentThread();

	Socket * pSocket = NULL;

	//for (int i=0; i<5; i++)
	{
		try {
			pSocket = new Socket( pInfo->ip, pInfo->port );

			// try to connect to server
			pSocket->connect();

			// make nonblocking socket
			pSocket->setNonBlocking();

			// make no-linger socket
			pSocket->setLinger(0);

			// create player
			RequestClientPlayer* pPlayer = new RequestClientPlayer(pSocket);
			pSocket = NULL;

			pPlayer->setRequestServerName( pInfo->name.c_str() );
			pPlayer->setRequestServerIP( pInfo->ip.c_str() );
			pPlayer->setRequestMode( pInfo->requestMode );

			if (g_pRequestClientPlayerManager!=NULL)
			{
				SetThreadPriority(hConnectionThread, THREAD_PRIORITY_NORMAL);

				g_pRequestClientPlayerManager->AddRequestClientPlayer( pPlayer );

				SetThreadPriority(hConnectionThread, THREAD_PRIORITY_LOWEST);
			}
			else
			{
			}

		} catch (Throwable& t) {

			DEBUG_ADD_FORMAT_ERR("[RequestClientPlayerManager] Can't Connect %s:%s(%d)", pInfo->name.c_str(), pInfo->ip.c_str(), pInfo->port);
			DEBUG_ADD(t.toString().c_str());

			if (pSocket!=NULL)
			{
				delete pSocket;
			}

			//------------------------------------------------------
			// What a failed connection means depends on why it was
			// wanted.
			//------------------------------------------------------
			switch (pInfo->requestMode)
			{
				//------------------------------------------------------
				// A whisper that could not be delivered directly.
				//------------------------------------------------------
				case REQUEST_CLIENT_MODE_WHISPER :

					SetThreadPriority(hConnectionThread, THREAD_PRIORITY_NORMAL);

					// Forget the peer's address so the next whisper
					// asks the server for it again, and count the
					// attempt against the queued messages - the queue
					// falls back to the game server after the third.
					Wire::RemoveRequestUserLater( pInfo->name );
					Wire::TryToSendWhisperMessage( pInfo->name );

					SetThreadPriority(hConnectionThread, THREAD_PRIORITY_LOWEST);
				break;

				//------------------------------------------------------
				// A profile that could not be fetched.
				//------------------------------------------------------
				case REQUEST_CLIENT_MODE_PROFILE :
					Wire::RemoveProfileRequire( pInfo->name );
				break;
			}

			// Drop the CONNECTION_INFO
			g_pRequestClientPlayerManager->RemoveConnectionInfo(pInfo->name.c_str());
		}
	}

	//delete pInfo;

	return 0L;
}
