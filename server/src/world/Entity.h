// An entity is a set of components, and most components are a struct of the fields clients are told
// about. Setting a field marks it changed only when what clients would receive differs, so an
// update carries real changes and nothing else.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>

#include "Math.h"
#include "Protocol.h"

namespace world {

using namespace protocol;

// a component's bit in Entity::comps
constexpr uint64_t bit(int c) { return 1ull << c; }

// one bit per field, by wire id
using FieldMask = unsigned __int128;

// A field's bit in the changed mask is its id on the wire.
constexpr int wireId(int field) {
    return field == kFirstField ? 1 : field;
}
// the flower's fields only its owner is told
constexpr FieldMask kPrivateFields = FieldMask(0x3f) << F_XP;

inline uint16_t angleU16(double a) {
    if (a < 0) a += TAU;
    return uint16_t(toU32(a * (65535.0 / TAU)));
}

// whether two values reach clients as the same bytes; an int is sent as one byte
inline bool sameOnWire(int a, int b) {
    return uint8_t(a) == uint8_t(b);
}
inline bool sameOnWire(uint32_t a, uint32_t b) {
    return a == b;
}
inline bool sameOnWire(uint16_t a, uint16_t b) {
    return a == b;
}
inline bool sameOnWire(bool a, bool b) {
    return a == b;
}
inline bool sameOnWire(const std::string &a, const std::string &b) {
    return a == b;
}
inline bool sameOnWire(float a, float b) {
    return !std::memcmp(&a, &b, sizeof a);
}

// a list field: clients are told only the elements that changed
template <class T, size_t N>
struct WireList {
    std::array<T, N> values{};
    uint32_t changed = 0;  // one bit per element

    const T &operator[](int i) const { return values[i]; }
};

class FieldGroup {
public:
    explicit FieldGroup(FieldMask *changed) : changed_(changed) {}

protected:
    template <class T>
    void change(int field, T &value, const T &to) {
        if (!sameOnWire(value, to)) mark(field);
        value = to;
    }
    template <class T, size_t N>
    void changeAt(int field, WireList<T, N> &list, int i, const T &to) {
        if (!sameOnWire(list.values[i], to)) {
            mark(field);
            list.changed |= 1u << i;
        }
        list.values[i] = to;
    }

private:
    void mark(int field) { *changed_ |= FieldMask(1) << wireId(field); }

    FieldMask *changed_;
};

struct DropData : FieldGroup {
    using FieldGroup::FieldGroup;
    int type = 0;
    int flags = 0;  // bit 0: going away

    void setType(int v) { change(F_DROP_TYPE, type, v); }
    void setFlags(int v) { change(F_DROP_FLAGS, flags, v); }
};

struct MovesData : FieldGroup {
    using FieldGroup::FieldGroup;
    int state = 0;  // bit 0: going after a target

    void setState(int v) { change(F_MOVES_STATE, state, v); }
};

struct PeaData : FieldGroup {
    using FieldGroup::FieldGroup;
    int flags = 0;  // bit 0: poison, bit 1: big

    void setFlags(int v) { change(F_PEA_FLAGS, flags, v); }
};

struct CameraData : FieldGroup {
    using FieldGroup::FieldGroup;
    int respawnLevel = 0;
    float x = 0;
    std::string killer;
    WireList<int, 10> kept;
    float score = 0;
    float y = 0;
    float fov = 0;
    int keptCount = 0;
    int deathLevel = 0;
    uint32_t player = 0;

    void setRespawnLevel(int v) { change(F_RESPAWN_LEVEL, respawnLevel, v); }
    void setX(float v) { change(F_CAMERA_X, x, v); }
    void setKiller(const std::string &v) { change(F_KILLER, killer, v); }
    void setKept(int i, int v) { changeAt(F_KEPT, kept, i, v); }
    void setScore(float v) { change(F_SCORE, score, v); }
    void setY(float v) { change(F_CAMERA_Y, y, v); }
    void setFov(float v) { change(F_FOV, fov, v); }
    void setKeptCount(int v) { change(F_KEPT_COUNT, keptCount, v); }
    void setDeathLevel(int v) { change(F_DEATH_LEVEL, deathLevel, v); }
    void setPlayer(uint32_t v) { change(F_PLAYER, player, v); }
};

struct PetalData : FieldGroup {
    using FieldGroup::FieldGroup;
    int type = 0;

    void setType(int v) { change(F_PETAL_TYPE, type, v); }
};

struct AntData : FieldGroup {
    using FieldGroup::FieldGroup;
    int kind = 0;

