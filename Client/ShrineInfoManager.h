#pragma once

#include "CTypeTable.h"
#include <vector>

class RegenTowerInfo
{
public :
	static constexpr int MaxCount = 256; // CGSelectRegenZone carries a BYTE id.
	static constexpr int MapWidth = 128, MapHeight = 256;
	RegenTowerInfo() { num = -1; zoneID = -1; x=0,y=0; owner = -1; }
	~RegenTowerInfo() {}
	// Four nonnegative integers, with optional surrounding whitespace/comment.
	// Invalid input preserves all fields, including the current owner.
	bool	LoadFromLine(const char *szLine);
	// The shrine minimap has exactly three rectangles, for zones 71..73.
	bool	IsValid() const;

	void	LoadFromFile(std::ifstream&) { }
	void	SaveToFile(std::ofstream&) { }
	
	int		num;
	int		zoneID;	
	int		x,y;
	int		owner;
};

// Resource access stays with the executable; the table consumes its lines.
struct RegenTowerLineReader {
	// Return one NUL-terminated line, consuming any clipped remainder. False
	// means EOF; report read failures by throwing. Input must not contain NUL.
	void* context = nullptr;
	bool (*GetString)(void*, char*, int) = nullptr;
};

class RegenTowerInfoManager : public CTypeTable< RegenTowerInfo >
{
public :	
	static constexpr size_t MaxTextBytes = 1024 * 1024;
	RegenTowerInfoManager();

	bool			LoadRegenTowerInfo();
	// Complete unique rows 0..count-1 replace the table together. Missing,
	// invalid, duplicate, excessive or truncated rows preserve the old table.
	bool			LoadRegenTowerInfoLines(const RegenTowerLineReader& reader);
	
private :
};

// Process-owned table; resource access stays in the executable.
extern RegenTowerInfoManager *g_pRegenTowerInfoManager;
