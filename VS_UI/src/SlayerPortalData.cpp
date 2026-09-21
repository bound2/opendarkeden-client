#include "Platform.h"
#include "SlayerPortalData.h"
#include "RarFile.h"
#include <cassert>

bool ReadSlayerPortalData(CRarFile& file, SlayerPortalData& flags)
{
	UI_PORTAL_FLAG temp_flag;
	int map_max = *(int *)file.Read(sizeof(int));
	assert(map_max == flags.size());
	for (size_t j = 0; j < flags.size(); ++j)
	{
		int size = *((int *)file.Read(sizeof(int)));
		assert(size != 0);
		for (int i = 0; i < size; ++i)
		{
			temp_flag.zone_id = *((int *)file.Read(sizeof(int)));
			temp_flag.x = *((int *)file.Read(sizeof(int)));
			temp_flag.y = *((int *)file.Read(sizeof(int)));
			temp_flag.portal_x = *((int *)file.Read(sizeof(int)));
			temp_flag.portal_y = *((int *)file.Read(sizeof(int)));
			assert(temp_flag.zone_id >= 0);
			assert(temp_flag.x >= 0);
			assert(temp_flag.x >= 0);
			assert(temp_flag.portal_x >= 0 && temp_flag.portal_x < 256);
			assert(temp_flag.portal_y >= 0 && temp_flag.portal_y < 256);
			flags[j].push_back(temp_flag);
		}
	}
	return true;
}
