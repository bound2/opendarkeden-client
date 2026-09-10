//----------------------------------------------------------------------
// RequestUserManager.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "RequestUserManager.h"

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
RequestUserManager*		g_pRequestUserManager = NULL;

//----------------------------------------------------------------------
//
// constructor / destructor
//
//----------------------------------------------------------------------
RequestUserManager::RequestUserManager()
{
	InitializeCriticalSection(&m_Lock);
}

RequestUserManager::~RequestUserManager()
{
	Release();

	DeleteCriticalSection(&m_Lock);
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
RequestUserManager::Release()
{
	REQUEST_USER_MAP::iterator iUser = m_RequestUsers.begin();

	while (iUser != m_RequestUsers.end())
	{
		RequestUserInfo* pUser = iUser->second;

		delete pUser;

		iUser ++;
	}

	m_RequestUsers.clear();
}

//----------------------------------------------------------------------
// Has RequestUser
//----------------------------------------------------------------------
bool
RequestUserManager::HasRequestUser(const char* pName) const
{
	REQUEST_USER_MAP::const_iterator iUser = m_RequestUsers.find( std::string(pName) );

	if (iUser!=m_RequestUsers.end())
	{
		return true;
	}

	return false;
}

//----------------------------------------------------------------------
// Add RequestUser
//----------------------------------------------------------------------
// 이미 있다면.. ip를 바꿔준다.. 
// 이미 접속중이라면 접속해제..
//----------------------------------------------------------------------
void
RequestUserManager::AddRequestUser(const char* pName, const char* pIP, int UDPPort)
{
	RequestUserInfo* pUser = GetUserInfo( pName );

	if (pUser==NULL)
	{		
		// 없다면 정보 생성.
		pUser = new RequestUserInfo;

		pUser->Name = pName;
		pUser->IP	= pIP;
		pUser->UDPPort	= UDPPort;

		m_RequestUsers[pUser->Name] = pUser;
	}
	else
	{
		// IP가 달라진 경우..
		if (pUser->IP != pIP)
		{
			pUser->IP = pIP;
			pUser->UDPPort = UDPPort;
		}
	}	
}

//----------------------------------------------------------------------
// Get UserInfo
//----------------------------------------------------------------------
RequestUserInfo*
RequestUserManager::GetUserInfo(const char* pName) const
{
	REQUEST_USER_MAP::const_iterator iUser = m_RequestUsers.find( std::string(pName) );

	if (iUser!=m_RequestUsers.end())
	{
		return iUser->second;
	}

	return NULL;
}
