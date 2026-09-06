#include "Client_PCH.h"
#include "SystemAvailabilities.h"
#include <algorithm>	// std::ranges::any_of / none_of, for the two filter scans
#include <istream>
#include <string>
#include <string_view>	// starts_with, for the script file's line prefixes
#include <string.h>
#include <stdio.h>

SystemAvailabilitiesManager *g_pSystemAvailableManager = NULL;

SystemAvailabilitiesManager::SystemAvailabilitiesManager()
{
	// Default 로 모두 가능 상태로 되어있다.
	for(int i = 0; i < m_Flag.size(); i++ )
		m_Flag.set(i);

	m_OpenDegree = 0xFF;
	m_LimitSkillLevel = 0xFF;
}

SystemAvailabilitiesManager::~SystemAvailabilitiesManager()
{
}

bool	SystemAvailabilitiesManager::ScriptFiltering(int scriptid, int answerid)
{
	for(int i = 0; i < SYSTEM_MAX; i++ )
	{
		if( m_Flag[i] )
			continue;

		const std::list<FilterScript> &Filter = m_ScriptFilter[(SystemKind)i];

		if( Filter.empty() )
			continue;

		if( CheckScript( Filter, scriptid, answerid ) == false )		// 해당 스크립트를 사용할 수 없으면 false 를 리턴
			return false;
	}	

	std::list<FilterScriptByDegree>::const_iterator itr = m_DegreeScriptFilter.begin();
	std::list<FilterScriptByDegree>::const_iterator enditr = m_DegreeScriptFilter.end();
	
	while( itr != enditr )
	{
		const FilterScriptByDegree &scr = *itr;

		if( scriptid == scr.scriptID && answerid == scr.answerID )
		{
			if( !ZoneFiltering( scr.zoneID ) )
				return false;
		}
		
		itr++;
	}
	return true;
}

bool	SystemAvailabilitiesManager::ZoneFiltering( int zoneID ) const
{
	if( m_OpenDegree >= MAX_OPEN_DEGREE )
		return true;

	for(int i = m_OpenDegree; i >= 0; i-- )
	{
		// A membership test over the degree's allowed zones: 90909 is
		// the row that allows every zone, so either it or the zone
		// asked about is enough. The hand written scan tested the two
		// in this order on each element and returned on the first hit,
		// which is what any_of over the same predicate does.
		const bool bAllowed = std::ranges::any_of( m_ZoneFilter[i],
				[zoneID]( int AllowZoneID )
				{
					return AllowZoneID == 90909 || zoneID == AllowZoneID;
				} );

		if( bAllowed )
			return true;
	}
	return false;
}

bool	SystemAvailabilitiesManager::CheckScript( const std::list<FilterScript>& List, int &scriptID, int& answerID ) const
{
	// A membership test over the filter rows: a script that is in the
	// list is blocked, and one that is not may be used. The scan this
	// replaces walked the same rows with the same predicate and
	// returned on the first hit.
	return std::ranges::none_of( List,
			[&]( const FilterScript& Script )
			{
				return Script.scriptID == scriptID &&
					   Script.answerID == answerID;
			} );		// not in the list means it may be used
}

bool	SystemAvailabilitiesManager::LoadFromStream(std::istream& in)
{
	if( !in.good() )
		return false;

	std::string line;
	char szLine[512];

	int key=-1, count=-1;

	std::list<FilterScript>			ScriptList;
	std::list<FilterScriptByDegree> ScriptListByDegree;
	std::list<int>					ZoneList;

	enum KIND_PARSE
	{
		SCRIPT_PARSE,
		ZONE_PARSE,
		DEGREE_SCRIPT_PARSE,

		PARSE_MAX
	};

	KIND_PARSE Kind = PARSE_MAX;

	while( std::getline( in, line ) )
	{
		// CRarFile::GetString used to hand out the line without its
		// newline; a CR left by a Windows file is dropped the same way.
		if( line.ends_with( '\r' ) )
			line.erase( line.size()-1 );
		strncpy( szLine, line.c_str(), sizeof(szLine)-1 );
		szLine[sizeof(szLine)-1] = '\0';

		// One view over the truncated copy the rest of the loop reads.
		// A view built from a char* measures it with strlen, so empty()
		// is the test the explicit strlen() call made, and it is still
		// the copy - not the std::string - that is measured, so a line
		// carrying an embedded null is as short here as it always was.
		const std::string_view	svLine( szLine );

		// '*' starts a key, ';' starts a comment.
		if( svLine.empty() )
			continue;

		if( svLine.starts_with( ';' ) )
			continue;

		if( svLine.starts_with( '*' ) )
		{
			sscanf(szLine+1,"%d %d",&key,&count);		// key 는 enum(SystemKind) 값.
			ScriptList.clear();
			count--;
			Kind = SCRIPT_PARSE;
			continue;
		}

		if( svLine.starts_with( 'Z' ) )
		{
			sscanf(szLine+1,"%d",&key);					// key 는 회차
			ZoneList.clear();
			Kind = ZONE_PARSE;
			continue;
		}
		
		if( svLine.starts_with( 'S' ) )
		{
			sscanf(szLine+1,"%d %d",&key,&count);		// key 는 무효-_-
			ScriptListByDegree.clear();
			count--;
			Kind = DEGREE_SCRIPT_PARSE;
			continue;
		}

		if( key != -1 )
		{
			switch( Kind )
			{
			case SCRIPT_PARSE :
				{
					FilterScript SCR;
					sscanf(szLine,"%d %d",&SCR.scriptID,&SCR.answerID);
					SCR.answerID--;
					ScriptList.push_back( SCR );
					count--;

					if( count < 0 )
					{
						m_ScriptFilter[(SystemKind)key] = ScriptList;
						ScriptList.clear();
						key = -1;
						Kind = PARSE_MAX;
					}
				}
				break;
			
			case ZONE_PARSE :
				{
					int FilterZoneID;
					sscanf(szLine,"%d",&FilterZoneID);
					if( FilterZoneID != 99999 )
						ZoneList.push_back( FilterZoneID );

					if( FilterZoneID == 99999 )
					{
						if( key >= 0 && key < MAX_OPEN_DEGREE )
							m_ZoneFilter[key] = ZoneList;
						ZoneList.clear();
						key = -1;
						Kind = PARSE_MAX;						
					}
				}
				break;
			case DEGREE_SCRIPT_PARSE :
				{
					FilterScriptByDegree SCR;
					sscanf(szLine,"%d %d %d",&SCR.scriptID,&SCR.answerID,&SCR.zoneID);
					SCR.answerID--;
					ScriptListByDegree.push_back( SCR );
					count--;

					if( count < 0 )
					{
						m_DegreeScriptFilter = ScriptListByDegree;
						ScriptListByDegree.clear();
						key = -1;
						Kind = PARSE_MAX;
					}
				}
				break;
			}			
		}
	}
	return true;
}
