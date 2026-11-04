// Where mobs spawn. What the kinds of mobs and petals are is in Catalog.h.
#pragma once
#include <cstdint>

#include "Catalog.h"

namespace game {

using namespace catalog;

inline bool isPetal(int type) {
    return type >= 1 && type < kServerPetals;
}

struct SpawnEntry {
    float weight;
    uint8_t mob;
};
struct SpawnTable {
    float total;
    int count;
    SpawnEntry entries[16];
};

extern const SpawnTable kSpawns[8];
extern const float kZoneLo[7], kZoneHi[7];  // zones 1..7 as fractions of the map width

}  // namespace game
