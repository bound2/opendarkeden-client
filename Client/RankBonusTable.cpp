//---------------------------------------------------------------------------------
// RankBonusTable.cpp
//---------------------------------------------------------------------------------
#include "Client_PCH.h"
#include "MString.h"
#include "RankBonusTable.h"
#include "RankBonusDef.h"

//---------------------------------------------------------------------------------
// Global
//---------------------------------------------------------------------------------
RankBonusTable* g_pRankBonusTable = NULL;

//---------------------------------------------------------------------------------
//
//					StatusInfo
//
//---------------------------------------------------------------------------------
RankBonusInfo::RankBonusInfo()
{
	m_type = 0;
	m_level = 0;
	m_race = RACE_SLAYER;	// 0 : Slayer  1 : Vampire
	m_skillIconID = 0;
	m_status = STATUS_NULL;
	m_point = 0;
}

void				
RankBonusInfo::LoadFromFile(std::ifstream& file)
{
	file.read((char*)&m_type, 2);
	m_Name.LoadFromFile(file);
	file.read((char*)&m_level, 1);
	file.read((char*)&m_race, 1);
	file.read((char*)&m_point, 4);
	file.read((char*)&m_skillIconID, 2);
}

