// What's done to a flower or a mob for a while (poison, no healing, honey), and webs.
#pragma once
#include <cstdint>
#include <vector>

#include "game/Components.h"
#include "game/System.h"

namespace game {

class StatusEffects : public System {
public:
    explicit StatusEffects(Game &game);

    bool noHeal(world::Entity &e);
    float slowdown(world::Entity &e);
    float spinFactor(world::Entity &e);
    void afflict(world::Entity &e, EffectKind kind, double ms, uint32_t source = 0);
    void makeWeb(world::Entity &petal);
    void release(world::Entity &e, uint32_t web);
    void tick(double dt);
    void tickWebs();

    std::vector<uint32_t> affected;  // everything that can be poisoned and so on, in the order it was made
    std::vector<uint32_t> webs;      // in the order they were made
};

}  // namespace game
