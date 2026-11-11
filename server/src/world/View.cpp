#include "world/View.h"

#include <algorithm>

namespace world {

static bool bySlot(uint32_t a, uint32_t b) {
    return (a & 16383) < (b & 16383);
}
static bool contains(const std::vector<uint32_t> &ids, uint32_t id) {
    auto it = std::lower_bound(ids.begin(), ids.end(), id, bySlot);
    return it != ids.end() && *it == id;
}

void writeFull(net::Writer &w, const Entity &e, bool yours) {
    if (e.drop) {
        w.u8(e.drop->type);
        w.u8(e.drop->flags);
    }
    if (e.moves) w.u8(e.moves->state);
    if (e.pea) w.u8(e.pea->flags);
    if (e.camera) {
        const CameraData &c = *e.camera;
        w.u8(c.respawnLevel);
        w.f32(c.x);
        w.str(c.killer);
        for (int petal : c.kept.values) w.u8(petal);
        w.u8(0);
        w.f32(c.score);
        w.f32(c.y);
        w.f32(c.fov);
        w.u8(c.keptCount);
        w.u8(c.deathLevel);
        w.ref(c.player);
    }
    if (e.petal) w.u8(e.petal->type);
    if (e.ant) w.u8(e.ant->kind);
    if (e.mob) w.u8(e.mob->type);
    if (e.owned) {
        w.ref(0);
        w.ref(e.owned->owner);
    }
    if (e.arena) {
        const ArenaData &a = *e.arena;
        for (uint32_t flower : a.leaderFlowers.values) w.ref(flower);
        for (const std::string &name : a.leaderNames.values) w.str(name);
        w.u8(a.state);
        w.u8(a.mode);
        w.f32(a.width);
        w.varint(a.seed);
        for (uint32_t score : a.leaderScores.values) w.varint(score);
        w.f32(a.height);
        w.f32(a.time);
        w.u8(a.leaderRows);
        w.varint(a.players);
    }
    if (e.name) w.str(e.name->name);
    if (e.health) {
        w.f32(e.health->health);
        w.f32(e.health->maxHealth);
    }
    if (e.position) {
        w.u16(e.position->angle);
        w.f32(e.position->y);
        w.f32(e.position->x);
    }
    if (e.segment) {
        w.u8(e.segment->flags);
        w.bit(false);
    }
    if (e.death) w.bit(e.death->dead);
    if (e.size) w.f32(e.size->radius);
    if (e.status) w.u8(e.status->flags);
    if (e.tank) {
        const TankData &t = *e.tank;
        w.u8(t.layout);
        w.varint(t.fired);
        for (uint16_t a : t.turrets.values) w.u16(a);
        w.u16(t.spin);
        w.u8(t.flags);
        w.u8(t.alpha);
        w.ref(t.team);
        w.u8(t.color);
        w.f32(t.score);
    }
    if (e.shot) {
        w.u8(e.shot->kind);
        w.ref(e.shot->team);
        w.u8(e.shot->look);
        w.u8(e.shot->color);
        w.u8(e.shot->mob);
    }
    if (e.driver) {
        const DriverData &d = *e.driver;
        w.u8(d.tank);
        w.u8(d.level);
        w.f32(d.score);
        for (int v : d.stats.values) w.u8(v);
        w.u8(d.points);
        w.f32(d.alive);
        w.u8(d.deathTank);
    }
    if (e.flower) {
        const FlowerData &f = *e.flower;
        w.u8(f.eyes);
        w.u8(0);
        w.u8(f.face);
        if (!yours) return;
        w.varint(f.xp);
        w.f32(f.score);
        for (int type : f.slots.values) w.u8(type);
        for (uint32_t cooldown : f.cooldowns.values) w.varint(cooldown);
        w.u8(f.level);
        w.u8(f.zoneTimer);
    }
}

// the changed elements of a list: (index step ^ 1) then the element, and 1 to end
template <class T, size_t N, class Write>
static void writeChanged(net::Writer &w, const WireList<T, N> &list, Write write) {
    int last = 0;
    for (int i = 0; i < int(N); i++) {
        if (!(list.changed >> i & 1)) continue;
        w.varint(uint32_t(i + 1 - last) ^ 1);
        last = i + 1;
        write(list[i]);
    }
    w.varint(1);
}

static void writeField(net::Writer &w, const Entity &e, int id) {
    auto u8 = [&](int v) { w.u8(v); };
    auto varint = [&](uint32_t v) { w.varint(v); };
    auto ref = [&](uint32_t v) { w.ref(v); };
    auto str = [&](const std::string &v) { w.str(v); };
    switch (id) {
        case wireId(F_RESPAWN_LEVEL): w.u8(e.camera->respawnLevel); break;
        case F_EYES: w.u8(e.flower->eyes); break;
        case F_LB_FLOWERS: writeChanged(w, e.arena->leaderFlowers, ref); break;
        case F_CAMERA_X: w.f32(e.camera->x); break;
        case F_KILLER: w.str(e.camera->killer); break;
        case F_RADIUS: w.f32(e.size->radius); break;
        case F_ANGLE: w.u16(e.position->angle); break;
        case F_PEA_FLAGS: w.u8(e.pea->flags); break;
        case F_LB_NAMES: writeChanged(w, e.arena->leaderNames, str); break;
        case F_ARENA_STATE: w.u8(e.arena->state); break;
        case F_STATUS_FLAGS: w.u8(e.status->flags); break;
        case F_KEPT: writeChanged(w, e.camera->kept, u8); break;
        case F_ANT_KIND: w.u8(e.ant->kind); break;
        case F_MAP_WIDTH: w.f32(e.arena->width); break;
        case F_SCORE: w.f32(e.camera->score); break;
        case F_DROP_TYPE: w.u8(e.drop->type); break;
        case F_CAMERA_Y: w.f32(e.camera->y); break;
        case F_Y: w.f32(e.position->y); break;
        case F_SEGMENT_FLAGS: w.u8(e.segment->flags); break;
        case F_ARENA_SEED: w.varint(e.arena->seed); break;
        case F_FACE: w.u8(e.flower->face); break;
        case F_FOV: w.f32(e.camera->fov); break;
        case F_KEPT_COUNT: w.u8(e.camera->keptCount); break;
        case F_MOVES_STATE: w.u8(e.moves->state); break;
        case F_DEAD: w.bit(e.death->dead); break;
        case F_LB_SCORES: writeChanged(w, e.arena->leaderScores, varint); break;
        case F_MAP_HEIGHT: w.f32(e.arena->height); break;
        case F_MOB_TYPE: w.u8(e.mob->type); break;
        case F_ARENA_TIME: w.f32(e.arena->time); break;
        case F_LB_ROWS: w.u8(e.arena->leaderRows); break;
        case F_DEATH_LEVEL: w.u8(e.camera->deathLevel); break;
        case F_NAME: w.str(e.name->name); break;
        case F_X: w.f32(e.position->x); break;
        case F_PETAL_TYPE: w.u8(e.petal->type); break;
        case F_PLAYERS: w.varint(e.arena->players); break;
        case F_HEALTH: w.f32(e.health->health); break;
        case F_DROP_FLAGS: w.u8(e.drop->flags); break;
        case F_MAX_HEALTH: w.f32(e.health->maxHealth); break;
        case F_PLAYER: w.ref(e.camera->player); break;
        case F_OWNER: w.ref(e.owned->owner); break;
        case F_XP: w.varint(e.flower->xp); break;
        case F_FLOWER_SCORE: w.f32(e.flower->score); break;
        case F_SLOTS: writeChanged(w, e.flower->slots, u8); break;
        case F_COOLDOWNS: writeChanged(w, e.flower->cooldowns, varint); break;
        case F_LEVEL: w.u8(e.flower->level); break;
        case F_ZONE_TIMER: w.u8(e.flower->zoneTimer); break;
        case F_TANK_LAYOUT: w.u8(e.tank->layout); break;
        case F_TANK_FIRED: w.varint(e.tank->fired); break;
        case F_TANK_TURRETS: writeChanged(w, e.tank->turrets, [&](uint16_t v) { w.u16(v); }); break;
        case F_TANK_SPIN: w.u16(e.tank->spin); break;
        case F_TANK_FLAGS: w.u8(e.tank->flags); break;
        case F_TANK_ALPHA: w.u8(e.tank->alpha); break;
        case F_TANK_TEAM: w.ref(e.tank->team); break;
        case F_SHOT_KIND: w.u8(e.shot->kind); break;
        case F_SHOT_TEAM: w.ref(e.shot->team); break;
        case F_SHOT_LOOK: w.u8(e.shot->look); break;
        case F_SHOT_COLOR: w.u8(e.shot->color); break;
        case F_TANK_COLOR: w.u8(e.tank->color); break;
        case F_TANK_SCORE: w.f32(e.tank->score); break;
        case F_SHOT_MOB: w.u8(e.shot->mob); break;
        case F_DRIVER_TANK: w.u8(e.driver->tank); break;
        case F_DRIVER_LEVEL: w.u8(e.driver->level); break;
        case F_DRIVER_SCORE: w.f32(e.driver->score); break;
        case F_DRIVER_STATS: writeChanged(w, e.driver->stats, u8); break;
        case F_DRIVER_POINTS: w.u8(e.driver->points); break;
        case F_DRIVER_ALIVE: w.f32(e.driver->alive); break;
        case F_DRIVER_DEATH_TANK: w.u8(e.driver->deathTank); break;
        default: break;
    }
}

// what this client gets told of what changed
static FieldMask changesFor(const Entity &e, bool yours) {
    return yours ? e.changed : e.changed & ~kPrivateFields;
}

// the fields in id order: (id step ^ 1) then the field, and 1 to end
void writeDelta(net::Writer &w, const Entity &e, bool yours) {
    FieldMask changes = changesFor(e, yours);
    int last = 0;
    for (int id = 1; id < 128; id++) {
        if (!(changes >> id & 1)) continue;
        w.varint(uint32_t(id - last) ^ 1);
        last = id;
        writeField(w, e, id);
    }
    w.varint(1);
}

// a component mask: four or more components all below 32 as the mask itself, otherwise each one
static bool maskAsBits(uint64_t mask) {
    return __builtin_popcountll(mask) >= 4 && !(mask >> 32);
}
static void comps(net::Writer &w, uint64_t mask) {
    if (maskAsBits(mask)) {
        w.u32(uint32_t(mask));
        return;
    }
    int last = 0;
    for (int c = 0; c < 64; c++) {
        if (!(mask >> c & 1)) continue;
        w.varint(uint32_t(c + 1 - last) ^ 1);
        last = c + 1;
    }
    w.varint(1);
}

// deletions go out in slot order shuffled the way a heap sort shuffles equal keys
static void heapOrder(std::vector<uint32_t> &a) {
    for (size_t x = a.size(); x > 2; x--) {
        std::swap(a[0], a[x - 1]);
        size_t len = x - 1, hole = 0, child = 1;
        uint32_t top = a[0];
        while (true) {
            a[hole] = a[child];
            hole = child;
            if (child > (len - 2) / 2) break;
            child = 2 * child + 1;
        }
        a[hole] = top;
    }
    if (a.size() >= 2) std::swap(a[0], a[1]);
}

void View::write(net::Writer &w, const World &world, const std::vector<uint32_t> &visible,
                 const std::function<uint8_t()> &pad, const std::function<bool(const Entity &)> &yours) {
    std::vector<uint32_t> gone;
    for (uint32_t id : known_)
        if (!contains(visible, id) || !world.get(id)) gone.push_back(id);
    heapOrder(gone);
    w.varint(uint32_t(gone.size()));
    w.u8(pad());
    w.u8(pad());
    for (uint32_t id : gone) w.ref(id);
    w.u8(pad());
    w.u8(pad());
    for (uint32_t id : visible) {
        const Entity *e = world.get(id);
        if (!e) continue;
        bool mine = yours(*e);
        uint8_t own = mine ? 0x40 : 0;
        if (!contains(known_, id)) {
            uint64_t mask = e->comps & kWireComps;
            w.ref(id);
            // create, full state; 0x80: made this tick or the last; 0x01: components as a bit mask
            w.u8((0x08 | own | (e->age <= 1 ? 0x80 : 0) | (maskAsBits(mask) ? 0x01 : 0)) ^ 0x36);
            comps(w, mask);
            writeFull(w, *e, mine);
        } else if (changesFor(*e, mine)) {
            w.ref(id);
            w.u8((0x02 | own) ^ 0x36);  // update, field deltas
            writeDelta(w, *e, mine);
        }
    }
    w.u32(0);
    known_ = visible;
}

bool View::knows(uint32_t id) const {
    auto it = std::lower_bound(known_.begin(), known_.end(), id,
                               [](uint32_t a, uint32_t b) { return (a & 16383) < (b & 16383); });
    return it != known_.end() && *it == id;
}

}  // namespace world
