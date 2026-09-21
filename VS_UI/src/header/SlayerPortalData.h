#ifndef SLAYER_PORTAL_DATA_H
#define SLAYER_PORTAL_DATA_H

#include "PortalFlag.h"
#include <array>
#include <vector>

class CRarFile;

// Map order is the six MAP_SPK_INDEX entries in C_VS_UI_SLAYER_PORTAL.
using SlayerPortalData = std::array<std::vector<UI_PORTAL_FLAG>, 6>;
bool ReadSlayerPortalData(CRarFile& file, SlayerPortalData& flags);

#endif
