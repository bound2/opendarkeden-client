#include "ShrineInfoManager.h"
#include <cstdio>
#include <cstring>

void RegenTowerInfo::LoadFromLine(char *szLine)
{
	if( szLine == NULL )
		return;

	sscanf( szLine,"%d %d %d %d",&num, &zoneID,&x,&y);
	owner = -1;
}

RegenTowerInfoManager::RegenTowerInfoManager()
{
}

bool RegenTowerInfoManager::LoadRegenTowerInfoLines(const RegenTowerLineReader& reader)
{
	if (reader.GetString == nullptr) return false;
	char szLine[512];
	bool bInit = false;

	while( reader.GetString( reader.context, szLine, 512 ) )
	{
		if( szLine[0] == ';' )
			continue;

		if( szLine[0] == '*' && bInit == false )
		{
			int n;
			sscanf(szLine+1,"%d",&n);

			Init( n );
			bInit = true;
			continue;
		}

		if( strlen(szLine) <= 0 )
			continue;

		int num;
		sscanf(szLine,"%d",&num);

		if( num >= 0 && num < GetSize() )
			m_pTypeInfo[num].LoadFromLine( szLine );
	}
	return true;
}
