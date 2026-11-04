// Everything that exists, and the basics of what any of it can be: where it is, how big, who owns it.
#pragma once
#include <cstdint>
#include <vector>

#include "game/Components.h"
#include "game/System.h"
#include "world/World.h"

namespace game {

class Entities {
public:
    explicit Entities(Game &game);

    Obj &obj(const world::Entity &e) { return objs[e.slot()]; }
    world::Entity &create(uint32_t comps);
    void destroy(uint32_t id);
    void setOwner(world::Entity &e, uint32_t owner);
    Body &body(world::Entity &e);
    Hitbox &hitbox(world::Entity &e);
    void setPos(world::Entity &e, double x, double y);
    void setAngle(world::Entity &e, double a);
    void setRadius(world::Entity &e, float r);
    // sets max health, keeping the same fraction of it
    void setMaxHealth(world::Entity &e, float max);
    Life &life(world::Entity &e);
    void queueRemoval(world::Entity &e);
    world::Entity *cameraOf(world::Entity *e);
    world::Entity *killerOf(world::Entity *by);
    uint32_t root(uint32_t id);

    world::World world;
    std::vector<Obj> objs;           // by slot
    std::vector<uint32_t> removals;  // destroyed after this tick's packets, first in first out

private:
    Game &game_;
};

}  // namespace game
