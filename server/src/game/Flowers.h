// Players' flowers: spawning, levels and stats, reloading petals, their zones, and dying.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "game/Components.h"
#include "game/Rng.h"
#include "game/System.h"

namespace game {

// a player's name as it's shown
std::string cleanName(std::string n);

class Flowers : public System {
public:
    explicit Flowers(Game &game);

    void spawn(Client &c);
    void giveXp(world::Entity &flower, float xp);
    void onDev(world::Entity &flower, int what, int type);
    void onDeath(world::Entity &flower);
    void deathScreen(world::Entity &flower, world::Entity *by);
    void setSlot(world::Entity &flower, int slot, uint8_t type);
    int countAlive(const FlowerState &fs, int level, uint8_t type);
    bool zoneLocked(world::Entity &flower);
    void tick(double dt);
    void move(double dt);
    bool findSpawnSpot(int earned, float &x, float &y);

    std::unordered_map<uint16_t, FlowerState> states;  // by slot
    std::vector<uint32_t> all;

private:
    void givePetals(world::Entity &f, FlowerState &fs, const world::CameraData &cam, int earned);
    void turnRing(world::Entity &f, FlowerState &fs, int level, bool locked, double dt);
    void reloadPetals(world::Entity &f, FlowerState &fs, int level, bool locked, double dt);
    void runZoneTimer(world::Entity &f, FlowerState &fs, int level, double dt);
    void recover(world::Entity &f, FlowerState &fs, int level, double dt);
    bool spawnTaken(float x, float y);
    float levelNeed(int level);
    void updateStats(world::Entity &flower);

    Rng deathRng_;  // shuffles a dead flower's petals; seeded on the first death
    bool deathRngSeeded_ = false;
};

}  // namespace game
