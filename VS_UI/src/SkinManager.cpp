#include "Client_PCH.h"
#include "SkinManager.h"
#include "RarFile.h"
#include "VS_UI_filepath.h"


#include <map>
#include <cctype>
#include <sstream>
#include <string>
#include <utility>

SkinManager *g_pSkinManager = NULL;

namespace {
const char* SkipWhitespace(const char* line)
{
	while (*line && std::isspace(static_cast<unsigned char>(*line))) ++line;
	return line;
}
}

InterfaceInformation::InterfaceInformation()
{
	m_ID = 0;
	m_PointList.clear();
	m_RectList.clear();
}

InterfaceInformation::~InterfaceInformation()
{
}

bool		InterfaceInformation::LoadFromLinePointList( const char *szLine )
{
	if (szLine == NULL) return false;
	POINT pt = {};
	std::istringstream input(szLine);
	if (!(input >> pt.x >> pt.y)) return false;
	
	m_PointList.push_back( pt );
	return true;
}

bool		InterfaceInformation::LoadFromLineRectList( const char *szLine )
{
	if (szLine == NULL) return false;
	RECT rect = {};
	std::istringstream input(szLine);
	if (!(input >> rect.left >> rect.top >> rect.right >> rect.bottom)) return false;

	m_RectList.push_back( rect );
	return true;
}

SkinManager::SkinManager()
{
	Init( INTERFACE_MAX );
}

SkinManager::~SkinManager()
{
}

bool			SkinManager::LoadInformation(const char *szFileName)
{
	CRarFile rarfile;
	rarfile.SetRAR(RPK_INFO,RPK_PASSWORD);
	rarfile.Open(szFileName);
	
	if( !rarfile.IsSet() )
		return false;
		
	SkinManager loaded;
	char szLine[256];

	std::map< std::string, int >	MapStringToKey;

	MapStringToKey["INFO"] = 0;
	MapStringToKey["GAME_MENU"] = 1;
	MapStringToKey["OPTION"] = 2;
	MapStringToKey["TITLE"] = 3;
	MapStringToKey["NEW_CHAR"] = 4;

	while( rarfile.GetString( szLine, 256 ) )
	{
		const char* line = SkipWhitespace(szLine);
		if (*line == ';' || *line == '\0') continue;

		if( *line == '*' )
		{
			std::string key, type;
			std::istringstream header(line + 1);
			if (!(header >> key)) return false;
			const auto itr = MapStringToKey.find(key);
			if(  itr != MapStringToKey.end() )
			{		
				if (!(header >> type)) return false;
				if (type == "POINT_LIST")
				{
					if (!loaded.LoadPointList(itr->second, &rarfile)) return false;
				}
				else if (type == "RECT_LIST")
				{
					if (!loaded.LoadRectList(itr->second, &rarfile)) return false;
				}
				else return false;
			}
		}
	}
	rarfile.Release();
	std::swap(m_Size, loaded.m_Size);
	std::swap(m_pTypeInfo, loaded.m_pTypeInfo);
	return true;
}

bool		SkinManager::LoadRectList( int k , void* rar )
{
	char szLine[256];
	
	CRarFile *rarfile = reinterpret_cast<CRarFile*>(rar);
	while( rarfile->GetString( szLine, 256 ) )
	{
		const char* line = SkipWhitespace(szLine);
		if (*line == ';' || *line == '\0') continue;
		
		if( *line == '*' )
		{
			std::string key;
			std::istringstream header(line + 1);
			if (!(header >> key)) return false;
			if (key == "END") return true;
			return false;
		}
		if (k < 0 || k >= INTERFACE_MAX || !m_pTypeInfo[k].LoadFromLineRectList(line))
			return false;
	}
	return true;
}

bool		SkinManager::LoadPointList(int k, void *rar )
{
	char szLine[256];
	CRarFile *rarfile = reinterpret_cast<CRarFile*>(rar);
	while( rarfile->GetString( szLine, 256 ) )
	{
		const char* line = SkipWhitespace(szLine);
		if (*line == ';' || *line == '\0') continue;
		
		if( *line == '*' )
		{
			std::string key;
			std::istringstream header(line + 1);
			if (!(header >> key)) return false;
			if (key == "END") return true;
			return false;
		}
		if (k < 0 || k >= INTERFACE_MAX || !m_pTypeInfo[k].LoadFromLinePointList(line))
			return false;
	}
	return true;
}
