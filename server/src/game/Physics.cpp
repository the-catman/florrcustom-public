#include "game/Physics.h"

#include <algorithm>
#include <cmath>

#include "Math.h"
#include "game/Game.h"

namespace game {
using namespace world;

static bool finite(double v) {
    return std::isfinite(v);
}

static bool dead(const Entity &e) {
    return e.has(C_HEALTH) && e.hp() <= 0.0f;
}

Physics::Physics(Game &game) : System(game) {}

// velocity decays with a 125 ms time constant; positions move by the exact integral of that and
// stay inside the map
void Physics::integrate(double dt) {
    constexpr double reach = 1 / (1 - 0.992);
    double k = std::pow(0.992, dt), m = reach - k * reach;
    for (uint16_t s : world_.slots()) {
        Obj &o = entities_.objs[s];
        if (!o.body) continue;
        Body &b = *o.body;
        Entity &e = *world_.at(s);
        float turn = float(dt * 0.001 * double(float(b.spin)));
        if (!(std::fabs(turn) < FLOAT_EPSILON))
            entities_.setAngle(e, std::fmod(double(float(b.angle + double(turn))), TAU));
        double x = m * b.vx + dt * b.ax + double(b.x), y = m * b.vy + dt * b.ay + double(b.y);
        if (std::isfinite(x) && std::isfinite(y)) {
            b.x = float(x);
            b.y = float(y);
            e.position->setX(b.x);
            e.position->setY(b.y);
        }
        b.vy *= k;
        b.vx *= k;
        if (std::fabs(b.vx) < EPSILON) b.vx = 0;
        if (std::fabs(b.vy) < EPSILON) b.vy = 0;
        if (!b.unbounded) {
            float r = o.hit ? o.hit->radius : 0.0f;
            float lo = game_.arena.mapW * -0.5f - -r;
            if (lo > b.x) {
                b.x = lo;
                b.vx = 0;
            }
            lo = game_.arena.mapH * -0.5f - -r;
            if (lo > b.y) {
                b.y = lo;
                b.vy = 0;
            }
            float hi = -r + game_.arena.mapW * 0.5f;
            if (hi < b.x) {
                b.x = hi;
                b.vx = 0;
            }
            hi = -r + game_.arena.mapH * 0.5f;
            if (hi < b.y) {
                b.y = hi;
                b.vy = 0;
            }
        }
        e.position->setX(b.x);
        e.position->setY(b.y);
    }
}

// a hitbox's bounding square, from where it is now; once a tick
void Physics::treeBox(uint16_t slot) {
    const Hitbox &h = *entities_.objs[slot].hit;
    const Body &b = *entities_.objs[slot].body;
    float r = h.radius;
    tree.setBox(slot, b.x - r, b.y - r, r + r);
}

void Physics::treeAdd(Entity &e) {
    treeBox(e.slot());
    tree.place(e.slot());
}

// once a tick, after things have moved: fresh boxes, everything moved to its node, then the
// collisions
void Physics::treeUpdate() {
    for (uint16_t s : sized) tree.newTick(s);
    for (uint16_t s : sized) treeBox(s);
    if (!treeReady_) {
        // the first time, the root gets its real size and everything goes in afresh
        treeReady_ = true;
        float hh = game_.arena.mapH * 0.5f, hw = game_.arena.mapW * 0.5f;
        float bottom = hh + 500.0f, right = hw + 500.0f, top = -500.0f - hh, left = -500.0f - hw;
        tree.setRoot(left, top, right - left, bottom - top);
    }
    for (size_t i = 0; i < sized.size(); i++) {
        uint16_t s = sized[i];
        treeBox(s);
        tree.place(s);
    }
    tree.merge();
}

// every pair of overlapping hitboxes, each handled from its lower slot
void Physics::collide() {
    for (size_t i = 0; i < sized.size(); i++) {
        uint16_t s = sized[i];
        const Hitbox &h = *entities_.objs[s].hit;
        Entity &e = *world_.at(s);
        if (h.ghost || dead(e)) continue;
        QuadTree::Box box = tree.box(s);
        tree.visit(box.x, box.y, box.w, box.h, [&](uint16_t o) { return collidePair(e, *world_.at(o)); });
    }
}

// true once e is dead and should stop looking
bool Physics::collidePair(Entity &e, Entity &other) {
    Obj &so = entities_.obj(e), &oo = entities_.obj(other);
    Hitbox &sh = *so.hit, &oh = *oo.hit;
    if (oh.ghost || dead(other)) return false;
    if (e.slot() >= other.slot()) return false;
    Body &sb = *so.body, &ob = *oo.body;
    float dx = sb.x - ob.x, dy = sb.y - ob.y, rr = oh.radius + sh.radius;
    if (!(dx * dx + dy * dy < rr * rr)) return false;
    bool same = entities_.root(e.id) == entities_.root(other.id);
    if ((!same || (sh.hitsTeam && oh.hitsTeam)) && !sh.still && !oh.still) {
        double svx = sb.vx + sb.ax, svy = sb.vy + sb.ay, ovx = ob.vx + ob.ax, ovy = ob.vy + ob.ay;
        bump(e, other, svx, svy, ovx, ovy, same);
        bump(other, e, ovx, ovy, svx, svy, same);
        if (sh.pushable && oh.pushable) {
            // pushed apart, the lighter one further
            double ex = double(sb.x) - double(ob.x), ey = double(sb.y) - double(ob.y);
            double d2 = ex * ex + ey * ey;
            double r = double(sh.radius) + double(oh.radius), r2 = r * r;
            if (d2 < r2) {
                double om = double(oh.radius);
                om = om * om * double(oh.density);
                double sm = double(sh.radius);
                sm = sm * sm * double(sh.density);
                double total = sm + om, share = om / total;
                double depth = std::sqrt(r2) - std::sqrt(d2);
                double len = std::sqrt(ex * ex + ey * ey), ux, uy;
                if (len < EPSILON) {
                    uy = 0;
                    ux = 1;
                } else {
                    uy = ey / len;
                    ux = ex / len;
                }
                double py = depth * uy, px = depth * ux;
                double ny = share * py + double(sb.y), nx = share * px + double(sb.x);
                if (finite(nx) && finite(ny)) entities_.setPos(e, nx, ny);
                double back = -sm / total;
                ny = back * py + double(ob.y);
                nx = back * px + double(ob.x);
                if (finite(nx) && finite(ny)) entities_.setPos(other, nx, ny);
            }
        }
    }
    if (!same) {
        if (e.has(C_PETAL)) game_.petals.contact(e, other);
        if (other.has(C_PETAL)) game_.petals.contact(other, e);
        for (auto [f, o] : {std::pair<Entity *, Entity *>{&e, &other}, {&other, &e}}) {
            if (!f->has(C_FLOWER)) continue;
            FlowerState &fs = game_.flowers.states[f->slot()];
            for (int s = 0; s < 8; s++)
                for (int k = 0; k < 3; k++)
                    if (Entity *p = world_.get(fs.petal[s][k]); p && p->has(C_PETAL))
                        game_.petals.flowerContact(*f, *p, *o);
        }
    }
    return dead(e);
}

// What touching other does to e: a bounce off it (weighted by mass when both can move), a push
// out of it otherwise, an extra shove for flowers, and the damage.
void Physics::bump(Entity &e, Entity &other, double vx, double vy, double ox, double oy, bool sameSide) {
    Obj &so = entities_.obj(e), &oo = entities_.obj(other);
    Hitbox &sh = *so.hit, &oh = *oo.hit;
    Body &sb = *so.body, &ob = *oo.body;
    bool flower = e.has(C_FLOWER);
    bool physics = true, weighted = false;
    if (sameSide && flower != other.has(C_FLOWER)) {
        if (flower || !sh.pushable) physics = false;
    } else if (!sh.pushable) {
        physics = false;
    } else {
        weighted = oh.pushable;
    }
    double p = double(sb.x) - double(ob.x), q = double(sb.y) - double(ob.y), r = p * p + q * q;
    if (physics) {
        if (weighted) {
            if (r > EPSILON) {
                double om = double(oh.radius);
                om = om * om * double(oh.density);
                double sm = double(sh.radius);
                sm = sm * sm * double(sh.density);
                double d = ((vx - ox) * p + (vy - oy) * q) * (om * -2.0) / (r * (om + sm));
                sb.vx = sb.vx + p * d;
                sb.vy = sb.vy + q * d;
            }
        } else {
            if (r > EPSILON) {
                double d = ((vx - ox) * p + (vy - oy) * q) * -2.0 / r;
                sb.vx = p * d + sb.vx;
                sb.vy = q * d + sb.vy;
            }
            double ex = double(sb.x) - double(ob.x), ey = double(sb.y) - double(ob.y);
            double d2 = ex * ex + ey * ey;
            double rr = double(sh.radius) + double(oh.radius);
            rr = rr * rr;
            if (d2 < rr) {
                double depth = std::sqrt(rr) - std::sqrt(d2);
                double len = std::sqrt(ex * ex + ey * ey), ux, uy;
                if (len < EPSILON) {
                    uy = 0;
                    ux = 1;
                } else {
                    uy = ey / len;
                    ux = ex / len;
                }
                double ny = depth * uy + double(sb.y), nx = depth * ux + double(sb.x);
                if (finite(nx) && finite(ny)) entities_.setPos(e, nx, ny);
            }
        }
        if (flower) {
            double d = std::sqrt(r), ex, ey;
            if (d < EPSILON) {
                ey = 0;
                ex = 0.2;
            } else {
                ey = q * 0.2 / d;
                ex = p * 0.2 / d;
            }
            sb.vx = ex + sb.vx;
            sb.vy = ey + sb.vy;
        }
    }
    if (sameSide || !e.has(C_HEALTH)) return;
    // flowers hurt each other less at low levels: nothing up to level 5, all of it from 10
    float scale = 1.0f;
    if (flower && other.has(C_FLOWER)) {
        int a = e.level(), b = other.level();
        float k = float(double(std::min(a, b) - 5) * 0.2);
        scale = k < 0.0f ? 0.0f : std::fmin(k, 1.0f);
    }
    game_.combat.damage(e, &other, scale * oh.damage);
}

}  // namespace game
