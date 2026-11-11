#include "game/Drops.h"

#include <cmath>

#include "Math.h"
#include "game/Diep.h"
#include "game/Game.h"

namespace game {
using namespace world;

Drops::Drops(Game &game) : System(game) {}

// whether a mythic petal is already out there: on a flower other than the dying one, or lying
// on the ground
bool Drops::mythicTaken(uint8_t type, const Entity &dying) {
    for (uint32_t id : game_.flowers.all) {
        Entity *f = world_.get(id);
        if (!f || f->id == dying.id) continue;
        const FlowerState &fs = game_.flowers.states[f->slot()];
        int n = slotsWithSpares(f->level());
        for (int d = 0; d < n; d++)
            if (fs.slots[d] == type) return true;
    }
    for (uint32_t id : all) {
        Entity *d = world_.get(id);
        if (d && entities_.obj(*d).drop && entities_.obj(*d).drop->type == type) return true;
    }
    return false;
}

// a dead mob's petals land where it died, fanned out when there are several, and its xp goes to
// whoever's flower killed it
void Drops::dropLoot(Entity &e, Entity *by) {
    Obj &o = entities_.obj(e);
    if (!o.loot) return;
    const Body src = *o.body;
    std::vector<uint8_t> loot = o.loot->petals;
    Entity *cam = by ? entities_.cameraOf(by) : nullptr;
    Entity *killer = cam ? world_.get(cam->camera->player) : nullptr;
    bool flowerKill = killer && killer->has(C_FLOWER);
    if (game_.tankMode && !flowerKill) loot.clear();
    size_t n = loot.size();
    for (size_t q = 0; q < n; q++) {
        uint8_t type = loot[q];
        Entity &d = entities_.create(0);  // taken before anything is checked, like the free slot it is
        if (!isPetal(type)) continue;
        const PetalDef &def = kPetals[type];
        if (def.rarity == 5 && mythicTaken(type, e)) continue;
        Body &b = entities_.body(d);
        Hitbox &h = entities_.hitbox(d);
        entities_.setRadius(d, 35);
        h.ghost = true;
        if (std::isfinite(double(src.x)) && std::isfinite(double(src.y))) entities_.setPos(d, src.x, src.y);
        b.vx = src.vx + src.ax;
        b.vy = src.vy + src.ay;
        d.add(C_DEATH);
        Obj &od = entities_.obj(d);
        od.drop = std::make_unique<DropState>();
        DropState &ds = *od.drop;
        d.add(C_DROP);
        all.push_back(d.id);
        static const double kLasts[4] = {42000, 52000, 62000, 62000};
        int r = int(def.rarity) - 2;
        double lasts = unsigned(r) <= 3 ? kLasts[r] : 32000.0;
        ds.spawnedAt = game_.now;
        ds.timed = true;
        ds.expireAt = lasts + game_.now;
        ds.type = type;
        d.drop->setType(type);
        if (n >= 2) {
            float a = float(double(q) * TAU / double(n));
            b.vx = b.vx + double(std::cos(a) * 0.6f);
            b.vy = b.vy + double(std::sin(a) * 0.6f);
        }
    }
    float xp = o.loot->xp;
    if (!(xp > 0.0f) || !cam) return;
    if (game_.tankMode && !flowerKill) {
        game_.tanks.giveScore(cam, xp * diep::kScorePerXp);
        return;
    }
    if (flowerKill) game_.flowers.giveXp(*killer, xp);
}

// Petals on the ground flash for their last two seconds and then go. Until then, from half a
// second after landing, the nearest live flower within 70 with a free slot takes one, and it
// flies into that flower for two seconds.
void Drops::tick(double dt) {
    double pull = 1.0 - std::pow(0.8, dt * 0.06);
    pull = pull < 0.0 ? 0.0 : std::fmin(pull, 1.0);
    for (size_t i = 0; i < all.size(); i++) {
        Entity *d = world_.get(all[i]);
        if (!d) continue;
        DropState &ds = *entities_.obj(*d).drop;
        if (!(ds.timed && ds.expireAt > game_.now + 2000.0)) d->drop->setFlags(d->drop->flags | 1);
        bool lying = ds.timed && game_.now < ds.expireAt;
        if (!lying) entities_.queueRemoval(*d);
        if (!(d->drop->flags & 1) && lying && !(game_.now < ds.spawnedAt + 500.0)) pickUp(*d, ds);

        Entity *to = world_.get(ds.collector);
        if (!to || !entities_.obj(*to).body) continue;
        const Body &b = *entities_.obj(*d).body, &t = *entities_.obj(*to).body;
        double x = b.x, tx = t.x, y = b.y, ty = t.y;
        x = std::fabs(x - tx) < 1e-07 ? tx : pull * (tx - x) + x;
        y = std::fabs(y - ty) < 1e-07 ? ty : pull * (ty - y) + y;
        if (std::isfinite(x) && std::isfinite(y)) entities_.setPos(*d, x, y);
    }
}

void Drops::pickUp(Entity &d, DropState &ds) {
    const Body &b = *entities_.obj(d).body;
    Entity *best = nullptr;
    double bestD = 0;
    game_.physics.tree.visit(b.x - 70.0f, b.y - 70.0f, 140.0f, 140.0f, [&](uint16_t s) {
        Entity &f = *world_.at(s);
        if (!f.has(C_HEALTH) || !f.has(C_POSITION) || !f.has(C_FLOWER)) return false;
        const FlowerState &fs = game_.flowers.states[s];
        int n = slotsWithSpares(f.level());
        bool room = false;
        for (int j = 0; j < n && !room; j++) room = !fs.slots[j];
        if (!room || f.hp() <= 0.0f) return false;
        const Body &fb = *entities_.obj(f).body;
        double dx = double(b.x) - double(fb.x), dy = double(b.y) - double(fb.y);
        double dd = dx * dx + dy * dy;
        if (!(dd < 4900.0) || (best && !(dd < bestD))) return false;
        bestD = dd;
        best = &f;
        return false;
    });
    if (!best) return;
    const FlowerState &fs = game_.flowers.states[best->slot()];
    int n = slotsWithSpares(best->level()), slot = -1;
    for (int j = 0; j < n; j++)
        if (!fs.slots[j]) {
            slot = j;
            break;
        }
    if (slot >= 0) game_.flowers.setSlot(*best, slot, ds.type);
    ds.collector = best->id;
    ds.timed = true;
    ds.expireAt = game_.now + 2000.0;
    d.drop->setFlags(d.drop->flags | 1);
    Entity *cam = entities_.cameraOf(best);
    if (!cam) return;
    for (auto &[id, c] : game_.clients.all) {
        if (c.camera != cam->id) continue;
        net::Writer w;
        w.varint(EVENT_PICKUP);
        w.ref(d.id);
        w.u8(uint8_t(slot));
        c.events.push_back({true, std::move(w.b)});
    }
}

}  // namespace game
