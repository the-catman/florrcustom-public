// Damage, healing, and what happens when health runs out.
#pragma once
#include <cstdint>
#include <vector>

#include "game/Components.h"
#include "game/System.h"

namespace game {

class Combat : public System {
public:
    explicit Combat(Game &game);

    void damage(world::Entity &e, world::Entity *by, float amount, uint16_t flags = 0);
    void heal(world::Entity &e, float amount);
    void reapDead(double dt);

    std::vector<uint32_t> healths;  // everything with health, in the order it got it
};

}  // namespace game
