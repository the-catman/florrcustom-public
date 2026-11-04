#include "world/World.h"

#include <algorithm>

namespace world {

World::World() {
    // generations start out scattered, from a 64-bit LCG that skips 0
    uint64_t x = 0;
    for (uint32_t slot = 0; slot < uint32_t(kSlots); slot++) {
        uint32_t gen;
        do {
            x = x * 6364136223846793005ull + 1;
            gen = uint32_t(x >> 33);
        } while (!(gen & 0x3ffff));
        ids_[slot] = slot | gen << 14;
        free_.push_back(uint16_t(slot));
    }
}

Entity &World::create(uint32_t comps) {
    uint16_t slot = free_.front();
    free_.pop_front();
    auto &p = ents_[slot];
    p = std::make_unique<Entity>();
    p->id = ids_[slot];
    for (int c = 0; c < 32; c++)
        if (comps >> c & 1) p->add(c);
    used_.insert(std::lower_bound(used_.begin(), used_.end(), slot), slot);
    return *p;
}

Entity *World::get(uint32_t id) const {
    Entity *e = ents_[id & 16383].get();
    return e && e->id == id ? e : nullptr;
}

void World::destroy(uint32_t id) {
    uint16_t slot = id & 16383;
    if (!get(id)) return;
    ents_[slot].reset();
    used_.erase(std::lower_bound(used_.begin(), used_.end(), slot));
    // next generation, wrapping to 1
    ids_[slot] = (id >> 14) == 0x3ffff ? (slot | 16384u) : id + 16384u;
    free_.push_back(slot);
}

}  // namespace world
