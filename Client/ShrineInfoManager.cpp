
#include "Client_PCH.h"
#include "ShrineInfoManager.h"
#include "RarFile.h"
#include "Properties.h"

RegenTowerInfoManager *g_pRegenTowerInfoManager = NULL;

bool RegenTowerInfoManager::LoadRegenTowerInfo()
{
	CRarFile rarfile;
	
	rarfile.SetRAR( g_pFileDef->getProperty("FILE_INFO_DATA").c_str(), "darkeden" );
	rarfile.Open( g_pFileDef->getProperty("FILE_REGEN_TOWER_INFO").c_str() );
	
	if( !rarfile.IsSet() )
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
