#include "game/Game.h"

#include <algorithm>

namespace game {
using namespace world;

System::System(Game &game) : game_(game), world_(game.entities.world), entities_(game.entities), rng_(game.rng) {}

Game::Game(uint32_t seed, double now)
    : rng(seed),
      now(now),
      entities(*this),
      arena(*this),
      physics(*this),
      combat(*this),
      drops(*this),
      flowers(*this),
      petals(*this),
      effects(*this),
      mobs(*this),
      mobAi(*this),
      tanks(*this),
      clients(*this) {
    arena.init();
    // nobody is told about these two
    entities.create(bit(C_EVENT));
    entities.create(bit(C_ROYALE));
    mobs.spawn();
}

void Game::after(double ms, std::function<void()> fn) {
    timers_.push_back({now + ms, std::move(fn)});
}

// whatever was put off until now
void Game::runTimers() {
    for (size_t i = 0; i < timers_.size(); i++) {
        if (now < timers_[i].at) continue;
        std::function<void()> fn = std::move(timers_[i].fn);
        fn();
        if (i + 1 < timers_.size()) timers_[i] = std::move(timers_.back());
        timers_.pop_back();
        i--;
    }
}

void Game::tick(double time) {
    double dt = last_ < 0 ? 50 : time - last_;
    last_ = time;
    now = time;
    dt = double(std::min(float(dt), 100.0f));
    arena.time += dt;
    runTimers();
    combat.reapDead(dt);
    drops.tick(dt);
    mobs.moveSegments();
    mobs.spawn();
    mobAi.tick(dt);
    clients.waitToSpawn(dt);
    arena.updateLeaderboard();
    effects.tick(dt);
    effects.tickWebs();
    if (tankMode) tanks.tick(dt);
    flowers.tick(dt);
    petals.move(dt);
    flowers.move(dt);
    physics.integrate(dt);
    physics.treeUpdate();
    physics.collide();
    clients.moveCameras();
    if (Entity *a = entities.world.get(arena.id)) a->arena->setTime(float(arena.time));
    clients.buildPackets();
    entities.world.endTick();
    tickNo++;
    for (size_t i = 0; i < entities.removals.size(); i++) entities.destroy(entities.removals[i]);
    entities.removals.clear();
}

}  // namespace game