    void setKind(int v) { change(F_ANT_KIND, kind, v); }
};

struct MobData : FieldGroup {
    using FieldGroup::FieldGroup;
    int type = 0;

    void setType(int v) { change(F_MOB_TYPE, type, v); }
};

struct OwnedData : FieldGroup {
    using FieldGroup::FieldGroup;
    uint32_t owner = 0;

    void setOwner(uint32_t v) { change(F_OWNER, owner, v); }
};

struct ArenaData : FieldGroup {
    using FieldGroup::FieldGroup;
    WireList<uint32_t, 10> leaderFlowers;
    WireList<std::string, 10> leaderNames;
    int state = 0;  // bit 0: running
    int mode = 0;   // 1: battle royale
    float width = 0;
    uint32_t seed = 0;
    WireList<uint32_t, 10> leaderScores;
    float height = 0;
    float time = 0;
    int leaderRows = 0;
    uint32_t players = 0;

    void setLeaderFlower(int i, uint32_t v) { changeAt(F_LB_FLOWERS, leaderFlowers, i, v); }
    void setLeaderName(int i, const std::string &v) { changeAt(F_LB_NAMES, leaderNames, i, v); }
    void setState(int v) { change(F_ARENA_STATE, state, v); }
    void setWidth(float v) { change(F_MAP_WIDTH, width, v); }
    void setSeed(uint32_t v) { change(F_ARENA_SEED, seed, v); }
    void setLeaderScore(int i, uint32_t v) { changeAt(F_LB_SCORES, leaderScores, i, v); }
    void setHeight(float v) { change(F_MAP_HEIGHT, height, v); }
    void setTime(float v) { change(F_ARENA_TIME, time, v); }
    void setLeaderRows(int v) { change(F_LB_ROWS, leaderRows, v); }
    void setPlayers(uint32_t v) { change(F_PLAYERS, players, v); }
};

struct NameData : FieldGroup {
    using FieldGroup::FieldGroup;
    std::string name;

    void setName(const std::string &v) { change(F_NAME, name, v); }
};

struct HealthData : FieldGroup {
    using FieldGroup::FieldGroup;
    float health = 0;
    float maxHealth = 0;

    void setHealth(float v) { change(F_HEALTH, health, v); }
    void setMaxHealth(float v) { change(F_MAX_HEALTH, maxHealth, v); }
};

struct PositionData : FieldGroup {
    using FieldGroup::FieldGroup;
    uint16_t angle = 0;
    float y = 0;
    float x = 0;

    void setAngle(double radians) { change(F_ANGLE, angle, angleU16(radians)); }
    void setY(float v) { change(F_Y, y, v); }
    void setX(float v) { change(F_X, x, v); }
};

struct SegmentData : FieldGroup {
    using FieldGroup::FieldGroup;
    int flags = 0;  // bit 0: evil, bit 1: head

    void setFlags(int v) { change(F_SEGMENT_FLAGS, flags, v); }
};

struct DeathData : FieldGroup {
    using FieldGroup::FieldGroup;
    bool dead = false;

    void setDead(bool v) { change(F_DEAD, dead, v); }
};

struct SizeData : FieldGroup {
    using FieldGroup::FieldGroup;
    float radius = 0;

    void setRadius(float v) { change(F_RADIUS, radius, v); }
};

struct StatusData : FieldGroup {
    using FieldGroup::FieldGroup;
    int flags = 0;  // bit 0: poisoned, bit 1: can't heal, bit 2: anything at all

    void setFlags(int v) { change(F_STATUS_FLAGS, flags, v); }
};

struct FlowerData : FieldGroup {
    using FieldGroup::FieldGroup;
    int eyes = 0;
    int face = 0;  // bit 0: attacking, bit 1: defending, bit 2: antennae
    // for its owner only
    uint32_t xp = 0;
    float score = 0;
    WireList<int, 16> slots;
    WireList<uint32_t, 8> cooldowns;
    int level = 0;
    int zoneTimer = 0;

    void setEyes(int v) { change(F_EYES, eyes, v); }
    void setFace(int v) { change(F_FACE, face, v); }
    void setXp(uint32_t v) { change(F_XP, xp, v); }
    void setScore(float v) { change(F_FLOWER_SCORE, score, v); }
    void setSlot(int i, int v) { changeAt(F_SLOTS, slots, i, v); }
    void setCooldown(int i, uint32_t v) { changeAt(F_COOLDOWNS, cooldowns, i, v); }
    void setLevel(int v) { change(F_LEVEL, level, v); }
    void setZoneTimer(int v) { change(F_ZONE_TIMER, zoneTimer, v); }
};

struct TankData : FieldGroup {
    using FieldGroup::FieldGroup;
    int layout = 0;
    uint32_t fired = 0;
    WireList<uint16_t, 8> turrets;
    uint16_t spin = 0;
    int flags = 0;  // bit 0: just spawned
    int alpha = 255;
    uint32_t team = 0;
    int color = 0;
    float score = 0;

