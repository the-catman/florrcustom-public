// What the server keeps about an entity beside the fields clients are told, and about each client.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "TankData.h"
#include "world/View.h"

namespace game {

// Positions are kept as floats, like on the wire; the maths in between runs in doubles.
struct Body {
    float x = 0, y = 0;
    double vx = 0, vy = 0;       // per ms
    double ax = 0, ay = 0;       // steady drift per ms
    double angle = 0, spin = 0;  // radians, radians per second
    bool unbounded = false;      // may leave the map
    bool diep = false;           // its velocity fades like diep's, 0.9 a tick
};

// the circle other things collide with
struct Hitbox {
    float radius = 0;
    float density = 1;
    float damage = 0;
    bool pushable = true;  // static things (rocks, cacti, ant holes) are not
    bool ghost = false;    // nothing touches it
    bool still = false;    // never bounced or pushed
    bool hitsTeam = true;  // bumps into things on its own side
};

// How a mob moves and picks fights. Speeds are per second, distances in world units, times in ms.
struct AiParams {
    float turn = 0;         // how fast the wander heading turns, radians/s
    bool faceMove = false;  // faces where it walks; otherwise it spins
    float spin = 0;         // radians/s, when not facing where it walks
    float speed = 0;        // walking
    bool pulse = false;     // walks in bursts
    float pulseRate = 0, pulsePow = 0;
    bool wobble = false;  // sways side to side as it walks
    float wobbleRate = 0, wobbleSize = 0;
    float headingPeriod = 0;  // picks a new heading this often, when > 0
    float minZone = 0;        // won't go after things left of this fraction of the map
    bool hunts = false;       // looks for targets
    bool fightsBack = false;  // goes after whatever hurts it
    bool leads = false;       // aims where its target is going
    float leadSpeed = 0;      // how fast it reckons it closes in, for leading
    float chaseSpeed = 0;     // chasing, or walking home
    float sight = 0;          // how far it looks for targets
    float giveUp = 0;         // gives up beyond sight times this
    float orbitDist = 0;      // circles a target closer than this
    float orbitSpeed = 0;
    float leash = 0;      // how far it strays from its leader
    float guard = 0;      // ignores targets further than this from its leader
    bool shoots = false;  // fires a missile at its target, every attackPeriod
    float attackPeriod = 0;
};
struct Ai {
    AiParams p;
    double since = 0;  // when the params were set
    bool timerOn = false;
    double timerAt = 0;  // ms; next new heading
    bool wanderLeft = false;
    float wanderAngle = 0;  // heading while wandering
    float cooldown = 0;     // ms until the next attack
    uint32_t target = 0;
    uint32_t leader = 0;         // ants from a hole stay near it
    float homeX = 0, homeY = 0;  // where it's walking back to
    bool homing = false;
};

struct Mob {
    uint8_t type = 0;
    int8_t zone = -1;  // spawn zone it counts toward, -1 for none
};

struct Loot {
    float xp = 0;
    std::vector<uint8_t> petals;
};

// a petal lying on the ground
struct DropState {
    uint8_t type = 0;
    bool timed = false;
    double expireAt = 0;  // ms
    double spawnedAt = 0;
    uint32_t collector = 0;
};

// health bookkeeping beside the wire fields
struct Life {
    float fuse = 0;        // ms until it dies by itself, when > 0
    uint32_t hitTick = 0;  // tick of the last hit flash
    bool dead = false;
    bool timed = false;  // removal waits for removeAt
    double removeAt = 0;
};

// something done to a flower or a mob for a while
enum EffectKind : uint8_t { E_POISON, E_NO_HEAL, E_HONEY, E_WEB };
struct Effect {
    EffectKind kind;
    double until;         // ms
    uint32_t source = 0;  // poison: who gets the credit for what it does
};
// what's on a flower or a mob: effects that wear off, then those from webs it's in
struct Status {
    std::vector<Effect> timed, webs;
    bool flashed = false;  // when poison last showed a flash
    double flashAt = 0;
};
// a web: everything that can be slowed inside it is, but spiders
struct Web {
    float radius = 50;
    std::vector<uint32_t> caught;  // by slot
};

// How a diep thing meets others: diep's numbers, with health and damage in florr's units. Mobs
// get one made up from their hitbox when they meet one.
struct Contact {
    double push = 8;    // how hard it shoves what it touches, diep units a tick
    double absorb = 1;  // how much of a shove it takes
    double damage = 8;  // per diep tick of touching
    double minMul = 1, maxMul = 4;
    double reduction = 1;        // 0: takes no damage
    bool noOwnTeam = false;      // passes through its own side
    bool onlySameOwner = false;  // on its own side, meets only what has the same owner
    bool quiet = false;          // doesn't flash when hit
};

// one barrel and its firing rhythm, in diep ticks
struct Barrel {
    const tankdata::BarrelDef *def = nullptr;
    double pos = 0;
    double reload = 0;    // ticks between shots
    int drones = 0;       // its drones still out
    double accel = 0;     // what its bullets cruise at, diep units a tick
    uint32_t serial = 0;  // drones die with the barrel that fired them
};

// What picks targets for a drone, a turret or a boss, like diep's AI: the nearest tank in view,
// or else the nearest anything that isn't on its side.
struct Aim {
    double view = 1700;  // diep units
    bool predict = false;
    double speed = 0;  // how fast it reckons its shots close in, diep units a tick
    uint32_t target = 0;
    bool idle = true;
    double turn = 0.01;               // its idle turn, radians a tick
    double mouseX = 100, mouseY = 0;  // where it aims: a point in the world, or with no target, about (0, 0)
    int hash = 0;                     // spreads the searches over ticks
    int interval = 2;                 // looks for a new target every this many ticks, or every tick at 0
};

// an auto turret on a tank or a boss
struct Turret {
    const tankdata::BarrelDef *def = nullptr;
    double base = 25;        // diep size at scale 1
    double x = 0, y = 0;     // place on its tank, diep units at scale 1, before the tank or ring turns
    double slot = 0;         // a ring turret's place on the ring
    bool ring = false;       // turns with the ring and aims within a quarter turn of its place
    bool ownerAims = false;  // aims at its tank's mouse while the tank fires
    double angle = 0;        // where it points, in the world
    Aim aim;
    Barrel barrel;
};

struct TankState {
    const tankdata::TankDef *def = nullptr;
    int layout = 0;
    double base = 50;        // diep size at scale 1
    double scale = 1;        // grows 1% a level
    double reload = 15;      // ticks between shots, before each barrel's own factor
    double speed = 0;        // diep push a tick from moving
    double regen = 0;        // health a diep tick
    double lastHit = -1e18;  // ms
    double born = 0;
    bool shield = true;  // just spawned: blinks and takes no damage until it moves or fires
    double opacity = 1;
    std::vector<Barrel> barrels;
    uint32_t fired = 0;  // one bit per barrel, flipped on each shot; turrets from bit 16
    std::vector<Turret> turrets;
    double ring = 0;  // how far the ring of turrets has turned
    double scoreReward = 0;
    bool zoomed = false;  // a zooming tank's camera, while it looks ahead
    double zoomX = 0, zoomY = 0;
    // what it wants this tick: the mouse in the world, its input flags, which way it moves
    double mouseX = 0, mouseY = 0;
    uint32_t flags = 0;
    double moveX = 0, moveY = 0;
    // bosses
    bool boss = false;
    Aim aim;
    double homeX = 0, homeY = 0;
    int corner = -1;  // which corner of its patch it's heading for
    double bossSpeed = 0.5;
};

struct ShotState {
    int kind = 0;       // tankdata::BulletKind
    uint32_t tank = 0;  // what fired it: a tank, a boss or a shot with barrels of its own
    int barrel = -1;    // a drone: the serial of the barrel that fired it
    const tankdata::BarrelDef *def = nullptr;
    double born = 0;              // ms
    double age = 0;               // diep ticks
    double life = 0;              // diep ticks it lasts
    double accel = 0, speed = 0;  // its push a tick, and its first push
    double angle = 0;             // where it pushes, unless it pushes where it faces
    bool facing = false;          // pushes where it faces
    bool launched = false;
    double scale = 1;
    // drones and minions
    bool drone = false;
    bool controlled = false;  // follows its tank's mouse while the tank fires
    bool returning = false;
    int restDivisor = 6;
    bool facesVelocity = true;  // faces where it's going while it rests
    double reach = 1500;        // how far from its tank its targets may be
    bool reachAll = false;      // swarms go after anything in view
    Aim aim;
    // swarms wander when they have nothing to chase
    bool wandering = false;
    double heading = 0, headingTo = 0, hold = 0;
    // traps only meet their own side's things for their first ticks
    double ownUntil = -1;  // diep ticks
    double spin = 0;       // radians a tick
    // shots with barrels of their own
    std::vector<Barrel> barrels;
    uint32_t fired = 0;
    double reload = 15;
    bool firing = false;
};

// server-side state of one entity, beside its wire fields
struct Obj {
    uint32_t owner = 0;
    std::vector<uint32_t> owned;
    std::unique_ptr<Body> body;
    std::unique_ptr<Hitbox> hit;
    std::unique_ptr<Ai> ai;
    std::unique_ptr<Mob> mob;
    std::unique_ptr<Loot> loot;
    std::unique_ptr<Life> life;
    std::unique_ptr<DropState> drop;
    std::unique_ptr<Status> status;
    std::unique_ptr<Web> web;
    std::unique_ptr<Contact> contact;
    std::unique_ptr<TankState> tank;
    std::unique_ptr<ShotState> shot;
    uint32_t segPrev = 0, segNext = 0;  // centipede: toward the head, toward the tail
    uint32_t killer = 0;                // a camera whose flower was killed: what to look at
    bool removing = false;              // queued for removal at the end of the tick
};

struct Input {
    uint32_t flags = 0;
    float angle = 0, dist = 0;
};

// petal slots a flower has at a level
inline int slotCount(int level) {
    return level < 15 ? 5 : level < 30 ? 6 : level < 45 ? 7 : 8;
}
// and with the spare row of eight after them
inline int slotsWithSpares(int level) {
    return slotCount(level) + 8;
}
struct FlowerState {
    float rot = 0;              // where the petal ring has turned to
    uint8_t slots[16] = {};     // petal type in each slot, then the spare row
    uint32_t petal[8][3] = {};  // the petal entity for each slot and copy
    bool swapped[8][3] = {};    // put in by hand: reloads for at least 2.5 s
    float reload[8][3] = {};    // ms each missing copy has been reloading
    uint8_t ring = 0;           // places on the petal ring
    float levelXp = 0;          // toward the next level
    float zoneTime = 0;         // ms spent left of the level's zone, up to the limit
    bool safe = false;          // mobs leave it alone until safeUntil (ms)
    double safeUntil = 0;
    uint8_t invCount = 0;  // petals given up, oldest first, at most 16
    uint8_t inv[16] = {};
};
struct PetalState {
    uint32_t flower = 0;
    int index = 0;                // place on the ring
    int8_t slot = -1, copy = -1;  // which of the flower's petals it is
    double born = 0;              // ms
    bool used = false;            // has done its one thing (launched, popped, hatched)
    int heal = 0;                 // a rose on its way in: ticks so far
};

struct Client {
    uint32_t lastSeq = 0;  // highest message seq handled
    uint32_t packetSeq = 0;
    uint32_t camera = 0, flower = 0;
    float spawnWait = 0;    // ms before another spawn is allowed
    bool looked = false;    // has followed a flower since joining, so sees around its camera
    bool asFlower = false;  // spawns as a flower on a tank server
    Input input;
    std::string name;
    world::View view;
    struct Event {
        bool reliable;
        std::vector<uint8_t> bytes;
    };  // bytes: [varint type][payload]
    std::vector<Event> events;
};

}  // namespace game
