#include "game/Data.h"

namespace game {

const char *mobName(int t) {
    return t > 0 && t < M_COUNT ? kMobs[t].name : "?";
}
int mobRarity(int t) {
    return t > 0 && t < M_COUNT ? kMobs[t].rarity : 0;
}

static bool validPetal(int t) {
    return t > 0 && t < P_COUNT;
}
const char *petalName(int t) {
    return validPetal(t) ? kPetals[t].name : "?";
}
const char *petalDesc(int t) {
    return validPetal(t) ? kPetals[t].desc : "";
}
const char *petalDesc2(int t) {
    return validPetal(t) ? kPetals[t].desc2 : "";
}
int petalRarity(int t) {
    return validPetal(t) ? kPetals[t].rarity : 0;
}
int petalCount(int t) {
    return validPetal(t) ? kPetals[t].count : 1;
}
bool petalClustered(int t) {
    return validPetal(t) && kPetals[t].clustered();
}
float petalLabelSize(int t) {
    return validPetal(t) ? kPetals[t].label : 14;
}
float petalReload(int t) {
    return validPetal(t) ? kPetals[t].reload : 0;
}

static const uint32_t kRarityColors[6] = {0x7eef6d, 0xffe65d, 0x5e4de3, 0x861fde, 0xde1f1f, 0xde1f65};
static const char *const kRarityNames[6] = {"Common", "Unusual", "Rare", "Epic", "Legendary", "Unique"};
uint32_t rarityColor(int r) {
    return kRarityColors[r >= 0 && r < 6 ? r : 0];
}
const char *rarityName(int r) {
    return kRarityNames[r >= 0 && r < 6 ? r : 0];
}

const MobDef &mobDef(int type) {
    return kMobs[type > 0 && type < M_COUNT ? type : 0];
}

static const ChangelogEntry kChangelog[] = {
    {"November 2nd 2020", {"New petals."}},
    {"October 29th 2020", {"We're back to FFA."}},
    {"October 28th 2020",
     {"Changed value from last change to 2.5 seconds.",
      "Leaf, Cactus and Faster now only apply their effects once they have spawned the first time.\nEffect still "
      "applies if they're reloading after they get destroyed.",
      "We'll be playing Battle Royale again for the next couple of days.",
      "Battle Royale now has 3x exp and drop rate.",
      "The game mode will be polished a bit better later, for now it's a bit of a work in progress.",
      "Buffed Epic Peas a little bit."}},
    {"October 27th 2020",
     {"First petal spawn when changing a slot takes at least 5 seconds.\nWhich makes quickly swapping loadouts less "
      "effective."}},
    {"October 25th 2020", {"You can now press X to swap your loadout."}},
    {"September 24th 2020", {"Nerfed Leaf damage by 20%.", "Reduced Stinger health."}},
    {"May 16th 2020",
     {"It seems there aren't enough players to support the game mode. We've switched back to FFA now."}},
    {"May 15th 2020",
     {"We'll be trying out a new game mode for a couple of days: Battle Royale.\nThere aren't enough players to split "
      "among two game modes, so this will be the only game mode available for a little bit."}},
    {"May 14th 2020", {"Added 1 new petal."}},
    {"March 5th 2020", {"Honey now also slows down movement speed a little bit."}},
    {"March 4th 2020", {"Added 1 new petal."}},
    {"March 3rd 2020",
     {"Flowers now only keep up to 10 items after their death.",
      "Flowers now spawn at 3/4 their previous level, rather than half."}},
    {"February 27th 2020", {"You now get to keep some of your items and half your level when you die."}},
    {"February 27th 2020",
     {"Changed flower health from 100-200 (lvl 1-45) to 100-150.", "Rarer drops take a tiny bit longer to despawn.",
      "Legendary Egg hatch time nerfed."}},
    {"February 25th 2020", {"Added 1 new petal.", "Web now affects mobs."}},
    {"February 24th 2020",
     {"Turns out Hornets got too smart. Now only a small percentage of them will be einstein.",
      "Web movement speed debuff increased."}},
    {"February 24th 2020", {"Hornet AI is now a lot smarter. No more herding.", "Nerfed Hornet missile damage."}},
    {"February 23rd 2020", {"Added 1 new petal."}},
    {"February 21st 2020",
     {"Unknown zone is now a lot smaller.\nAll other zones are the same size as before, which means the map got "
      "smaller."}},
    {"February 20th 2020", {"Added spiders.", "Added 3 new petals."}},
    {"February 19th 2020",
     {"Legendary petals are now easier to find.\nSquare has been moved to a separate category.",
      "Rare Roses now stick together.", "Added 1 new petal.",
      "Changed map dimensions (total area still the same, just longer sideways).",
      "All flowers are now the same speed, regardless of level.\nMob chase speed adjusted to compensate."}},
    {"February 18th 2020",
     {"Rare Rose nerfed (-30% heal).", "Dandelion rarity changed to Rare (now drops a lot more frequently)."}},
    {"February 17th 2020",
     {"Added Ladybug variant to ???.",
      "Hornets were accidentally made a lot more common a couple of days ago, that has been fixed.",
      "Added 1 new petal."}},
    {"February 16th 2020",
     {"Petal drop rates are now generated automatically.\nThis will allow a more fair distribution of drops based on "
      "their rarity (so for example, all epics are equally as rare).",
      "Mobs can now drop more than one petal at a time."}},
    {"February 15th 2020", {"Added keyboard controls (disabled by default until we know which players prefer)."}},
    {"February 14th 2020", {"Added Egg petal."}},
    {"February 13th 2020",
     {"Improved changelog UI.", "Added Discord button. Join our community!",
      "Reddit button soon, meanwhile, join us at reddit.com/r/florrio", "Mob gallery improvements."}},
    {"February 11th 2020",
     {"Added mob gallery. No pictures at the moment, but maybe eventually.", "Added 2 new petals.",
      "Decreased ram damage between two low leveled flowers (to curb noobs chasing each other)."}},
    {"February 10th 2020", {"Added 1 new petal."}},
    {"February 9th 2020", {"Added 1 new petal."}},
    {"February 8th 2020", {"Added 3 new petals."}},
    {"February 7th 2020",
     {"Petals you haven't unlocked will not appear in the title screen anymore.",
      "Added a little movement arrow to help find your mouse (disabled by default).", "Added 1 new petal."}},
    {"February 6th 2020",
     {"Added Petal Gallery to keep track of which petals you've seen.",
      "New petal announcements will no longer include their names, to make it a bit more mysterious.",
      "Added 1 new petal."}},
    {"February 3rd 2020", {"Added Ants.", "A few balance tweaks."}},
    {"February 1st 2020",
     {"Changed map background.", "Early levels are a bit slower.", "Added Cacti to medium zone.",
      "Added Massive Beetle.", "Added Massive Centipede.", "Buffed Iris.", "Buffed Peas.", "Nerfed Rose."}},
};

const ChangelogEntry *changelog(int &count) {
    count = int(sizeof kChangelog / sizeof *kChangelog);
    return kChangelog;
}

}  // namespace game
