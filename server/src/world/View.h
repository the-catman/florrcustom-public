// What one client knows about the world, and the entity section of its next update.
#pragma once
#include <cstdint>
#include <functional>
#include <vector>

#include "Writer.h"
#include "world/World.h"

namespace world {

class View {
public:
    // visible: ids the client should know after this update, sorted by slot
    // pad: two random bytes for each padding slot
    // yours: whether an entity belongs to this client, which also sends it the flower's private fields
    void write(net::Writer &w, const World &world, const std::vector<uint32_t> &visible,
               const std::function<uint8_t()> &pad, const std::function<bool(const Entity &)> &yours);
    void reset() { known_.clear(); }
    bool knows(uint32_t id) const;

private:
    std::vector<uint32_t> known_;  // sorted by slot
};

void writeFull(net::Writer &w, const Entity &e, bool yours);
void writeDelta(net::Writer &w, const Entity &e, bool yours);

}  // namespace world
