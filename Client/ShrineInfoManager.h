#pragma once

#pragma warning(disable:4786)

#include "CTypeTable.h"
#include <vector>

class RegenTowerInfo
{
public :
	RegenTowerInfo() { zoneID = -1; x=0,y=0; owner = -1; }
	~RegenTowerInfo() {}
	void	LoadFromLine(char *szLine);

	void	LoadFromFile(std::ifstream& file) { }
	void	SaveToFile(std::ofstream& file) { }
	
	int		num;
	int		zoneID;	
	int		x,y;
	int		owner;
};

// Resource access stays with the executable; the table consumes its lines.
struct RegenTowerLineReader {
	void* context = nullptr;
	bool (*GetString)(void*, char*, int) = nullptr;
};

class RegenTowerInfoManager : public CTypeTable< RegenTowerInfo >
{
public :	
	RegenTowerInfoManager();

	bool			LoadRegenTowerInfo();
	bool			LoadRegenTowerInfoLines(const RegenTowerLineReader& reader);
	
private :
};

// Process-owned table; resource access stays in the executable.
extern RegenTowerInfoManager *g_pRegenTowerInfoManager;
