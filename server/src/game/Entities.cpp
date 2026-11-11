#include "game/Entities.h"

#include <algorithm>

#include "game/Game.h"

namespace game {
using namespace world;

// the lists are kept unordered: the last one moves into the gap
template <class T>
static void unlist(std::vector<T> &v, T x) {
    auto it = std::find(v.begin(), v.end(), x);
    if (it != v.end()) {
        *it = v.back();
        v.pop_back();
    }
}

Entities::Entities(Game &game) : objs(world::kSlots), game_(game) {}

Entity &Entities::create(uint64_t comps) {
    Entity &e = world.create(comps);
    objs[e.slot()] = Obj();
    if (comps & bit(C_STATUS)) {
        objs[e.slot()].status = std::make_unique<Status>();
        game_.effects.affected.push_back(e.id);
    }
    return e;
}

void Entities::destroy(uint32_t id) {
    Entity *e = world.get(id);
    if (!e) return;
    Obj &o = objs[e->slot()];
    game_.mobs.detachSegments(*e);
    if (o.mob && o.mob->zone >= 0 && o.mob->zone <= 7) game_.mobs.zoneCount[o.mob->zone]--;
    if (o.ai) unlist(game_.mobAi.thinkers, id);
    if (o.life) unlist(game_.combat.healths, id);
    if (o.drop) unlist(game_.drops.all, id);
    if (o.status) unlist(game_.effects.affected, id);
    if (o.web) {
        for (uint32_t c : o.web->caught)
            if (Entity *ce = world.get(c)) game_.effects.release(*ce, id);
        unlist(game_.effects.webs, id);
    }
    if (e->has(C_FLOWER)) unlist(game_.flowers.all, id);
    if (o.tank || o.shot) game_.tanks.forget(*e);
    if (e->has(C_PETAL)) unlist(game_.petals.all, id);
    for (Mobs::Hole &h : game_.mobs.holes)
        if (h.id == id) {
            h = game_.mobs.holes.back();
            game_.mobs.holes.pop_back();
            break;
        }
    if (o.hit) {
        game_.physics.tree.remove(e->slot());
        unlist(game_.physics.sized, e->slot());
    }
    // what it owned now answers to its owner
    std::vector<uint32_t> owned = o.owned;
    for (uint32_t c : owned)
        if (Entity *ce = world.get(c)) setOwner(*ce, o.owner);
    if (Entity *owner = world.get(o.owner)) {
        auto &v = objs[owner->slot()].owned;
        v.erase(std::remove(v.begin(), v.end(), id), v.end());
    }
    game_.flowers.states.erase(e->slot());
    game_.petals.states.erase(e->slot());
    o = Obj();
    world.destroy(id);
}

void Entities::setOwner(Entity &e, uint32_t owner) {
    Obj &o = obj(e);
    if (Entity *old = world.get(o.owner)) {
        auto &v = objs[old->slot()].owned;
        v.erase(std::remove(v.begin(), v.end(), e.id), v.end());
    }
    o.owner = owner;
    if (Entity *n = world.get(owner)) objs[n->slot()].owned.push_back(e.id);
    e.add(C_OWNED);
    e.owned->setOwner(owner);
}

Body &Entities::body(Entity &e) {
    Obj &o = obj(e);
    if (!o.body) {
        o.body = std::make_unique<Body>();
        e.add(C_POSITION);
    }
    return *o.body;
}

Hitbox &Entities::hitbox(Entity &e) {
    Obj &o = obj(e);
    if (!o.hit) {
        o.hit = std::make_unique<Hitbox>();
        e.add(C_SIZE);
        game_.physics.sized.push_back(e.slot());
    }
    return *o.hit;
}

void Entities::setPos(Entity &e, double x, double y) {
    Body &b = body(e);
    b.x = float(x);
    b.y = float(y);
    e.position->setX(b.x);
    e.position->setY(b.y);
}

void Entities::setAngle(Entity &e, double a) {
    body(e).angle = a;
    e.position->setAngle(a);
}

void Entities::setRadius(Entity &e, float r) {
    hitbox(e).radius = r;
    e.size->setRadius(r);
}

void Entities::setMaxHealth(Entity &e, float max) {
    e.add(C_HEALTH);
    life(e);
    float old = e.maxHp(), hp = e.hp();
    hp = old <= 0 ? max : hp * max / old;
    e.health->setHealth(hp);
    e.health->setMaxHealth(max);
}

Life &Entities::life(Entity &e) {
    Obj &o = obj(e);
    if (!o.life) {
        o.life = std::make_unique<Life>();
        game_.combat.healths.push_back(e.id);
    }
    return *o.life;
}

// the camera something answers to, through its owners
Entity *Entities::cameraOf(Entity *e) {
    while (e && !e->has(C_CAMERA)) {
        if (!e->has(C_OWNED)) return nullptr;
        e = world.get(obj(*e).owner);
    }
    return e;
}

// the first thing up from the attacker with a name and a place in the world, or the attacker
Entity *Entities::killerOf(Entity *by) {
    for (Entity *d = by; d;) {
        if (d->has(C_NAME) && d->has(C_POSITION)) return d;
        if (!d->has(C_OWNED)) break;
        d = world.get(obj(*d).owner);
    }
    return by;
}

void Entities::queueRemoval(Entity &e) {
    Obj &o = obj(e);
    if (o.removing) return;
    o.removing = true;
    removals.push_back(e.id);
}

// whoever is ultimately behind an entity: a flower's camera, a petal's flower's camera, the
// arena for mobs
uint32_t Entities::root(uint32_t id) {
    Entity *e = world.get(id);
    while (e && e->has(C_OWNED)) {
        Entity *o = world.get(obj(*e).owner);
        if (!o) break;
        e = o;
    }
    return e ? e->id : 0;
}

}  // namespace game
