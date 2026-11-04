// How mobs move: wandering, walking back home, picking a target and chasing it.
#pragma once
#include <cstdint>
#include <vector>

#include "game/Components.h"
#include "game/System.h"

namespace game {

class MobAi : public System {
public:
    explicit MobAi(Game &game);

    void init(world::Entity &e, const AiParams &p);
    void provoke(world::Entity &e, world::Entity *by);
    void tick(double dt);

    std::vector<uint32_t> thinkers;  // every mob that thinks, in the order they're run

private:
    void think(world::Entity &e, Ai &ai, float dt);
    void chase(world::Entity &e, Ai &ai, world::Entity &target, float dt);
    void wander(world::Entity &e, Ai &ai, float dt, float minX);
    bool canChase(const world::Entity &e, const Ai &ai, const world::Entity &target);
    uint32_t findTarget(world::Entity &e, Ai &ai, float minX);
    void provokeOwned(uint32_t id, world::Entity &aggressor);
};

}  // namespace game
