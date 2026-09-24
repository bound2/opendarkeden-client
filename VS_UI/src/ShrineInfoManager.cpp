
#include "Client_PCH.h"
#include "ShrineInfoManager.h"
#include "RarFile.h"
#include "Properties.h"
#include <cstring>

RegenTowerInfoManager *g_pRegenTowerInfoManager = NULL;

bool RegenTowerInfoManager::LoadRegenTowerInfo()
{
	CRarFile rarfile;
	
	rarfile.SetRAR( g_pFileDef->getProperty("FILE_INFO_DATA").c_str(), "darkeden" );
	rarfile.OpenText( g_pFileDef->getProperty("FILE_REGEN_TOWER_INFO").c_str() );
	
	if( !rarfile.IsSet() )
		return false;
	// A byte embedded in the file must not masquerade as a line terminator.
	// OpenText caps allocation before this table's smaller parsing budget.
	if (rarfile.GetRemainingSize() > RegenTowerInfoManager::MaxTextBytes ||
		std::memchr(rarfile.GetFilePointer(), '\0', rarfile.GetRemainingSize()) != nullptr)
		return false;

	const RegenTowerLineReader reader{
		&rarfile,
		[](void* context, char* line, int capacity) {
			return static_cast<CRarFile*>(context)->GetString(line, capacity);
		}
	};
	const bool loaded = LoadRegenTowerInfoLines(reader);
	rarfile.Release();
	return loaded;
}
