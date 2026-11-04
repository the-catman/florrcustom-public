// Hitboxes in the quadtree, the collisions found through it, and movement.
#pragma once
#include <cstdint>
#include <vector>

#include "game/Components.h"
#include "game/QuadTree.h"
#include "game/System.h"

namespace game {

class Physics : public System {
public:
    explicit Physics(Game &game);

    void treeAdd(world::Entity &e);
    void treeUpdate();
    void collide();
    void integrate(double dt);

    std::vector<uint16_t> sized;  // everything with a hitbox, in the order they got one
    QuadTree tree{world::kSlots};

private:
    void treeBox(uint16_t slot);
    bool collidePair(world::Entity &e, world::Entity &other);
    void bump(world::Entity &e, world::Entity &other, double vx, double vy, double ox, double oy, bool sameSide);

    bool treeReady_ = false;
};

}  // namespace game
