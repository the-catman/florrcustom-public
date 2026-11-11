// An entity as the server describes it: one struct per component, holding what the server sends
// and then whatever the client keeps to draw it.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Protocol.h"
#include "sim/Lerp.h"

namespace sim {

struct DropData {
    int type = 0;
    int flags = 0;

    float appear = 0;        // 0 hidden .. 1 shown
    bool collected = false;  // flying into the inventory

    bool goingAway() const { return flags & 1; }
};

struct MovesData {
    int state = 0;

    bool chasing() const { return state & 1; }
};

struct PeaData {
    int flags = 0;

    bool poison() const { return flags & 1; }
    bool big() const { return flags & 2; }
};

struct CameraData {
    int respawnLevel = 0;
    float x = 0;
    std::string killer;
    std::array<int, 10> kept{};
    float score = 0;
    float y = 0;
    float fov = 0;
    int keptCount = 0;
    int deathLevel = 0;
    uint32_t player = 0;
};

struct PetalData {
    int type = 0;
};

struct RoyaleData {
    float timeLeft = 0;  // ms
    bool starting = false;
};

struct AntData {
    int kind = 0;
};

struct MobData {
    int type = 0;

    double animT = 0;  // animation clock, ms
    float chaseK = 0;  // 0 wandering .. 1 chasing
    double walk = 0;   // how far its legs have stepped
    float walkSpeed = 0;
    float walkX = 0;
    float walkY = 0;
    bool walkSeen = false;
};

struct OwnedData {
    uint32_t parent = 0;
    uint32_t owner = 0;
};

struct ArenaData {
    std::array<uint32_t, 10> leaderFlowers{};
    std::array<std::string, 10> leaderNames;
    int state = 0;
    int mode = 0;
    float width = 0;
    uint32_t seed = 0;
    std::array<uint32_t, 10> leaderScores{};
    float height = 0;
    float time = 0;  // ms the round has run
    int leaderRows = 0;
    uint32_t players = 0;

    bool running() const { return state & 1; }
    bool won() const { return state & 2; }
    bool closing() const { return mode == 1; }
};

struct NameData {
    std::string name;
};

struct HealthData {
    float health = 0;
    float maxHealth = 0;
};

struct PositionData {
    float angle = 0;
    float y = 0;
    float x = 0;
};

struct SegmentData {
    int flags = 0;
};

struct DeathData {
    bool dead = false;
};

struct SizeData {
    float radius = 0;
};

struct StatusData {
    int flags = 0;

    bool poisoned() const { return flags & 1; }
    bool noHeal() const { return flags & 2; }
    bool any() const { return flags & 7; }
};

struct WebData {
    float grow = 0;  // 0 .. 1 as it spreads out
};

struct FlowerData {
    int eyes = 0;
    int lumpy = 0;
    int face = 0;
    // for its owner only; the lists stay empty for anyone else's flower
    uint32_t xp = 0;
    float score = 0;
    std::vector<int> slots;
    std::vector<uint32_t> cooldowns;
    int level = 0;
    int zoneTimer = 0;

    float healthBar = 1;    // the green bar
    float healthTrail = 1;  // the red part it leaves behind
    float healthAlpha = 0;
    float mouth = 5;  // 5 smile .. -5 frown
    float angry = 0;  // how far the brows are down
    float nameAlpha = 0;
    float lookX = 0;
    float lookY = 0;
    float purple = 0;
    float light = 0;

    bool attacking() const { return face & 1; }
    bool defending() const { return face & 2; }
    bool antennae() const { return face & 4; }
};

// a diep tank, a boss, or a shot with barrels of its own
struct TankData {
    int layout = 0;
    uint32_t fired = 0;  // a bit per barrel that flips when it fires; turrets from bit 16
    std::array<float, 8> turrets{};
    float spin = 0;  // how far the ring of turrets has turned
    int flags = 0;
    int alpha = 255;
    uint32_t team = 0;
    int color = 0;
    float score = 0;

    std::array<Lerp, 8> turretGlide;  // the turrets and the ring as drawn
    Lerp spinGlide;
    uint32_t seenFired = 0;
    bool firedSeen = false;
    std::array<double, 24> kickAt{};  // when each barrel last fired, ms
    float bar = 0;                    // how far its health bar has faded in
    float nameFade = 0;

    bool spawning() const { return flags & 1; }
};

// something a tank fired
struct ShotData {
    int kind = 0;
    uint32_t team = 0;
    int look = 1;  // sides
    int color = 0;
    int mob = 0;  // a claimed mob's type plus one
};

// the player's own tank: on the camera
struct DriverData {
    int tank = 0;
    int level = 0;
    float score = 0;
    std::array<int, 8> stats{};
    int points = 0;
    float alive = 0;
    int deathTank = 0;
};

struct Entity {
    uint32_t id = 0;     // slot | generation << 14
    uint64_t comps = 0;  // one bit per component

    std::optional<DropData> drop;
    std::optional<MovesData> moves;
    std::optional<PeaData> pea;
    std::optional<CameraData> camera;
    std::optional<PetalData> petal;
    std::optional<RoyaleData> royale;
    std::optional<AntData> ant;
    std::optional<MobData> mob;
    std::optional<OwnedData> owned;
    std::optional<ArenaData> arena;
    std::optional<NameData> name;
    std::optional<HealthData> health;
    std::optional<PositionData> position;
    std::optional<SegmentData> segment;
    std::optional<DeathData> death;
    std::optional<SizeData> size;
    std::optional<StatusData> status;
    std::optional<WebData> web;
    std::optional<FlowerData> flower;
    std::optional<TankData> tank;
    std::optional<ShotData> shot;
    std::optional<DriverData> driver;

    // gliding toward what the server says
    float x = 0;
    float y = 0;
    float angle = 0;
    float r = 0;
    Lerp lerpX, lerpY, lerpAngle{0, 0, 0, true}, lerpR;

    float hurt = 0;  // 1 when the server says it was hit, fading over 150 ms
    double hurtAt = -1e9;
    double born = 0;
    double deadAt = -1;   // when the server marked it dead; it fades out over the next 150 ms
    bool placed = false;  // has had its first position
    bool drawn = false;   // has been on screen; until then its bars and fades start settled
    bool fresh = false;   // made by the server just now, not just come into view

    bool has(int c) const { return comps >> c & 1; }
    float hp() const { return health ? health->health : 0; }
    float maxHp() const { return health ? health->maxHealth : 0; }
    int level() const { return flower ? flower->level : 0; }
};

}  // namespace sim
