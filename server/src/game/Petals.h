// What each kind of petal is like: its size, strength and health, how far out it sits on the
// ring, and what the special ones do there (heal, launch, split, hatch, ...).
#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "game/Components.h"
#include "game/System.h"

namespace game {

class Petals : public System {
public:
    explicit Petals(Game &game);

    struct RingPlace {
        float radius;
        bool skip;
    };

    void make(world::Entity &flower, int slot, int copy);
    void place(world::Entity &flower);
    void contact(world::Entity &petal, world::Entity &other);
    void flowerContact(world::Entity &flower, world::Entity &petal, world::Entity &other);
    void onDeath(world::Entity &petal);
    void move(double dt);

    std::unordered_map<uint16_t, PetalState> states;  // by slot
    std::vector<uint32_t> all;

private:
    bool olderThan(const PetalState &ps, double ms) const;
    RingPlace wingRing(const PetalState &ps, int face);
    RingPlace roseRing(world::Entity &p, PetalState &ps, world::Entity &f, float dt);
    RingPlace launchRing(world::Entity &p, PetalState &ps, world::Entity &f);
    RingPlace webRing(world::Entity &p, PetalState &ps, world::Entity &f);
    RingPlace bubbleRing(world::Entity &p, PetalState &ps, world::Entity &f);
    RingPlace pollenRing(world::Entity &p, PetalState &ps, world::Entity &f);
    RingPlace eggRing(world::Entity &p, PetalState &ps, world::Entity &f);
    RingPlace peasRing(world::Entity &p, PetalState &ps, world::Entity &f);
    void burst(world::Entity &p);
    void init(world::Entity &p, uint8_t type);
    void unslot(world::Entity &p);
    RingPlace petalRing(world::Entity &p, PetalState &ps, world::Entity &flower, float dt);
    uint32_t creditOf(world::Entity *petal);
};

}  // namespace game
