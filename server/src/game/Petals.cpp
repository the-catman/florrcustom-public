#include "game/Petals.h"

#include <algorithm>
#include <cmath>

#include "Math.h"
#include "game/Game.h"

namespace game {
using namespace world;

namespace {

enum Ring : uint8_t {
    R_PLAIN,
    R_ROSE,
    R_WING,
    R_MISSILE,
    R_DANDELION,
    R_PEAS,
    R_BUBBLE,
    R_POLLEN,
    R_EGG,
    R_WEB,
};

struct PetalKind {
    double radius;
    float damage, health;
    float attack = 130.0f, defend = 40.0f;  // ring radius when attacking, defending; 65 otherwise
    Ring ring = R_PLAIN;
    float density = 1.0f;
};

const PetalKind kKinds[38] = {
    /* 0 None */ {10, 10, 10},
    /* 1 Basic */ {10, 10, 10},
    /* 2 Fast */ {7, 8, 5, 145, 40},
    /* 3 Heavy */ {12, 20, 20, 120, 50},
    /* 4 Rock */ {14, 10, 90, 110, 55},
    /* 5 Square */ {10, 10, 10},
    /* 6 Rose */ {10, 5, 5, 65, 40, R_ROSE},
    /* 7 Stinger */ {7, 35, 8},
    /* 8 Iris */ {6, 5, 5, 140, 40},
    /* 9 Wing */ {15, 15, 15, 140, 40, R_WING},
    /* 10 Missile */ {10, 35, 10, 65, 40, R_MISSILE},
    /* 11 Peas */ {7, 8, 20, 65, 40, R_PEAS},
    /* 12 Peas */ {7, 8, 20, 65, 40, R_PEAS},
    /* 13 Cactus */ {15, 5, 15},
    /* 14 Faster */ {7, 8, 5, 145, 40},
    /* 15 Rose */ {10, 5, 5, 65, 40, R_ROSE},
    /* 16 Twin */ {7, 8, 5, 145, 40},
    /* 17 Triplet */ {7, 8, 5, 145, 40},
    /* 18 Bubble */ {12, 0, 1, 60, 60, R_BUBBLE},
    /* 19 Pollen */ {7, 8, 5, 65, 65, R_POLLEN},
    /* 20 Dandelion */ {9, 5, 10, 65, 40, R_DANDELION},
    /* 21 Cactus */ {15, 5, 15},
    /* 22 Egg */ {13, 1, 50, 65, 65, R_EGG},
    /* 23 Egg */ {15, 1, 50, 65, 65, R_EGG},
    /* 24 Rose */ {7, 5, 5, 65, 40, R_ROSE},
    /* 25 Antennae */ {10, 10, 10},
    /* 26 Heaviest */ {16, 10, 200, 100, 65, R_PLAIN, 4.0f},
    /* 27 Stinger */ {7, 35, 8},
    /* 28 Yin Yang */ {12, 15, 15},
    /* 29 Web */ {10, 8, 5, 65, 40, R_WEB},
    /* 30 Web */ {10, 8, 5, 65, 40, R_WEB},
    /* 31 Honey */ {10, 10, 10},
    /* 32 Leaf */ {10, 8, 10},
    /* 33 Cactus */ {15, 5, 15},
    /* 34 Peas */ {10, 12, 20, 65, 40, R_PEAS},
    /* 35 Basic */ {10, 10, 10},
    /* 36 Salt */ {10, 10, 10},
    /* 37 Rice */ {10, 5, 5},
};

const PetalKind &kindOf(uint8_t type) {
    return kKinds[isPetal(type) ? type : 0];
}

constexpr Petals::RingPlace kOnRing{65.0f, false};
constexpr Petals::RingPlace kDefending{40.0f, false};
constexpr Petals::RingPlace kGone{0.0f, true};  // moves itself now

// 1 - 0.8^(dt * 0.06): how far a petal drawn in by its flower gets this tick
double drawIn(double dt) {
    double k = 1.0 - std::pow(0.8, dt * 0.06);
    return k < 0.0 ? 0.0 : std::fmin(k, 1.0);
}

}  // namespace

Petals::Petals(Game &game) : System(game) {}

// the petal entity for slot i, copy k
void Petals::make(Entity &f, int i, int k) {
    FlowerState &fs = game_.flowers.states[f.slot()];
    uint8_t type = fs.slots[i];
    if (i > 7 || k > 2 || fs.petal[i][k] || !isPetal(type)) return;
    if (slotCount(f.level()) <= i) return;
    const PetalDef &def = kPetals[type];
    if (def.passive) return;  // passive: no entity
    if (def.count <= k) return;
    Entity &p = entities_.create(bit(C_PETAL) | bit(C_DEATH));
    entities_.setOwner(p, f.id);
    Body &fb = entities_.body(f);
    entities_.setPos(p, fb.x, fb.y);
    entities_.body(p).unbounded = true;
    PetalState &ps = states[p.slot()];
    ps = PetalState();
    ps.flower = f.id;
    ps.slot = int8_t(i);
    ps.copy = int8_t(k);
    init(p, type);
    all.push_back(p.id);
    fs.petal[i][k] = p.id;
    fs.reload[i][k] = 0;
    fs.swapped[i][k] = false;
    place(f);
}

// Places on the petal ring, in slot order. A missing copy keeps its place, the copies of a
// cluster share one, and a slot held by something that isn't a petal (a hatched pet) has none.
void Petals::place(Entity &f) {
    FlowerState &fs = game_.flowers.states[f.slot()];
    int n = slotCount(f.level()), b = 0;
    // 1: a petal, now at b; 0: something else; -1: nothing there
    auto place = [&](uint32_t id) {
        Entity *p = world_.get(id);
        if (!p) return -1;
        auto it = states.find(p->slot());
        if (!p->has(C_PETAL) || it == states.end()) return 0;
        it->second.index = b;
        return 1;
    };
    for (int s = 0; s < 8; s++) {
        uint8_t type = fs.slots[s];
        if (isPetal(type) && !(kPetals[type].passive)) {
            int count = kPetals[type].count;
            if (kPetals[type].clustered()) {
                int c = 0;
                for (int k = 0; k < 3; k++) {
                    int r = place(fs.petal[s][k]);
                    if (r == 1 && k == 2) {
                        c = 1;
                        break;
                    }
                    if (r == 1)
                        c = 1;
                    else if (r < 0)
                        c |= count > k;
                }
                b += c;
            } else {
                for (int k = 0; k < 3; k++) {
                    int r = place(fs.petal[s][k]);
                    if (r == 1)
                        b++;
                    else if (r < 0)
                        b += count > k;
                }
            }
        }
        if (n == s + 1) fs.ring = uint8_t(b);
    }
}

// Petals chase their place on a circle that turns with the flower. One without a living flower
// dies.
void Petals::move(double dt) {
    for (size_t i = 0; i < all.size(); i++) {
        Entity *p = world_.get(all[i]);
        if (!p || !entities_.obj(*p).body) continue;
        uint16_t slot = p->slot();
        float hp = p->hp();
        if (hp <= 0.0f) continue;
        Entity *f = p->has(C_OWNED) ? world_.get(entities_.obj(*p).owner) : nullptr;
        if (!f || (f->has(C_HEALTH) && f->hp() <= 0.0f) || !f->has(C_FLOWER)) {
            game_.combat.damage(*p, nullptr, hp, 1);
            continue;
        }
        PetalState &ps = states[slot];
        RingPlace place = petalRing(*p, ps, *f, float(dt));
        if (place.skip) continue;
        p = world_.get(all[i]);
        if (!p) continue;
        const Body &fb = entities_.body(*f);
        FlowerState &fs = game_.flowers.states[f->slot()];
        uint8_t type = uint8_t(p->petalType());
        int ring = fs.ring > 1 ? fs.ring : 1;
        double rb = double(uint32_t(ps.index)) * TAU / double(ring) + double(fs.rot);
        double a = std::fmod(rb, TAU), radius = double(place.radius);
        double tx = fb.x + std::cos(a) * radius, ty = fb.y + std::sin(a) * radius;
        if (isPetal(type) && (kPetals[type].clustered())) {
            double c =
                std::fmod(rb * 1.1 + double(uint32_t(ps.copy)) * TAU / double(uint32_t(kPetals[type].count)), TAU);
            double cr = double(kPetals[type].cluster);
            tx += std::cos(c) * cr;
            ty += std::sin(c) * cr;
        }
        Body &b = *entities_.objs[slot].body;
        double dx = tx - double(b.x), dy = ty - double(b.y), d2 = dx * dx + dy * dy, d = std::sqrt(d2);
        float pull = std::clamp(float(d2 * (1.0 / 900)), 0.01f, 2.0f);
        double s = dt * 0.007 * double(pull);
        double ux = 1, uy = 0;
        if (!(d < EPSILON)) {
            ux = dx / d;
            uy = dy / d;
        }
        b.vx += s * ux;
        b.vy += s * uy;
    }
}

// Gives a new petal entity its type: health, a hitbox that leaves its own side alone, and the
// kind's numbers.
void Petals::init(Entity &p, uint8_t type) {
    p.petal->setType(type);
    entities_.life(p);
    Hitbox &h = entities_.hitbox(p);
    h.hitsTeam = false;
    const PetalKind &k = kindOf(type);
    entities_.setRadius(p, float(k.radius));
    h.damage = k.damage;
    h.density = k.density;
    entities_.setMaxHealth(p, k.health);
    states[p.slot()].born = game_.now;
}

// A petal leaves its slot, which starts reloading.
void Petals::unslot(Entity &p) {
    const PetalState &ps = states[p.slot()];
    Entity *f = p.has(C_OWNED) ? world_.get(entities_.obj(p).owner) : nullptr;
    if (!f || !f->has(C_FLOWER) || ps.slot < 0 || ps.slot > 7 || ps.copy < 0 || ps.copy > 2) return;
    FlowerState &fs = game_.flowers.states[f->slot()];
    if (fs.petal[ps.slot][ps.copy] != p.id) return;
    fs.petal[ps.slot][ps.copy] = 0;
    place(*f);
}

// Where on the ring a petal goes (its distance from the flower), or that it moves itself. The
// special kinds act here too.
Petals::RingPlace Petals::petalRing(Entity &p, PetalState &ps, Entity &f, float dt) {
    const PetalKind &k = kindOf(uint8_t(p.petalType()));
    int face = f.flower->face;
    switch (k.ring) {
        case R_PLAIN:
            if (face & 1) return {k.attack, false};
            if (face & 2) return {k.defend, false};
            return kOnRing;
        case R_WING: return wingRing(ps, face);
        case R_ROSE: return roseRing(p, ps, f, dt);
        case R_MISSILE:
        case R_DANDELION: return launchRing(p, ps, f);
        case R_WEB: return webRing(p, ps, f);
        case R_BUBBLE: return bubbleRing(p, ps, f);
        case R_POLLEN: return pollenRing(p, ps, f);
        case R_EGG: return eggRing(p, ps, f);
        case R_PEAS: return peasRing(p, ps, f);
    }
    return kOnRing;
}

bool Petals::olderThan(const PetalState &ps, double ms) const {
    return game_.now - ps.born > ms;
}

Petals::RingPlace Petals::wingRing(const PetalState &ps, int face) {
    float s = float(std::sin((game_.now - ps.born) * 0.005));
    if (face & 1) return {s * 50.0f + 140.0f, false};
    if (face & 2) return {s * 5.0f + 40.0f, false};
    return {s * 10.0f + 65.0f, false};
}

// once the flower is hurt the rose flies into it and heals it
Petals::RingPlace Petals::roseRing(Entity &p, PetalState &ps, Entity &f, float dt) {
    if (!ps.heal && olderThan(ps, 1000)) {
        float hp = f.hp();
        if (hp > 0.0f && hp < f.maxHp() && !game_.effects.noHeal(f) && entities_.cameraOf(&f)) ps.heal = 1;
    }
    if (!ps.heal) return f.flower->face & 2 ? kDefending : kOnRing;
    const Body &pb = entities_.body(p), &fb = entities_.body(f);
    double pull = drawIn(double(dt));
    double x = pb.x, tx = fb.x, y = pb.y, ty = fb.y;
    if (!(std::fabs(x - tx) < 1e-07)) tx = (tx - x) * pull + x;
    if (!(std::fabs(y - ty) < 1e-07)) ty = (ty - y) * pull + y;
    if (std::isfinite(tx) && std::isfinite(ty)) entities_.setPos(p, tx, ty);
    if (++ps.heal == 10) {
        uint8_t type = uint8_t(p.petalType());
        game_.combat.heal(f, type == P_ROSE ? 10.0f : type == P_EPIC_ROSE ? 22.0f : 3.5f);
        game_.combat.damage(p, nullptr, p.hp(), 1);
    }
    return {f.flower->face & 2 ? 40.0f : 65.0f, true};
}

// points away from the flower, and once attacking flies off that way
Petals::RingPlace Petals::launchRing(Entity &p, PetalState &ps, Entity &f) {
    if (ps.used) return kGone;
    int face = f.flower->face;
    Body &pb = entities_.body(p);
    const Body &fb = entities_.body(f);
    double dx = double(pb.x) - double(fb.x), dy = double(pb.y) - double(fb.y);
    entities_.setAngle(p, std::fmod(double(float(std::atan2(dy, dx))), TAU));
    if (!(face & 1) || !olderThan(ps, 500)) return face & 2 ? kDefending : kOnRing;
    ps.used = true;
    double d = std::sqrt(dx * dx + dy * dy), ux = 1, uy = 0;
    if (!(d < EPSILON)) {
        ux = dx / d;
        uy = dy / d;
    }
    if (kindOf(uint8_t(p.petalType())).ring == R_MISSILE) {
        pb.vx += ux * 1.4;
        pb.vy += uy * 1.4;
        pb.ax += ux * 0.6;
        pb.ay += uy * 0.6;
    } else {
        ux = double(float(ux));
        uy = double(float(uy));
        pb.vx += double(float(ux * 1.4));
        pb.vy += double(float(uy * 1.4));
        pb.ax += double(float(ux * 0.6));
        pb.ay += double(float(uy * 0.6));
    }
    unslot(p);
    entities_.life(p).fuse = 1500.0f;
    return kOnRing;
}

Petals::RingPlace Petals::webRing(Entity &p, PetalState &ps, Entity &f) {
    if (ps.used) return kGone;
    int face = f.flower->face;
    if (!(face & 1) || !olderThan(ps, 500)) return face & 2 ? kDefending : kOnRing;
    ps.used = true;
    Body &pb = entities_.body(p);
    const Body &fb = entities_.body(f);
    double dx = double(pb.x) - double(fb.x), dy = double(pb.y) - double(fb.y);
    double d = std::sqrt(dx * dx + dy * dy), ux = double(2.2f), uy = 0;
    if (!(d < EPSILON)) {
        ux = double(float(double(float(dx / d)) * 2.2));
        uy = double(float(double(float(dy / d)) * 2.2));
    }
    pb.vx += ux;
    pb.vy += uy;
    unslot(p);
    entities_.life(p).fuse = 650.0f;
    return kOnRing;
}

// defending pops it and pushes the flower away from where it was
Petals::RingPlace Petals::bubbleRing(Entity &p, PetalState &ps, Entity &f) {
    if (ps.used) return kGone;
    RingPlace onRing{60.0f, false};
    if (!(f.flower->face & 2) || !olderThan(ps, 500)) return onRing;
    ps.used = true;
    const Body &pb = entities_.body(p);
    Body &fb = entities_.body(f);
    double dx = double(fb.x) - double(pb.x), dy = double(fb.y) - double(pb.y);
    double d = std::sqrt(dx * dx + dy * dy), ux = 1.4, uy = 0;
    if (!(d < EPSILON)) {
        ux = dx * 1.4 / d;
        uy = dy * 1.4 / d;
    }
    fb.vx += ux;
    fb.vy += uy;
    game_.combat.damage(p, nullptr, p.hp(), 1);
    return onRing;
}

// attacking throws it out, defending drops it behind
Petals::RingPlace Petals::pollenRing(Entity &p, PetalState &ps, Entity &f) {
    if (ps.used) return kGone;
    int face = f.flower->face;
    if (!(face & 3) || !olderThan(ps, 500)) return kOnRing;
    ps.used = true;
    Body &pb = entities_.body(p);
    const Body &fb = entities_.body(f);
    double dx = double(pb.x) - double(fb.x), dy = double(pb.y) - double(fb.y);
    double d = std::sqrt(dx * dx + dy * dy), ux = 0.5, uy = 0;
    if (!(d < EPSILON)) {
        ux = dx * 0.5 / d;
        uy = dy * 0.5 / d;
    }
    double s = (face & 1) ? 1.0 : -1.0;
    pb.vx += ux * s;
    pb.vy += uy * s;
    unslot(p);
    entities_.life(p).fuse = rng_.real(4500.0f, 5500.0f);
    return kOnRing;
}

// hatches into a pet that keeps to its flower and holds the slot while it lives
Petals::RingPlace Petals::eggRing(Entity &p, PetalState &ps, Entity &f) {
    if (ps.used) return {65.0f, true};
    uint8_t type = uint8_t(p.petalType());
    if (!olderThan(ps, type == P_EGG ? 1000 : 2500)) return kOnRing;
    ps.used = true;
    const Body &pb = entities_.body(p);
    Entity *m = game_.mobs.make(type == P_EGG ? M_SOLDIER_ANT : M_BEETLE, pb.x, pb.y, -1);
    if (!m) return kOnRing;
    Entity *owner = p.has(C_OWNED) ? world_.get(entities_.obj(p).owner) : nullptr;
    if (owner && owner->has(C_FLOWER) && ps.slot >= 0 && ps.slot <= 7 && ps.copy >= 0 && ps.copy <= 2) {
        FlowerState &fs = game_.flowers.states[owner->slot()];
        uint32_t &held = fs.petal[ps.slot][ps.copy];
        if (held == p.id) {
            held = m->id;
            place(*owner);
        }
    }
    entities_.setOwner(*m, owner ? owner->id : 0);
    if (Loot *l = entities_.obj(*m).loot.get()) {
        l->petals.clear();
        l->xp = 0;
    }
    if (Ai *ai = entities_.obj(*m).ai.get()) {
        ai->leader = owner ? owner->id : 0;
        ai->p.sight = 1000.0f;
        ai->p.leash = 300.0f;
        ai->p.guard = 1000.0f;
    }
    game_.combat.damage(p, nullptr, p.hp(), 1);
    return kOnRing;
}

// attacking bursts it into four peas that fly apart
Petals::RingPlace Petals::peasRing(Entity &p, PetalState &ps, Entity &f) {
    if (ps.used) return kGone;
    int face = f.flower->face;
    if (!(face & 1) || !olderThan(ps, 500)) return face & 2 ? kDefending : kOnRing;
    ps.used = true;
    burst(p);
    return kOnRing;
}

void Petals::burst(Entity &p) {
    uint8_t type = uint8_t(p.petalType());
    const Body &pb = entities_.body(p);
    std::vector<uint32_t> peas;
    uint32_t owner = p.has(C_OWNED) ? entities_.obj(p).owner : 0;
    for (int n = 0; n < 4; n++) {
        Entity &q = entities_.create(0);
        entities_.setOwner(q, owner);
        Body &qb = entities_.body(q);
        qb.unbounded = true;
        if (std::isfinite(double(pb.x)) && std::isfinite(double(pb.y))) entities_.setPos(q, pb.x, pb.y);
        q.add(C_PETAL);
        all.push_back(q.id);
        PetalState &qs = states[q.slot()];
        qs = PetalState();
        qs.flower = owner;
        init(q, type);
        q.add(C_DEATH);
        qs.used = true;
        peas.push_back(q.id);
    }
    float start = rng_.angle();
    float a = start;
    for (uint32_t id : peas) {
        a = float(TAU / double(peas.size()) + double(a));
        Entity &q = *world_.get(id);
        q.add(C_PEA);
        int pea = q.pea->flags;
        pea = type == P_PEAS ? pea & ~3 : type == P_POISON_PEAS ? (pea | 1) & ~2 : pea | 3;
        q.pea->setFlags(pea);
        entities_.setMaxHealth(q, 5.0f);
        entities_.life(q).fuse = 1000.0f;
        Body &qb = entities_.body(q);
        double y = double(std::sin(a) * 8.485281f) + double(qb.y);
        double x = double(std::cos(a) * 8.485281f) + double(qb.x);
        if (std::isfinite(x) && std::isfinite(y)) entities_.setPos(q, x, y);
    }
    game_.after(1.0, [this, peas, start] {
        float b = start;
        for (uint32_t id : peas) {
            b = float(TAU / double(peas.size()) + double(b));
            Entity *q = world_.get(id);
            if (!q || !entities_.obj(*q).body) continue;
            Body &qb = *entities_.obj(*q).body;
            double c = double(std::cos(b)), s = double(std::sin(b));
            qb.vx += double(float(c * 1.4));
            qb.vy += double(float(s * 1.4));
            qb.ax += double(float(c * 0.6));
            qb.ay += double(float(s * 0.6));
        }
    });
    unslot(p);
    entities_.queueRemoval(p);
}

// Who a petal works for: the flower its player is looking at, or that player's camera, or the
// petal itself when no player is behind it.
uint32_t Petals::creditOf(Entity *p) {
    if (!p) return 0;
    for (Entity *e = p; e;) {
        if (e->has(C_CAMERA)) {
            if (Entity *f = world_.get(e->camera->player)) return f->id;
            return e->id;
        }
        if (!e->has(C_OWNED)) break;
        e = world_.get(entities_.obj(*e).owner);
    }
    return p->id;
}

// what touching something does, for the kinds that do more than hit it
void Petals::contact(Entity &p, Entity &other) {
    switch (p.petalType()) {
        case P_IRIS: game_.effects.afflict(other, E_POISON, 6666.666666666667, creditOf(&p)); break;
        case P_POISON_PEAS: game_.effects.afflict(other, E_POISON, 888.8888888888889, creditOf(&p)); break;
        case P_DANDELION: game_.effects.afflict(other, E_NO_HEAL, 10000.0); break;
        case P_POISON_CACTUS: game_.effects.afflict(other, E_POISON, 666.6666666666667, creditOf(&p)); break;
        case P_HONEY: game_.effects.afflict(other, E_HONEY, 4000.0); break;
        case P_BIG_PEAS: game_.effects.afflict(other, E_POISON, 1777.7777777777778, creditOf(&p)); break;
        default: break;
    }
}

// and what a flower touching something does because of a petal it has
void Petals::flowerContact(Entity &, Entity &p, Entity &other) {
    if (p.petalType() == P_POISON_CACTUS) game_.effects.afflict(other, E_POISON, 4444.444444444444, creditOf(&p));
}

// what dying does, for the kinds that do something then
void Petals::onDeath(Entity &p) {
    int type = p.petalType();
    auto it = states.find(p.slot());
    if ((type == P_WEB || type == P_WEB_TRIO) && it != states.end() && it->second.used) game_.effects.makeWeb(p);
}

}  // namespace game
