// The game: one arena, any number of clients. Platform code feeds it client bytes, calls tick()
// every 50 ms with the current time, and sends each client the packet that tick produced.
#pragma once
#include <cstdint>
#include <functional>
#include <vector>

#include "game/Arena.h"
#include "game/Clients.h"
#include "game/Combat.h"
#include "game/Drops.h"
#include "game/Entities.h"
#include "game/Flowers.h"
#include "game/MobAi.h"
#include "game/Mobs.h"
#include "game/Petals.h"
#include "game/Physics.h"
#include "game/Rng.h"
#include "game/StatusEffects.h"
#include "game/Tables.h"

namespace game {

class Game {
public:
    Game(uint32_t seed, double now);

    int connect() { return clients.connect(); }
    void disconnect(int client) { clients.disconnect(client); }
    void receive(int client, const uint8_t *b, size_t n, double now) { clients.receive(client, b, n, now); }
    // advances the world to the given time (ms) and builds every client's packet
    void tick(double time);
    const std::vector<uint8_t> &packet(int client) const { return clients.packets.at(client); }

    uint32_t rngState() const { return rng.state(); }
    // where fresh random seeds come from (std::random_device unless set)
    void setEntropy(std::function<uint32_t()> f) { entropy = std::move(f); }

    // runs fn once this much time has passed, at the start of a tick
    void after(double ms, std::function<void()> fn);

    Rng rng;
    std::function<uint32_t()> entropy;
    double now = 0;
    uint32_t tickNo = 1;

    Entities entities;
    Arena arena;
    Physics physics;
    Combat combat;
    Drops drops;
    Flowers flowers;
    Petals petals;
    StatusEffects effects;
    Mobs mobs;
    MobAi mobAi;
    Clients clients;

private:
    struct Timer {
        double at;
        std::function<void()> fn;
    };

    void runTimers();

    std::vector<Timer> timers_;
    double last_ = -1;
};

}  // namespace game
