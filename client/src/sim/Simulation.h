// The world as far as this client knows it, kept up to date from the server's updates.
#pragma once
#include <cstdint>
#include <unordered_map>

#include "Reader.h"
#include "sim/Entity.h"

namespace sim {

class Simulation {
public:
    std::unordered_map<uint16_t, Entity> entities;  // by slot
    uint32_t cameraId = 0;
    uint32_t arenaId = 0;

    Entity *get(uint32_t id);
    const Entity *get(uint32_t id) const;
    CameraData *camera();
    const ArenaData *arena() const;
    // the flower the camera is following
    Entity *flower();

    void clear();
    // reads the entity section of an update; false if it doesn't parse
    bool decodeUpdate(net::Reader &r, double now);

private:
    void decodeFull(net::Reader &r, Entity &e, bool yours);
    void decodeDelta(net::Reader &r, Entity &e);
};

}  // namespace sim