    void setLayout(int v) { change(F_TANK_LAYOUT, layout, v); }
    void setFired(uint32_t v) { change(F_TANK_FIRED, fired, v); }
    void setTurret(int i, double radians) { changeAt(F_TANK_TURRETS, turrets, i, angleU16(radians)); }
    void setSpin(double radians) { change(F_TANK_SPIN, spin, angleU16(radians)); }
    void setFlags(int v) { change(F_TANK_FLAGS, flags, v); }
    void setAlpha(int v) { change(F_TANK_ALPHA, alpha, v); }
    void setTeam(uint32_t v) { change(F_TANK_TEAM, team, v); }
    void setColor(int v) { change(F_TANK_COLOR, color, v); }
    void setScore(float v) { change(F_TANK_SCORE, score, v); }
};

struct ShotData : FieldGroup {
    using FieldGroup::FieldGroup;
    int kind = 0;
    uint32_t team = 0;
    int look = 0;
    int color = 0;
    int mob = 0;

    void setKind(int v) { change(F_SHOT_KIND, kind, v); }
    void setTeam(uint32_t v) { change(F_SHOT_TEAM, team, v); }
    void setLook(int v) { change(F_SHOT_LOOK, look, v); }
    void setColor(int v) { change(F_SHOT_COLOR, color, v); }
    void setMob(int v) { change(F_SHOT_MOB, mob, v); }
};

struct DriverData : FieldGroup {
    using FieldGroup::FieldGroup;
    int tank = 0;
    int level = 0;
    float score = 0;
    WireList<int, 8> stats;
    int points = 0;
    float alive = 0;
    int deathTank = 0;

    void setTank(int v) { change(F_DRIVER_TANK, tank, v); }
    void setLevel(int v) { change(F_DRIVER_LEVEL, level, v); }
    void setScore(float v) { change(F_DRIVER_SCORE, score, v); }
    void setStat(int i, int v) { changeAt(F_DRIVER_STATS, stats, i, v); }
    void setPoints(int v) { change(F_DRIVER_POINTS, points, v); }
    void setAlive(float v) { change(F_DRIVER_ALIVE, alive, v); }
    void setDeathTank(int v) { change(F_DRIVER_DEATH_TANK, deathTank, v); }
};

struct Entity {
    uint32_t id = 0;        // slot | generation << 14
    uint64_t comps = 0;     // one bit per component
    FieldMask changed = 0;  // fields changed since the last update went out, by wire id
    uint8_t age = 0;       // ticks since it was made, counting up to 2

    std::optional<DropData> drop;
    std::optional<MovesData> moves;
    std::optional<PeaData> pea;
    std::unique_ptr<CameraData> camera;
    std::optional<PetalData> petal;
    std::optional<AntData> ant;
    std::optional<MobData> mob;
    std::optional<OwnedData> owned;
    std::unique_ptr<ArenaData> arena;
    std::optional<NameData> name;
    std::optional<HealthData> health;
    std::optional<PositionData> position;
    std::optional<SegmentData> segment;
    std::optional<DeathData> death;
    std::optional<SizeData> size;
    std::optional<StatusData> status;
    std::unique_ptr<FlowerData> flower;
    std::optional<TankData> tank;
    std::optional<ShotData> shot;
    std::unique_ptr<DriverData> driver;

    Entity() = default;
    Entity(const Entity &) = delete;
    Entity &operator=(const Entity &) = delete;

    bool has(int c) const { return comps >> c & 1; }
    // a player's body: a flower, or a tank with a camera behind it
    bool isPlayer() const { return has(C_FLOWER) || (has(C_TANK) && !has(C_SHOT) && tank->team); }
    uint16_t slot() const { return uint16_t(id & 16383); }

    // what it reads as without the component: nothing
    float hp() const { return health ? health->health : 0; }
    float maxHp() const { return health ? health->maxHealth : 0; }
    int level() const { return flower ? flower->level : 0; }
    int petalType() const { return petal ? petal->type : 0; }

    // adds a component, its fields starting at zero
    void add(int c);
    // the changes went out with this tick's updates
    void clean();
};

}  // namespace world
