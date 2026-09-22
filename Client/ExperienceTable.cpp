//---------------------------------------------------------------------------------
// ExperienceTable.cpp
//---------------------------------------------------------------------------------
#include "Client_PCH.h"
#include "ExperienceTable.h"
#include <limits>

namespace {
void LoadExperienceRows(std::ifstream& file, ExpTable& rows)
{
	int count = 0;
	if (!file.read(reinterpret_cast<char*>(&count), 4) || count < 0 ||
		count == (std::numeric_limits<int>::max)()) {
		file.setstate(std::ios::failbit);
		return;
	}
	// Each row is a four-byte level followed by two four-byte experience values.
	const std::streamoff position = file.tellg();
	if (position < 0) { file.setstate(std::ios::failbit); return; }
	file.seekg(0, std::ios::end);
	const std::streamoff end = file.tellg();
	if (!file.good() || end < position) { file.setstate(std::ios::failbit); return; }
	file.seekg(position, std::ios::beg);
	if (!file.good() || count > (end - position) / 12) {
		file.setstate(std::ios::failbit);
		return;
	}

	// File levels start at one; the unused zero row stays a default value.
	rows.Init(count + 1);
	for (int i = 0; i < count; ++i) {
		int level = 0;
		if (!file.read(reinterpret_cast<char*>(&level), 4)) return;
		ExpInfo value{};
		value.LoadFromFile(file);
		if (!file.good()) return;
		if (!rows.Set(level, value)) {
			file.setstate(std::ios::failbit);
			return;
		}
	}
}
}

//---------------------------------------------------------------------------------
// Global
//---------------------------------------------------------------------------------
ExperienceTable* g_pExperienceTable = NULL;

//---------------------------------------------------------------------------------
//
//					ExperienceTable
//
//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------
// constructor / destructor
//---------------------------------------------------------------------------------
ExperienceTable::ExperienceTable()
{
	// 냠냠..
}

ExperienceTable::~ExperienceTable()
{
	// 음냐.. 안해도 되는데.. 걍..
	Release();
}

//---------------------------------------------------------------------------------
// Release
//---------------------------------------------------------------------------------
void
ExperienceTable::Release()
{
	m_STRExp.Release();
	m_DEXExp.Release();
	m_INTExp.Release();
	m_VampireExp.Release();
	m_OustersExp.Release();
	m_SlayerRankExp.Release();
	m_VampireRankExp.Release();
	m_OustersRankExp.Release();
	m_PetExp.Release();
}

//---------------------------------------------------------------------------------
// LoadFromFileSTR
//---------------------------------------------------------------------------------
void		
ExperienceTable::LoadFromFileSTR(std::ifstream& file)
{
	LoadExperienceRows(file, m_STRExp);
}

//---------------------------------------------------------------------------------
// Load From File DEX
//---------------------------------------------------------------------------------
void		
ExperienceTable::LoadFromFileDEX(std::ifstream& file)
{
	LoadExperienceRows(file, m_DEXExp);
}

//---------------------------------------------------------------------------------
// Load From File INT
//---------------------------------------------------------------------------------
void		
ExperienceTable::LoadFromFileINT(std::ifstream& file)
{
	LoadExperienceRows(file, m_INTExp);
}

//---------------------------------------------------------------------------------
// Load From File Vampire
//---------------------------------------------------------------------------------
void		
ExperienceTable::LoadFromFileVampire(std::ifstream& file)
{
	LoadExperienceRows(file, m_VampireExp);
}

//---------------------------------------------------------------------------------
// Load From File Ousters
//---------------------------------------------------------------------------------
void		
ExperienceTable::LoadFromFileOusters(std::ifstream& file)
{
	LoadExperienceRows(file, m_OustersExp);
}

//---------------------------------------------------------------------------------
// Load From File Slayer Rank
//---------------------------------------------------------------------------------
void		
ExperienceTable::LoadFromFileSlayerRank(std::ifstream& file)
{
	LoadExperienceRows(file, m_SlayerRankExp);
}

