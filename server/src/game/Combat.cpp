#include "game/Combat.h"

#include "game/Game.h"

namespace game {
using namespace world;

Combat::Combat(Game &game) : System(game) {}

// flags: low byte set for no hit flash, 256 for damage that was itself handed back
void Combat::damage(Entity &e, Entity *by, float amount, uint16_t flags) {
    if (!e.has(C_HEALTH)) return;
    float hp = e.hp();
    if (hp <= 0.0f) return;
    // while the arena isn't running only damage from nobody counts
    bool counts = !by || game_.arena.running();
    float dealt = counts ? amount : 0.0f;
    float left = hp - dealt;
    if (TankState *ts = entities_.obj(e).tank.get()) ts->lastHit = game_.now;
    e.health->setHealth(left);
    if (left < 0.0f) e.health->setHealth(0.0f);
    uint8_t quiet = counts ? uint8_t(flags & 255) : 1;
    game_.mobAi.provoke(e, by);

    // salt hands a quarter of it back to the flower behind the attacker
    if (e.has(C_FLOWER) && !game_.flowers.zoneLocked(e) &&
        game_.flowers.countAlive(game_.flowers.states[e.slot()], e.level(), 36) && flags <= 255 && by) {
        Entity *d = by;
        while (d && !d->has(C_FLOWER)) d = d->has(C_OWNED) ? world_.get(entities_.obj(*d).owner) : nullptr;
        if (d && d->has(C_HEALTH)) damage(*d, &e, dealt * 0.25f, uint16_t(quiet | 256));
    }

    // a flash for whoever can see it, once a tick
    if (e.has(C_DEATH) && !quiet) {
        Life &l = entities_.life(e);
        if (l.hitTick != game_.tickNo) {
            l.hitTick = game_.tickNo;
            for (auto &[id, c] : game_.clients.all) {
                if (!c.view.knows(e.id)) continue;
                net::Writer w;
                w.varint(EVENT_HIT);
                w.ref(e.id);
                c.events.push_back({false, std::move(w.b)});
            }
        }
    }

    if (e.hp() > 0.0f) return;
    Life &l = entities_.life(e);
    l.dead = true;
    l.timed = true;
    l.removeAt = game_.now + 1000;
    if (e.death) e.death->setDead(true);
    Obj &o = entities_.obj(e);
    if (o.body) {
        if (e.has(C_FLOWER)) game_.flowers.onDeath(e);
        game_.drops.dropLoot(e, by);
    }
    if (o.tank || o.shot) game_.tanks.onDeath(e, by);
    if (game_.tankMode && o.mob) game_.tanks.onKill(e, by);
    game_.mobs.detachSegments(e);
    game_.mobAi.provoke(e, by);
    if (e.has(C_PETAL)) game_.petals.onDeath(e);
    if (e.has(C_FLOWER)) {
        FlowerState &fs = game_.flowers.states[e.slot()];
        for (int s = 0; s < 8; s++)
            for (int k = 0; k < 3; k++) {
                Entity *p = world_.get(fs.petal[s][k]);
                if (!p) continue;
                if (p->has(C_HEALTH))
                    damage(*p, by, p->hp(), 1);
                else
                    entities_.queueRemoval(*p);
            }
        game_.flowers.deathScreen(e, by);
    }
}

void Combat::heal(Entity &e, float amount) {
    float hp = e.hp();
    if (!(hp > 0.0f) || game_.effects.noHeal(e)) return;
    if (e.has(C_FLOWER) && !entities_.cameraOf(&e)) return;
    float v = hp + amount;
    if (v < 0.0f) v = 0.0f;
    float max = e.maxHp();
    e.health->setHealth(v > max ? max : v);
}

// burnt-out fuses, and the dead whose second is up
void Combat::reapDead(double dt) {
    for (size_t i = 0; i < healths.size(); i++) {
        Entity *e = world_.get(healths[i]);
        if (!e) continue;
        Life &l = *entities_.obj(*e).life;
        float hp = e->hp();
        if (hp > 0.0f && l.fuse > 0.0f) {
            float left = float(double(l.fuse) - dt);
            if (left <= 0.0f) {
                l.fuse = 0;
                damage(*e, nullptr, hp, 1);
            } else {
                l.fuse = left;
            }
        }
        if (!l.dead) continue;
        if (l.timed && game_.now < l.removeAt) continue;
        entities_.queueRemoval(*e);
    }
}

}  // namespace game
