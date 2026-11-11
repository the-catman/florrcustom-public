#include "game/Arena.h"

#include <algorithm>
#include <cmath>

#include "Math.h"
#include "game/Game.h"

namespace game {
using namespace world;

Arena::Arena(Game &game) : System(game) {}

// every quarter second: the ten best flowers by score, and how many are playing
void Arena::updateLeaderboard() {
    if (lbOn_ && game_.now < lbAt_) return;
    lbOn_ = true;
    lbAt_ = game_.now + 250;
    std::vector<Client *> rows;
    for (auto &[id, c] : game_.clients.all)
        if (world_.get(c.flower)) rows.push_back(&c);
    auto score = [&](const Client *c) {
        Entity &f = *world_.get(c->flower);
        if (f.has(C_FLOWER)) return f.flower->score;
        Entity *cam = world_.get(c->camera);
        return cam && cam->has(C_DRIVER) ? cam->driver->score : 0.0f;
    };
    std::sort(rows.begin(), rows.end(), [&](const Client *a, const Client *b) { return score(b) < score(a); });
    Entity *arena = world_.get(id);
    if (!arena) return;
    int n = int(std::min<size_t>(rows.size(), 10));
    arena->arena->setLeaderRows(n);
    for (int i = 0; i < n; i++) {
        float s = score(rows[i]);
        arena->arena->setLeaderScore(i, toU32(s));
        arena->arena->setLeaderName(i, world_.get(rows[i]->flower)->name->name);
        arena->arena->setLeaderFlower(i, rows[i]->camera);
    }
    arena->arena->setPlayers(uint32_t(rows.size()));
}

bool Arena::running() {
    Entity *a = world_.get(id);
    return a && (a->arena->state & 1);
}

// x below which a flower of this level is out of its zone
float Arena::zoneEdge(int level) {
    float w = mapW;
    double lo = double(w * -0.5f), span = double(w * 0.5f) - lo;
    if (royale()) {
        double t = time * (1.0 / 900000);
        double k = t < 0.0 ? -0.1 : std::fmin(t, 1.0) * 1.1 - 0.1;
        k = k < 0.0 ? 0.0 : std::fmin(k, 1.0);
        return std::roundf(float(span * k + lo) * 0.02f) * 50.0f;
    }
    if (level < 15) return -INFINITY;
    double k = level <= 29 ? 0.3f : level <= 44 ? 0.6f : 0.9f;
    return std::roundf(float(span * k + lo) * 0.02f) * 50.0f;
}

bool Arena::royale() {
    Entity *a = world_.get(id);
    return a && a->arena->mode == 1;
}

void Arena::init() {
    Entity &a = entities_.create(bit(C_ARENA));
    id = a.id;
    a.arena->setState(1);
    a.arena->setWidth(mapW);
    a.arena->setHeight(mapH);
    a.arena->setLeaderRows(0);
    a.arena->setPlayers(0);
    a.arena->setSeed(rng_.between(0, 0xffffffffu));
}

}  // namespace game
