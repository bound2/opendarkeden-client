#include "Client_PCH.h"
#include "ShowTimeChecker.h"

ShowTimeChecker::ShowTimeChecker()
{
	Loop = false;

	// MinDelay ~ MaxDelay 사이에는 꼭 한 번 소리가 나야한다.
	MinDelay = 60000;
	MaxDelay = 60000;

	// StartHour부터 EndHour 사이에만 소리가 난다. (0~24시면 종일?)
	StartHour = 0;
	EndHour = 24;

	NextPlayTime = MonotonicClock::TimePoint();
}

ShowTimeChecker::~ShowTimeChecker()
{
}

void					
ShowTimeChecker::SaveToFile(std::ofstream& file)
{
	file.write((const char*)&Loop, 1);

	// MinDelay ~ MaxDelay 사이에는 꼭 한 번 소리가 나야한다.
	file.write((const char*)&MinDelay, 4);
	file.write((const char*)&MaxDelay, 4);
	
	// StartHour부터 EndHour 사이에만 소리가 난다. (0~24시면 종일?)
	file.write((const char*)&StartHour, 1);
	file.write((const char*)&EndHour, 1);
}

void					
ShowTimeChecker::LoadFromFile(std::ifstream& file)
{
	BYTE loop = 0;
	file.read(reinterpret_cast<char*>(&loop), 1);
	if (loop > 1) file.setstate(std::ios::failbit);
	Loop = loop != 0;

	// MinDelay ~ MaxDelay 사이에는 꼭 한 번 소리가 나야한다.
	file.read((char*)&MinDelay, 4);
	file.read((char*)&MaxDelay, 4);
	
	// StartHour부터 EndHour 사이에만 소리가 난다. (0~24시면 종일?)
	file.read((char*)&StartHour, 1);
	file.read((char*)&EndHour, 1);
}
