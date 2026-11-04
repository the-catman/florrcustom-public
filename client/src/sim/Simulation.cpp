#include "sim/Simulation.h"

using namespace protocol;

namespace sim {

namespace {

// what an update's operation byte says, once unmasked
constexpr uint32_t kOpMask = 54;
constexpr uint32_t kOpComponentBits = 1;
constexpr uint32_t kOpUpdate = 2;
constexpr uint32_t kOpFullState = 8;
constexpr uint32_t kOpYours = 64;
constexpr uint32_t kOpFresh = 128;

float readAngle(net::Reader &r) {
    return float(r.u16() * 0.000096);
}

// the changed elements of a list: (index step ^ 1) then the element, until a step of 0
template <class T, class Read>
void readChanged(net::Reader &r, T &list, size_t size, Read read) {
    size_t i = 0;
    while (!r.bad()) {
        uint32_t step = r.varint() ^ 1;
        if (!step) return;
        i += step;
        auto value = read();
        if (i - 1 < size) list[i - 1] = value;
    }
}

// the same, for a list nothing keeps
template <class Read>
void skipChanged(net::Reader &r, Read read) {
    while (!r.bad()) {
        if (!(r.varint() ^ 1)) return;
        read();
    }
}

// a field can arrive for a component the entity wasn't made with; it is kept all the same
template <class T>
T &need(std::optional<T> &group) {
    if (!group) group.emplace();
    return *group;
}

void addComponents(Entity &e) {
    if (e.has(C_DROP)) e.drop.emplace();
    if (e.has(C_MOVES)) e.moves.emplace();
    if (e.has(C_PEA)) e.pea.emplace();
    if (e.has(C_CAMERA)) e.camera.emplace();
    if (e.has(C_PETAL)) e.petal.emplace();
    if (e.has(C_ROYALE)) e.royale.emplace();
    if (e.has(C_ANT)) e.ant.emplace();
    if (e.has(C_MOB)) e.mob.emplace();
    if (e.has(C_OWNED)) e.owned.emplace();
    if (e.has(C_ARENA)) e.arena.emplace();
    if (e.has(C_NAME)) e.name.emplace();
    if (e.has(C_HEALTH)) e.health.emplace();
    if (e.has(C_POSITION)) e.position.emplace();
    if (e.has(C_SEGMENT)) e.segment.emplace();
    if (e.has(C_DEATH)) e.death.emplace();
    if (e.has(C_SIZE)) e.size.emplace();
    if (e.has(C_STATUS)) e.status.emplace();
    if (e.has(C_WEB)) e.web.emplace();
    if (e.has(C_FLOWER)) e.flower.emplace();
}

uint32_t readComponents(net::Reader &r, uint32_t op) {
    if (op & kOpComponentBits) return r.u32();
    uint32_t comps = 0;
    int i = 0;
    while (!r.bad()) {
        uint32_t step = r.varint() ^ 1;
        if (!step) break;
        i += step;
        comps |= 1u << (i - 1);
    }
    return comps;
}

// nothing here uses these; they are read to stay in step with what follows
void skipEventFull(net::Reader &r) {
    r.str();
    r.varint();
    r.f64();
    r.str();
    r.f32();
    r.bit();
    r.f64();
    r.varint();
    r.varint();
    r.varint();
}

}  // namespace

Entity *Simulation::get(uint32_t id) {
    return const_cast<Entity *>(static_cast<const Simulation *>(this)->get(id));
}

const Entity *Simulation::get(uint32_t id) const {
    if (!id) return nullptr;
    auto it = entities.find(uint16_t(id & 16383));
    return it == entities.end() || it->second.id != id ? nullptr : &it->second;
}

CameraData *Simulation::camera() {
    Entity *e = get(cameraId);
    return e ? &*e->camera : nullptr;
}

const ArenaData *Simulation::arena() const {
    const Entity *e = get(arenaId);
    return e ? &*e->arena : nullptr;
}

Entity *Simulation::flower() {
    CameraData *c = camera();
    return c ? get(c->player) : nullptr;
}

void Simulation::clear() {
    entities.clear();
    cameraId = arenaId = 0;
}

bool Simulation::decodeUpdate(net::Reader &r, double now) {
    uint32_t length = r.varint();
    size_t start = r.pos();

    uint32_t deletions = r.varint();
    r.skip(2);
    for (uint32_t i = 0; i < deletions && !r.bad(); i++) {
        auto it = entities.find(uint16_t(r.ref() & 16383));
        if (it != entities.end()) entities.erase(it);
    }
    r.skip(2);

    while (!r.bad()) {
        uint32_t id = r.ref();
        if (!id) break;
        uint32_t op = r.u8() ^ kOpMask;
        uint16_t slot = uint16_t(id & 16383);
        Entity *e;
        if (op & kOpUpdate) {
            auto it = entities.find(slot);
            if (it == entities.end()) {
                r.fail();
                break;
            }
            e = &it->second;
        } else {
            uint32_t comps = readComponents(r, op);
            e = &entities[slot];
            *e = Entity();
            e->id = id;
            e->comps = comps;
            e->born = now;
            addComponents(*e);
            if (e->has(C_CAMERA)) cameraId = id;
            if (e->has(C_ARENA)) arenaId = id;
        }
        if (op & kOpFresh) e->fresh = true;
        if (op & kOpFullState)
            decodeFull(r, *e, op & kOpYours);
        else
            decodeDelta(r, *e);
    }
    return !r.bad() && r.pos() - start == length;
}

void Simulation::decodeFull(net::Reader &r, Entity &e, bool yours) {
    if (e.drop) {
        e.drop->type = r.u8();
        e.drop->flags = r.u8();
    }
    if (e.moves) e.moves->state = r.u8();
    if (e.pea) e.pea->flags = r.u8();
    if (e.camera) {
        CameraData &c = *e.camera;
        c.respawnLevel = r.u8();
        c.x = r.f32();
        c.killer = r.str();
        for (int &petal : c.kept) petal = r.u8();
        r.u8();
        c.score = r.f32();
        c.y = r.f32();
        c.fov = r.f32();
        c.keptCount = r.u8();
        c.deathLevel = r.u8();
        c.player = r.ref();
    }
    if (e.petal) e.petal->type = r.u8();
    if (e.royale) {
        e.royale->timeLeft = r.f32();
        e.royale->starting = r.bit();
    }
    if (e.ant) e.ant->kind = r.u8();
    if (e.mob) e.mob->type = r.u8();
    if (e.owned) {
        e.owned->parent = r.ref();
        e.owned->owner = r.ref();
    }
    if (e.arena) {
        ArenaData &a = *e.arena;
        for (uint32_t &flower : a.leaderFlowers) flower = r.ref();
        for (std::string &name : a.leaderNames) name = r.str();
        a.state = r.u8();
        a.mode = r.u8();
        a.width = r.f32();
        a.seed = r.varint();
        for (uint32_t &score : a.leaderScores) score = r.varint();
        a.height = r.f32();
        a.time = r.f32();
        a.leaderRows = r.u8();
        a.players = r.varint();
    }
    if (e.name) e.name->name = r.str();
    if (e.health) {
        e.health->health = r.f32();
        e.health->maxHealth = r.f32();
    }
    if (e.position) {
        e.position->angle = readAngle(r);
        e.position->y = r.f32();
        e.position->x = r.f32();
    }
    if (e.segment) {
        e.segment->flags = r.u8();
        r.bit();
    }
    if (e.death) e.death->dead = r.bit();
    if (e.size) e.size->radius = r.f32();
    if (e.has(C_EVENT)) skipEventFull(r);
    if (e.status) e.status->flags = r.u8();
    if (e.flower) {
        FlowerData &f = *e.flower;
        f.eyes = r.u8();
        f.lumpy = r.u8();
        f.face = r.u8();
        if (!yours) return;
        f.xp = r.varint();
        f.score = r.f32();
        f.slots.resize(16);
        for (int &type : f.slots) type = r.u8();
        f.cooldowns.resize(8);
        for (uint32_t &cooldown : f.cooldowns) cooldown = r.varint();
        f.level = r.u8();
        f.zoneTimer = r.u8();
    }
}

void Simulation::decodeDelta(net::Reader &r, Entity &e) {
    int field = 0;
    while (!r.bad()) {
        uint32_t step = r.varint() ^ 1;
        if (!step) return;
        field += step;
        switch (field) {
            case F_EYES: need(e.flower).eyes = r.u8(); break;
            case F_LB_FLOWERS: {
                ArenaData &a = need(e.arena);
                readChanged(r, a.leaderFlowers, a.leaderFlowers.size(), [&] { return r.ref(); });
                break;
            }
            case F_EVENT_4: skipChanged(r, [&] { r.str(); }); break;
            case F_CAMERA_X: need(e.camera).x = r.f32(); break;
            case F_KILLER: need(e.camera).killer = r.str(); break;
            case F_RADIUS: need(e.size).radius = r.f32(); break;
            case F_ANGLE: need(e.position).angle = readAngle(r); break;
            case F_PEA_FLAGS: need(e.pea).flags = r.u8(); break;
            case F_LB_NAMES: {
                ArenaData &a = need(e.arena);
                readChanged(r, a.leaderNames, a.leaderNames.size(), [&] { return r.str(); });
                break;
            }
            case F_ARENA_STATE: need(e.arena).state = r.u8(); break;
            case F_ARENA_MODE: need(e.arena).mode = r.u8(); break;
            case F_ROYALE_TIME: need(e.royale).timeLeft = r.f32(); break;
            case F_STATUS_FLAGS: need(e.status).flags = r.u8(); break;
            case F_EVENT_15: r.varint(); break;
            case F_KEPT: {
                CameraData &c = need(e.camera);
                readChanged(r, c.kept, c.kept.size(), [&] { return int(r.u8()); });
                break;
            }
            case F_ANT_KIND: need(e.ant).kind = r.u8(); break;
            case F_EVENT_18: r.f64(); break;
            case F_MAP_WIDTH: need(e.arena).width = r.f32(); break;
            case F_CAMERA_UNUSED: r.u8(); break;
            case F_SCORE: need(e.camera).score = r.f32(); break;
            case F_DROP_TYPE: need(e.drop).type = r.u8(); break;
            case F_PARENT: need(e.owned).parent = r.ref(); break;
            case F_CAMERA_Y: need(e.camera).y = r.f32(); break;
            case F_EVENT_25: skipChanged(r, [&] { r.str(); }); break;
            case F_LUMPY: need(e.flower).lumpy = r.u8(); break;
            case F_EVENT_27: r.f32(); break;
            case F_Y: need(e.position).y = r.f32(); break;
            case F_SEGMENT_FLAGS: need(e.segment).flags = r.u8(); break;
            case F_ARENA_SEED: need(e.arena).seed = r.varint(); break;
            case F_FACE: need(e.flower).face = r.u8(); break;
            case F_FOV: need(e.camera).fov = r.f32(); break;
            case F_EVENT_33: skipChanged(r, [&] { r.bit(); }); break;
            case F_KEPT_COUNT: need(e.camera).keptCount = r.u8(); break;
            case F_EVENT_35: skipChanged(r, [&] { r.f64(); }); break;
            case F_MOVES_STATE: need(e.moves).state = r.u8(); break;
            case F_ROYALE_STARTING: need(e.royale).starting = r.bit(); break;
            case F_DEAD: need(e.death).dead = r.bit(); break;
            case F_LB_SCORES: {
                ArenaData &a = need(e.arena);
                readChanged(r, a.leaderScores, a.leaderScores.size(), [&] { return r.varint(); });
                break;
            }
            case F_SEGMENT_UNUSED: r.bit(); break;
            case F_EVENT_41: r.varint(); break;
            case F_MAP_HEIGHT: need(e.arena).height = r.f32(); break;
            case F_MOB_TYPE: need(e.mob).type = r.u8(); break;
            case F_EVENT_44: r.varint(); break;
            case F_ARENA_TIME: need(e.arena).time = r.f32(); break;
            case F_LB_ROWS: need(e.arena).leaderRows = r.u8(); break;
            case F_DEATH_LEVEL: need(e.camera).deathLevel = r.u8(); break;
            case F_NAME: need(e.name).name = r.str(); break;
            case F_X: need(e.position).x = r.f32(); break;
            case F_PETAL_TYPE: need(e.petal).type = r.u8(); break;
            case F_EVENT_51: r.varint(); break;
            case F_PLAYERS: need(e.arena).players = r.varint(); break;
            case F_HEALTH: need(e.health).health = r.f32(); break;
            case F_DROP_FLAGS: need(e.drop).flags = r.u8(); break;
            case F_MAX_HEALTH: need(e.health).maxHealth = r.f32(); break;
            case F_PLAYER: need(e.camera).player = r.ref(); break;
            case F_OWNER: need(e.owned).owner = r.ref(); break;
            case F_XP: need(e.flower).xp = r.varint(); break;
            case F_FLOWER_SCORE: need(e.flower).score = r.f32(); break;
            case F_SLOTS: {
                FlowerData &f = need(e.flower);
                f.slots.resize(16);
                readChanged(r, f.slots, f.slots.size(), [&] { return int(r.u8()); });
                break;
            }
            case F_COOLDOWNS: {
                FlowerData &f = need(e.flower);
                f.cooldowns.resize(8);
                readChanged(r, f.cooldowns, f.cooldowns.size(), [&] { return r.varint(); });
                break;
            }
            case F_LEVEL: need(e.flower).level = r.u8(); break;
            case F_ZONE_TIMER: need(e.flower).zoneTimer = r.u8(); break;
            default: need(e.camera).respawnLevel = r.u8(); break;  // id 1
        }
    }
}

}  // namespace sim
