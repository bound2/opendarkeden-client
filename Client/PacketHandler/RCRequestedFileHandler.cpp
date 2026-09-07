//////////////////////////////////////////////////////////////////////
//
// Filename    : RCRequestedFileHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Rpackets/RCRequestedFile.h"
#include "RequestClientPlayer.h"
#include "RequestFileManager.h"
#include "ClientDef.h"
#include "Packet/Properties.h"

//////////////////////////////////////////////////////////////////////
//
// The peer is another game client, so the wire filename is fully
// untrusted, and ReceiveFileInfo creates, truncates, renames and
// deletes whatever path it is given. Rather than trust the peer's path
// and enumerate every way it could escape, we ignore its directory
// entirely: only the leaf name is kept and it is re-rooted under our
// own profile directory (below), so a hostile peer can write nothing
// outside it. This validator therefore only has to vet a bare leaf
// name -- no separators, only the two profile extensions the sender
// legitimately transfers, and none of the Win32 aliasing traps.
//
//////////////////////////////////////////////////////////////////////
static std::string requestFileBaseName(const std::string& path)
{
	size_t sep = path.find_last_of("/\\");
	return (sep == std::string::npos) ? path : path.substr(sep+1);
}

static bool endsWithNoCase(const std::string& s, const char* suffix)
{
	size_t n = strlen(suffix);
	if (s.size() < n)
		return false;
	for (size_t i=0; i<n; i++)
	{
		if (tolower((unsigned char)s[s.size()-n+i]) != tolower((unsigned char)suffix[i]))
			return false;
	}
	return true;
}

static bool isSafeProfileBaseName(const std::string& base)
{
	if (base.empty() || base[0] == '.')
		return false;

	// A leaf name carries no separators, drive colon, or ".." run.
	if (base.find('/')  != std::string::npos
		|| base.find('\\') != std::string::npos
		|| base.find(':')  != std::string::npos
		|| base.find("..") != std::string::npos)
		return false;

	// StartReceive patches the extension via rfind("."), and only profile
	// sprites and their indexes are ever transferred.
	if (!endsWithNoCase(base, ".spk") && !endsWithNoCase(base, ".spki"))
		return false;

	// A trailing dot or space is stripped by the Win32 filesystem, and a
	// reserved device base name (CON, NUL, COM1, ...) opens the device no
	// matter what extension follows it.
	char lastCh = base[base.size()-1];
	if (lastCh == '.' || lastCh == ' ')
		return false;

	std::string stem = base.substr(0, base.find('.'));
	for (size_t i=0; i<stem.size(); i++)
		stem[i] = toupper((unsigned char)stem[i]);

	static const char* const reserved[] =
	{
		"CON", "PRN", "AUX", "NUL",
		"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
		"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
	};
	for (size_t r=0; r<sizeof(reserved)/sizeof(reserved[0]); r++)
	{
		if (stem == reserved[r])
			return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////
void RCRequestedFileHandler::execute ( RCRequestedFile * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	
#ifdef __GAME_CLIENT__

	RequestClientPlayer* pRequestClientPlayer = dynamic_cast<RequestClientPlayer*>( pPlayer );

	if (pRequestClientPlayer!=NULL
		&& g_pRequestFileManager!=NULL)
	{
		RequestReceiveInfo* pInfo = new RequestReceiveInfo( pRequestClientPlayer->getRequestServerName().c_str() );
				
		int listNum = pPacket->getListNum();

		for (int i=0; i<listNum; i++)
		{
			RCRequestedFileInfo* pFileInfo = pPacket->popInfo();

			if (pFileInfo!=NULL)
			{
				// Keep only the leaf name and re-root it under our own
				// profile directory, so the peer controls the filename but
				// never the location. Dropping a single file would
				// desynchronise the transfer stream, so a hostile or
				// undeliverable name aborts the whole exchange; pInfo owns the
				// ReceiveFileInfos started so far and their destructors close
				// the open streams, so it must be freed before the throw.
				const std::string baseName = requestFileBaseName( pFileInfo->getFilename() );

				std::string localPath;
				try
				{
					localPath = g_pFileDef->getProperty("DIR_PROFILE");
				}
				catch (...)
				{
					localPath.clear();
				}

				if (localPath.empty() || !isSafeProfileBaseName( baseName ))
				{
					DEBUG_ADD_FORMAT("[Error] RCRequestedFile: unsafe filename from peer: %s", pFileInfo->getFilename().c_str());
					delete pFileInfo;
					delete pInfo;
					throw DisconnectException("unsafe requested filename");
				}

				localPath += "\\";
				localPath += baseName;

				ReceiveFileInfo* pReceiveFileInfo = new ReceiveFileInfo( localPath.c_str(), pFileInfo->getRequestFileType() );

				pInfo->AddReceiveFileInfo( pReceiveFileInfo );

				pReceiveFileInfo->StartReceive( pFileInfo->getFileSize() );

				delete pFileInfo;
			}
		}
		
		if (g_pRequestFileManager->AddMyRequest(pInfo))
		{
			// 받기 시작할 준비를 한다.
			//pInfo->StartReceive( pFileInfo->getFileSize() );
		}
		else
		{
			//pRequestClientPlayer->disconnect( UNDISCONNECTED );
			throw DisconnectException("can't add myRequest");
		}
	}

#endif

	__END_CATCH
}
