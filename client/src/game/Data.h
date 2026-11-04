// What the title screen menus show.
#pragma once

#include <cstdint>

#include "Catalog.h"

namespace game {

using namespace catalog;

const char *mobName(int type);
int mobRarity(int type);
const char *petalName(int type);
const char *petalDesc(int type);
const char *petalDesc2(int type);  // a second description line, or ""
int petalRarity(int type);
int petalCount(int type);        // petals per slot
bool petalClustered(int type);   // its petals orbit together, as one
float petalLabelSize(int type);  // card label font size
float petalReload(int type);     // ms
uint32_t rarityColor(int rarity);
const char *rarityName(int rarity);
const MobDef &mobDef(int type);

struct ChangelogEntry {
    const char *date;
    const char *lines[8];  // a line may hold "\n" to continue on a new row without a bullet
};
const ChangelogEntry *changelog(int &count);

}  // namespace game