//---------------------------------------------------------------------------------
// Load From File Vampire Rank
//---------------------------------------------------------------------------------
void		
ExperienceTable::LoadFromFileVampireRank(std::ifstream& file)
{
	LoadExperienceRows(file, m_VampireRankExp);
}

//---------------------------------------------------------------------------------
// Load From File Ousters Rank
//---------------------------------------------------------------------------------
void		
ExperienceTable::LoadFromFileOustersRank(std::ifstream& file)
{
	LoadExperienceRows(file, m_OustersRankExp);
}

//---------------------------------------------------------------------------------
// Load From File Ousters Rank
//---------------------------------------------------------------------------------
void		
ExperienceTable::LoadFromFilePetExp(std::ifstream& file)
{
	LoadExperienceRows(file, m_PetExp);
}

void
ExperienceTable::LoadFromFileAdvanceMent(std::ifstream& file)
{
	LoadExperienceRows(file, m_advanceSkillExp);
}



//---------------------------------------------------------------------------------
// Get STR Info
//---------------------------------------------------------------------------------
const ExpInfo&		
ExperienceTable::GetSTRInfo(int level) const
{
	return m_STRExp[level];
}

//---------------------------------------------------------------------------------
// Get DEX Info
//---------------------------------------------------------------------------------
const ExpInfo&		
ExperienceTable::GetDEXInfo(int level) const
{
	return m_DEXExp[level];
}

//---------------------------------------------------------------------------------
// Get INT Info
//---------------------------------------------------------------------------------
const ExpInfo&		
ExperienceTable::GetINTInfo(int level) const
{
	return m_INTExp[level];
}

//---------------------------------------------------------------------------------
// Get Vampire Info
//---------------------------------------------------------------------------------
const ExpInfo&		
ExperienceTable::GetVampireInfo(int level) const
{
	return m_VampireExp[level];
}

//---------------------------------------------------------------------------------
// Get Ousters Info
//---------------------------------------------------------------------------------
const ExpInfo&		
ExperienceTable::GetOustersInfo(int level) const
{
	return m_OustersExp[level];
}

//---------------------------------------------------------------------------------
// Get SlayerRank Info
//---------------------------------------------------------------------------------
const ExpInfo&		
ExperienceTable::GetRankInfo(int level, Race_t race) const
{
	switch(race)
	{
	case RACE_SLAYER:
		return m_SlayerRankExp[level];
		break;

	case RACE_VAMPIRE:
		return m_VampireRankExp[level];
		break;

	case RACE_OUSTERS:
		return m_VampireRankExp[level];
		break;
	}

	return m_SlayerRankExp[level];
}

//---------------------------------------------------------------------------------
// Get SlayerRank Info
//---------------------------------------------------------------------------------
const ExpInfo&		
ExperienceTable::GetSlayerRankInfo(int level) const
{
	return m_SlayerRankExp[level];
}

//---------------------------------------------------------------------------------
// Get VampireRank Info
//---------------------------------------------------------------------------------
const ExpInfo&		
ExperienceTable::GetVampireRankInfo(int level) const
{
	return m_VampireRankExp[level];
}

//---------------------------------------------------------------------------------
// Get OustersRank Info
//---------------------------------------------------------------------------------
const ExpInfo&		
ExperienceTable::GetOustersRankInfo(int level) const
{
	return m_OustersRankExp[level];
}

//---------------------------------------------------------------------------------
// Get OustersRank Info
//---------------------------------------------------------------------------------
const ExpInfo&		
ExperienceTable::GetPetExp(int level) const
{
	return m_PetExp[level];
}


//---------------------------------------------------------------------------------
// Get advanceMent Info
//---------------------------------------------------------------------------------
const ExpInfo&		
ExperienceTable::GetAdvanceMent(int level) const
{
	return m_advanceSkillExp[level];
}
