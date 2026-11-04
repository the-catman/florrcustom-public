#include "game/MobAi.h"

#include <algorithm>
#include <cmath>

#include "Math.h"
#include "game/Game.h"

namespace game {
using namespace world;

// an angle folded into [-pi, pi]
static float wrap(double a) {
    double n = std::fmod(a, TAU);
    n = n < -PI ? n + TAU : n;
    return float(n > PI ? n - TAU : n);
}

static float maxf(float a, float b) {
    if (a != a || b != b) return NAN;
    if (a == 0 && b == 0) return std::signbit(a) ? b : a;
    return a > b ? a : b;
}

// x moved into [lo, hi], whichever way round the bounds come
static float clampTo(double x, double lo, double hi) {
    double mn = lo, mx = hi;
    if (mn > mx) std::swap(mn, mx);
    return float(mn > x ? mn : (mx < x ? mx : x));
}

MobAi::MobAi(Game &game) : System(game) {}

// Whatever hurt e from a player's side becomes the target of e, and of everything e owns, if
// they hunt or fight back and have nothing better to do.
void MobAi::provoke(Entity &e, Entity *by) {
    if (!by || !entities_.cameraOf(by)) return;
    Entity *ag = by;
    while (ag && !ag->has(C_FLOWER) && !ag->has(C_MOVES))
        ag = ag->has(C_OWNED) ? world_.get(entities_.obj(*ag).owner) : nullptr;
    if (!ag) return;
    Obj &o = entities_.obj(e);
    if (o.ai && !world_.get(o.ai->target) && (o.ai->p.hunts || o.ai->p.fightsBack) && canChase(e, *o.ai, *ag) &&
        o.ai->target != ag->id)
        o.ai->target = ag->id;
    std::vector<uint32_t> owned = o.owned;
    for (uint32_t id : owned) provokeOwned(id, *ag);
}

void MobAi::provokeOwned(uint32_t id, Entity &ag) {
    Entity *e = world_.get(id);
    if (!e) return;
    Obj &o = entities_.obj(*e);
    if (o.ai && !world_.get(o.ai->target) && (o.ai->p.hunts || o.ai->p.fightsBack) && canChase(*e, *o.ai, ag) &&
        o.ai->target != ag.id)
        o.ai->target = ag.id;
    std::vector<uint32_t> owned = o.owned;
    for (uint32_t c : owned) provokeOwned(c, ag);
}

void MobAi::init(Entity &e, const AiParams &p) {
    Obj &o = entities_.obj(e);
    if (!o.ai) {
        o.ai = std::make_unique<Ai>();
        e.add(C_MOVES);
        thinkers.push_back(e.id);
    }
    Ai &ai = *o.ai;
    ai.p = p;
    ai.since = game_.now;
    ai.wanderAngle = rng_.angle();
    ai.wanderLeft = rng_.between(0, 1) == 1;
    if (p.headingPeriod > 0) {
        ai.timerOn = true;
        ai.timerAt = game_.now + double(p.headingPeriod);
    }
    float a = p.faceMove ? ai.wanderAngle : rng_.angle();
    entities_.setAngle(e, std::fmod(double(a), TAU));
}

void MobAi::tick(double dt) {
    float step = float(dt);
    // mobs made during the loop (by an attack) get their turn the same tick
    for (size_t i = 0; i < thinkers.size(); i++) {
        Entity *e = world_.get(thinkers[i]);
        if (!e) continue;
        Obj &o = entities_.obj(*e);
        Ai &ai = *o.ai;
        if (e->has(C_HEALTH) && e->hp() <= 0.0f) continue;
        // a centipede's body goes after whatever the segment in front of it does
        if (Entity *front = world_.get(o.segPrev)) {
            Obj &f = entities_.obj(*front);
            if (f.ai && f.ai.get() != &ai) ai.target = world_.get(f.ai->target) ? f.ai->target : 0;
            continue;
        }
        think(*e, ai, step);
    }
    game_.mobs.hatchFromHoles();
}

void MobAi::think(Entity &e, Ai &ai, float dt) {
    ai.cooldown = maxf(ai.cooldown - dt, 0.0f);
    float minX = game_.arena.mapW * (world_.get(ai.leader) ? -0.5f : ai.p.minZone - 0.5f);
    Entity *target = world_.get(ai.target);
    if (target && !canChase(e, ai, *target)) target = nullptr;
    if (!target) {
        ai.target = 0;
        if (ai.p.hunts) {
            if (uint32_t t = findTarget(e, ai, minX)) {
                ai.target = t;
                target = world_.get(t);
                float keep = std::min(ai.p.attackPeriod, 500.0f);
                ai.cooldown = ai.cooldown < keep ? keep : ai.cooldown;
            }
        }
    }
    int state = e.moves->state;
    if (target) {
        e.moves->setState(state | 1);
        chase(e, ai, *target, dt);
    } else {
        e.moves->setState(state & 254);
        wander(e, ai, dt, minX);
    }
}

void MobAi::chase(Entity &e, Ai &ai, Entity &target, float dt) {
    Body &b = entities_.body(e);
    const Body &t = entities_.body(target);
    double sx = b.x, sy = b.y, tx = t.x, ty = t.y;
    // where the target will be by the time it's reached, refined three times
    double ox = 0, oy = 0;
    if (ai.p.leads) {
        double vx = t.vx + t.ax, vy = t.vy + t.ay, lead = double(ai.p.leadSpeed);
        double dx = sx - tx, dy = sy - ty;
        double s = std::sqrt(dx * dx + dy * dy) / lead;
        double py = ty + s * vy, px = tx + s * vx;
        for (int k = 0; k < 2; k++) {
            float fy = float(py - ty), fx = float(px - tx);
            dx = sx - (tx + double(fx));
            dy = sy - (ty + double(fy));
            s = std::sqrt(dx * dx + dy * dy) / lead;
            py = ty + s * vy;
            px = tx + s * vx;
        }
        oy = double(float(py - ty));
        ox = double(float(px - tx));
    }
    double dy = sy - (ty + oy), dx = sx - (tx + ox);
    float dir = float(std::atan2(ty + oy - sy, tx + ox - sx));
    entities_.setAngle(e, std::fmod(double(dir), TAU));
    float vx, vy;
    if (dx * dx + dy * dy < double(ai.p.orbitDist) * double(ai.p.orbitDist)) {
        // close enough: circle it
        float side = float(double(dir) + HALF_PI);
        float v = float(double(ai.p.orbitSpeed * dt) * 0.001);
        vy = std::sin(side) * v;
        vx = std::cos(side) * v;
    } else {
        float v = float(double(ai.p.chaseSpeed * dt) * 0.001);
        vy = std::sin(dir) * v;
        vx = std::cos(dir) * v;
    }
    float slow = game_.effects.slowdown(e);
    vy *= slow;
    vx *= slow;
    b.vx += double(vx);
    b.vy += double(vy);
    if (ai.p.shoots && ai.cooldown <= 0.0f) {
        ai.cooldown += ai.p.attackPeriod;
        game_.mobs.shootMissile(e);
    }
}

void MobAi::wander(Entity &e, Ai &ai, float dt, float minX) {
    Body &b = entities_.body(e);
    float r = entities_.hitbox(e).radius;
    double sx = b.x, sy = b.y;
    // ants stay within reach of their hole: past it, they pick a spot near it to walk back to
    if (Entity *leader = world_.get(ai.leader)) {
        const Body &l = entities_.body(*leader);
        double leash = double(ai.p.leash);
        bool pick;
        if (ai.homing) {
            double dx = double(ai.homeX) - double(l.x), dy = double(ai.homeY) - double(l.y);
            pick = dx * dx + dy * dy > leash * leash;
        } else {
            double dx = sx - double(l.x), dy = sy - double(l.y);
            pick = dx * dx + dy * dy > leash * leash;
        }
        if (pick) {
            float dist = rng_.unit() * float(leash * 0.9);
            float a = rng_.angle();
            float oy = dist * std::sin(a), ox = dist * std::cos(a);
            ai.homeY = float(double(l.y) + double(oy));
            ai.homeX = float(double(l.x) + double(ox));
            ai.homing = true;
        }
    }
    // anything left of its zone walks back in
    if (!ai.homing && sx < double(minX)) {
        ai.homeY = float(sy);
        ai.homeX = minX + 100.0f;
        ai.homing = true;
    }
    bool homing = false;
    if (ai.homing) {
        float left = game_.arena.mapW * -0.5f;
        ai.homeX =
            clampTo(double(ai.homeX), double(r + (left < minX ? minX : left)), double(game_.arena.mapW * 0.5f - r));
        float half = game_.arena.mapH * 0.5f;
        ai.homeY = clampTo(double(ai.homeY), double(r - half), double(half - r));
        double dx = sx - double(ai.homeX), dy = sy - double(ai.homeY);
        if (dx * dx + dy * dy < 2500.0)
            ai.homing = false;
        else
            homing = true;
    }
    float speed = (homing ? ai.p.chaseSpeed : ai.p.speed) * game_.effects.slowdown(e);
    float turn = ai.wanderLeft ? ai.p.turn : -ai.p.turn;
    double age = game_.now - ai.since;
    if (!homing && ai.p.pulse) {
        float k = float(std::pow(0.5 - std::cos(age * double(ai.p.pulseRate)) * 0.5, double(ai.p.pulsePow)));
        speed *= k;
        turn *= k;
    }
    if (ai.timerOn) {
        while (!(game_.now < ai.timerAt)) {
            ai.timerAt += double(ai.p.headingPeriod);
            ai.wanderAngle = rng_.angle();
        }
    }
    ai.wanderAngle = float(double(turn * dt) * 0.001 + double(ai.wanderAngle));
    float h = ai.wanderAngle = wrap(double(ai.wanderAngle));
    // turn away from the edges of the map
    if (sx < double(r + game_.arena.mapW * -0.5f + 10.0f)) {
        double n = double(h);
        if (n > HALF_PI || n < -HALF_PI) h = ai.wanderAngle = wrap(PI - n);
    }
    if (sy < double(r + game_.arena.mapH * -0.5f + 10.0f) && h < 0.0f) h = ai.wanderAngle = wrap(double(-h));
    if (sx > double(game_.arena.mapW * 0.5f - r - 10.0f)) {
        double n = double(h);
        if (n > -HALF_PI && n < HALF_PI) h = ai.wanderAngle = wrap(PI - n);
    }
    if (sy > double(game_.arena.mapH * 0.5f - r - 10.0f) && h > 0.0f) h = ai.wanderAngle = wrap(double(-h));
    float dir = h;
    if (ai.p.wobble) dir = float(std::sin(age * double(ai.p.wobbleRate)) * double(ai.p.wobbleSize) + double(dir));
    if (ai.homing) dir = float(std::atan2(double(ai.homeY) - sy, double(ai.homeX) - sx));
    if (ai.p.faceMove) {
        entities_.setAngle(e, std::fmod(double(dir), TAU));
    } else {
        float s = float(double(ai.p.spin * dt) * 0.001);
        if (!(std::fabs(s) < FLOAT_EPSILON)) entities_.setAngle(e, std::fmod(double(float(b.angle + double(s))), TAU));
    }
    float v = float(double(speed * dt) * 0.001);
    b.vx += double(std::cos(dir) * v);
    b.vy += double(std::sin(dir) * v);
}

// whether e can keep going after target
bool MobAi::canChase(const Entity &e, const Ai &ai, const Entity &target) {
    Entity *leader = world_.get(ai.leader);
    const Body &s = *entities_.objs[e.slot()].body, &t = *entities_.objs[target.slot()].body;
    double tx = t.x, ty = t.y;
    if (tx < double(game_.arena.mapW * (leader ? -0.5f : ai.p.minZone - 0.5f))) return false;
    double dx = tx - double(s.x), dy = ty - double(s.y);
    double far = double(ai.p.sight * ai.p.giveUp);
    if (dx * dx + dy * dy > far * far) return false;
    if (target.has(C_HEALTH) && target.hp() <= 0.0f) return false;
    if (leader) {
        const Body &l = *entities_.objs[leader->slot()].body;
        dx = double(l.x) - tx;
        dy = double(l.y) - ty;
        double g = double(ai.p.guard);
        if (dx * dx + dy * dy > g * g) return false;
    }
    return entities_.root(e.id) != entities_.root(target.id);
}

// The best thing in sight: flowers before anything else, then mobs that hunt, then whatever is
// nearest; a later candidate only displaces an earlier one by being nearer, unless it ranks
// higher than it.
uint32_t MobAi::findTarget(Entity &e, Ai &ai, float minX) {
    const Body &s = entities_.body(e);
    float sight = ai.p.sight;
    float x0 = float(double(s.x) - double(sight)), y0 = float(double(s.y) - double(sight));
    float w = sight + sight, h = w;
    Entity *best = nullptr;
    double bestDist = 0;
    game_.physics.tree.visit(x0, y0, w, h, [&](uint16_t slot) {
        const Obj &o = entities_.objs[slot];
        Entity &d = *world_.at(slot);
        if (!o.body) return false;
        if (double(o.body->x) < double(minX)) return false;
        bool flower = d.has(C_FLOWER), moves = d.has(C_MOVES);
        if (!flower && !moves) return false;
        if (d.has(C_HEALTH) && !(d.hp() > 0.0f)) return false;
        if (flower) {
            auto it = game_.flowers.states.find(slot);
            if (it != game_.flowers.states.end() && it->second.safe && game_.now < it->second.safeUntil) return false;
        }
        double dx = double(s.x) - double(o.body->x), dy = double(s.y) - double(o.body->y);
        double n = dx * dx + dy * dy;
        if (!(n < double(sight) * double(sight))) return false;
        if (best) {
            bool bFlower = best->has(C_FLOWER), bMoves = best->has(C_MOVES);
            bool outranks;
            if (flower && !bFlower)
                outranks = true;
            else if (!moves)
                outranks = false;
            else if (!bMoves)
                outranks = true;
            else if (!o.ai->p.hunts)
                outranks = false;
            else
                outranks = !entities_.obj(*best).ai->p.hunts;
            if (!outranks && !(n < bestDist)) return false;
        }
        if (!canChase(e, ai, d)) return false;
        best = &d;
        bestDist = n;
        return false;
    });
    return best ? best->id : 0;
}

}  // namespace game
