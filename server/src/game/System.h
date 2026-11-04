// A part of the game: it keeps its own state and reaches the other parts through game_.
#pragma once
#include "game/Rng.h"
#include "world/World.h"

namespace game {

class Game;
class Entities;

class System {
protected:
    explicit System(Game &game);

    Game &game_;
    world::World &world_;
    Entities &entities_;
    Rng &rng_;
};

}  // namespace game
