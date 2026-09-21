#ifndef SLAYER_PORTAL_DATA_H
#define SLAYER_PORTAL_DATA_H

#include "PortalFlag.h"
#include <array>
#include <vector>

class CRarFile;

// Map order is the six MAP_SPK_INDEX entries in C_VS_UI_SLAYER_PORTAL.
using SlayerPortalData = std::array<std::vector<UI_PORTAL_FLAG>, 6>;
// Reads the remaining raw file as six nonempty maps of little-endian int32s.
// At most 65536 flags per map; zone IDs fit uint16 and destinations fit uint8.
// Screen positions are nonnegative int32s; the UI checks its sprite dimensions.
// Success replaces flags and consumes the file. Failure preserves both.
bool ReadSlayerPortalData(CRarFile& file, SlayerPortalData& flags);

#endif
