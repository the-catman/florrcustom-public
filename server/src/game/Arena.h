// The map, how long the round has run, and the leaderboard.
#pragma once
#include <cstdint>

#include "game/Components.h"
#include "game/System.h"

namespace game {

class Arena : public System {
public:
    explicit Arena(Game &game);

    bool running();
    float zoneEdge(int level);
    bool royale();
    void updateLeaderboard();
    // makes the arena entity, at the very start of the game
    void init();

    uint32_t id = 0;
    float mapW = 50000;
    float mapH = 4000;
    double time = 0;

private:
    bool lbOn_ = false;
    double lbAt_ = 0;  // next leaderboard update
};

}  // namespace game
