// Petals lying on the ground: what dying things leave behind, and flowers picking them up.
#pragma once
#include <cstdint>
#include <vector>

#include "game/Components.h"
#include "game/System.h"

namespace game {

class Drops : public System {
public:
    explicit Drops(Game &game);

    void dropLoot(world::Entity &e, world::Entity *by);
    bool mythicTaken(uint8_t type, const world::Entity &dying);
    void tick(double dt);

    std::vector<uint32_t> all;

private:
    void pickUp(world::Entity &drop, DropState &ds);
};

}  // namespace game
