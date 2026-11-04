#include "game/StatusEffects.h"

#include <algorithm>
#include <cmath>

#include "game/Game.h"

namespace game {
using namespace world;

namespace {

float remaining(const Effect &fx, double now) {
    return fx.kind == E_WEB ? INFINITY : float(fx.until - now);
}
float speedOf(const Effect &fx) {
    return fx.kind == E_HONEY ? 0.95f : fx.kind == E_WEB ? 0.25f : 1.0f;
}
float spinOf(const Effect &fx) {
    return fx.kind == E_HONEY ? 0.7f : 1.0f;
}
float dpsOf(const Effect &fx) {
    return fx.kind == E_POISON ? 9.0f : 0.0f;
}

template <class F>
bool any(const Status &st, F f) {
    for (const Effect &fx : st.timed)
        if (f(fx)) return true;
    for (const Effect &fx : st.webs)
        if (f(fx)) return true;
    return false;
}

template <class F>
float product(const Status *st, F f) {
    float k = 1.0f;
    if (!st) return k;
    for (const Effect &fx : st->timed) k *= f(fx);
    for (const Effect &fx : st->webs) k *= f(fx);
    return k;
}

}  // namespace

StatusEffects::StatusEffects(Game &game) : System(game) {}

bool StatusEffects::noHeal(Entity &e) {
    const Status *st = entities_.obj(e).status.get();
    return st && any(*st, [](const Effect &fx) { return fx.kind == E_NO_HEAL; });
}

// how fast it can move, and how fast its petals turn
float StatusEffects::slowdown(Entity &e) {
    return product(entities_.obj(e).status.get(), speedOf);
}

float StatusEffects::spinFactor(Entity &e) {
    return product(entities_.obj(e).status.get(), spinOf);
}

void StatusEffects::afflict(Entity &e, EffectKind kind, double ms, uint32_t source) {
    Status *st = entities_.obj(e).status.get();
    if (!st) return;
    Effect fx{kind, game_.now + ms, source};
    if (remaining(fx, game_.now) > 0.0f) st->timed.push_back(fx);
}

// Each tick: worn-off effects go, the worst poison does its damage, and the flags say what's on.
void StatusEffects::tick(double dt) {
    float step = float(dt);
    for (size_t i = 0; i < affected.size(); i++) {
        Entity *e = world_.get(affected[i]);
        if (!e) continue;
        Status &st = *entities_.obj(*e).status;
        auto &t = st.timed;
        for (size_t k = 0; k < t.size();) {
            if (remaining(t[k], game_.now) > 0.0f) {
                k++;
                continue;
            }
            t[k] = t.back();
            t.pop_back();
        }
        int flags = e->status->flags;
        if (e->has(C_HEALTH)) {
            float worst = 0.0f;
            uint32_t by = 0;
            auto weigh = [&](const Effect &fx) {
                float left = remaining(fx, game_.now);
                float d = dpsOf(fx) * (left > step ? step : left);
                if (d > worst) {
                    worst = d;
                    by = fx.kind == E_POISON ? fx.source : 0;
                }
            };
            for (const Effect &fx : st.timed) weigh(fx);
            for (const Effect &fx : st.webs) weigh(fx);
            float amount = worst * 0.001f;
            if (amount > 0.0f) {
                e->status->setFlags(flags |= 1);
                bool quiet = st.flashed && game_.now - st.flashAt < 600.0;
                if (!quiet) {
                    st.flashed = true;
                    st.flashAt = game_.now;
                }
                game_.combat.damage(*e, world_.get(by), amount, quiet ? 1 : 0);
            } else {
                e->status->setFlags(flags &= ~1);
            }
        }
        flags = e->status->flags;
        e->status->setFlags(any(st, [](const Effect &fx) { return fx.kind == E_NO_HEAL; }) ? flags | 2 : flags & ~2);
        flags = e->status->flags;
        e->status->setFlags(!st.timed.empty() || !st.webs.empty() ? flags | 4 : flags & ~4);
    }
}

// A thrown web lands where it is and stays twelve seconds.
void StatusEffects::makeWeb(Entity &p) {
    Entity &w = entities_.create(bit(C_WEB) | bit(C_DEATH) | bit(C_CATCH));
    Hitbox &h = entities_.hitbox(w);
    entities_.setRadius(w, 50);
    h.still = true;
    entities_.life(w).fuse = 12000;
    entities_.setMaxHealth(w, 5);
    Body &b = entities_.body(w);
    b.unbounded = true;
    const Body &pb = entities_.body(p);
    if (std::isfinite(double(pb.x)) && std::isfinite(double(pb.y))) entities_.setPos(w, pb.x, pb.y);
    entities_.obj(w).web = std::make_unique<Web>();
    webs.push_back(w.id);
    entities_.setOwner(w, p.has(C_OWNED) ? entities_.obj(p).owner : 0);
}

// e leaves a web
void StatusEffects::release(Entity &e, uint32_t web) {
    Status *st = entities_.obj(e).status.get();
    if (!st) return;
    auto &v = st->webs;
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i].source != web) continue;
        v[i] = v.back();
        v.pop_back();
        return;
    }
}

// Each web: what's inside it now is caught, and what has left is let go.
void StatusEffects::tickWebs() {
    for (size_t i = 0; i < webs.size(); i++) {
        Entity *w = world_.get(webs[i]);
        if (!w) continue;
        Web &web = *entities_.obj(*w).web;
        std::vector<uint32_t> was;
        for (uint32_t id : web.caught)
            if (world_.get(id)) was.push_back(id);
        std::vector<uint32_t> now;
        if (!(w->has(C_HEALTH) && w->hp() <= 0.0f)) {
            const Body &wb = entities_.body(*w);
            float r = web.radius, wx = wb.x, wy = wb.y;
            game_.physics.tree.visit(wx - r, wy - r, r + r, r + r, [&](uint16_t s) {
                Entity &e = *world_.at(s);
                const Obj &o = entities_.objs[s];
                if (!e.has(C_STATUS) || !o.body) return false;
                float dx = o.body->x - wx, dy = o.body->y - wy, reach = r + o.hit->radius;
                if (!(dx * dx + dy * dy < reach * reach)) return false;
                if (o.mob && o.mob->type == M_SPIDER) return false;
                now.push_back(e.id);
                return false;
            });
        }
        auto bySlot = [&](uint32_t a, uint32_t b) { return world_.get(a)->slot() < world_.get(b)->slot(); };
        std::sort(was.begin(), was.end(), bySlot);
        std::sort(now.begin(), now.end(), bySlot);
        size_t a = 0, b = 0;
        while (a < was.size() || b < now.size()) {
            Entity *x = a < was.size() ? world_.get(was[a]) : nullptr,
                   *y = b < now.size() ? world_.get(now[b]) : nullptr;
            if (y && (!x || y->slot() < x->slot())) {
                entities_.obj(*y).status->webs.push_back({E_WEB, 0, w->id});
                b++;
            } else if (x && y && x->slot() == y->slot()) {
                a++;
                b++;
            } else {
                release(*x, w->id);
                a++;
            }
        }
        web.caught = std::move(now);
    }
}

}  // namespace game
