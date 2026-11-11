// Entity storage: 16384 slots, each id carrying a generation so a reused slot gets a new id.
// Free slots are handed out oldest-freed first.
#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

#include "world/Entity.h"

namespace world {

constexpr int kSlots = 16384;

class World {
public:
    World();

    Entity &create(uint64_t comps);
    Entity *get(uint32_t id) const;
    Entity *at(uint16_t slot) const { return ents_[slot].get(); }
    void destroy(uint32_t id);

    // slots in use, in slot order
    const std::vector<uint16_t> &slots() const { return used_; }
    template <class F>
    void each(F f) {
        for (uint16_t s : used_) f(*ents_[s]);
    }

    void endTick() {
        for (uint16_t s : used_) ents_[s]->clean();
    }

private:
    std::array<std::unique_ptr<Entity>, kSlots> ents_{};
    std::array<uint32_t, kSlots> ids_{};  // next id for each slot
    std::deque<uint16_t> free_;
    std::vector<uint16_t> used_;  // sorted
};

}  // namespace world
