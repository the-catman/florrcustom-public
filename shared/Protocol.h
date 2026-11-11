// What the client and server agree on: the components an entity can have and the numbers of their
// fields. A full state sends a component's fields in the order listed here; a delta goes by number.
#pragma once

namespace protocol {

enum Comp : int {
    C_DROP = 0,
    C_MOVES = 1,
    C_ROCK = 2,
    C_SPIDER = 3,
    C_PEA = 4,
    C_CAMERA = 5,
    C_PETAL = 6,
    C_ROYALE = 7,
    C_ANT = 9,
    C_ANT_HOLE = 10,
    C_WEB = 11,
    C_LADYBUG = 12,
    C_MOB = 13,
    C_OWNED = 14,
    C_ARENA = 15,
    C_BEETLE = 16,
    C_MISSILE = 17,
    C_NAME = 18,
    C_HEALTH = 19,
    C_POSITION = 20,
    C_SEGMENT = 21,
    C_CACTUS = 22,
    C_HORNET = 23,
    C_DEATH = 24,
    C_SIZE = 25,
    C_EVENT = 26,
    C_STATUS = 27,
    C_BEE = 28,
    C_FLOWER = 29,
    C_LOOT = 30,    // server only: what a mob drops and the xp it gives
    C_CATCH = 31,   // server only: a web that slows what's inside it
    C_TANK = 32,    // a diep tank's body, or anything drawn like one
    C_SHOT = 33,    // something a tank fired
    C_DRIVER = 34,  // on a camera: the tank its player drives
};

// the components clients are told about; the others carry no fields
constexpr unsigned long long kWireComps = 0x3fffffffull | 1ull << C_TANK | 1ull << C_SHOT | 1ull << C_DRIVER;

enum Field : int {
    // drop
    F_DROP_TYPE = 22,
    F_DROP_FLAGS = 54,  // flags, bit 0: going away
    // moves
    F_MOVES_STATE = 36,  // bit 0: going after a target
    // pea
    F_PEA_FLAGS = 9,  // bit 0: poison, bit 1: big
    // camera
    F_RESPAWN_LEVEL = 64,
    F_CAMERA_X = 5,
    F_KILLER = 6,
    F_KEPT = 16,
    F_CAMERA_UNUSED = 20,
    F_SCORE = 21,
    F_CAMERA_Y = 24,
    F_FOV = 32,
    F_KEPT_COUNT = 34,
    F_DEATH_LEVEL = 47,
    F_PLAYER = 56,
    // petal
    F_PETAL_TYPE = 50,
    // royale
    F_ROYALE_TIME = 13,
    F_ROYALE_STARTING = 37,
    // ant
    F_ANT_KIND = 17,
    // mob
    F_MOB_TYPE = 43,
    // owned
    F_PARENT = 23,
    F_OWNER = 57,
    // arena
    F_LB_FLOWERS = 3,
    F_LB_NAMES = 10,
    F_ARENA_STATE = 11,
    F_ARENA_MODE = 12,
    F_MAP_WIDTH = 19,
    F_ARENA_SEED = 30,
    F_LB_SCORES = 39,
    F_MAP_HEIGHT = 42,
    F_ARENA_TIME = 45,
    F_LB_ROWS = 46,
    F_PLAYERS = 52,  // state, bit 0: running, bit 1: won; mode 1: battle royale
    // name
    F_NAME = 48,
    // health
    F_HEALTH = 53,
    F_MAX_HEALTH = 55,
    // position
    F_ANGLE = 8,
    F_Y = 28,
    F_X = 49,
    // segment
    F_SEGMENT_FLAGS = 29,
    F_SEGMENT_UNUSED = 40,  // flags, bit 0: evil, bit 1: head
    // death
    F_DEAD = 38,
    // size
    F_RADIUS = 7,
    // event: never sent by this server
    F_EVENT_4 = 4,
    F_EVENT_15 = 15,
    F_EVENT_18 = 18,
    F_EVENT_25 = 25,
    F_EVENT_27 = 27,
    F_EVENT_33 = 33,
    F_EVENT_35 = 35,
    F_EVENT_41 = 41,
    F_EVENT_44 = 44,
    F_EVENT_51 = 51,
    // status
    F_STATUS_FLAGS = 14,  // bit 0: poisoned, bit 1: can't heal, bit 2: anything at all
    // flower, for everyone
    F_EYES = 2,
    F_LUMPY = 26,
    F_FACE = 31,  // eyes: 256ths of a turn; face, bit 0: attacking, bit 1: defending, bit 2: antennae
    // flower, for its owner only
    F_XP = 58,
    F_FLOWER_SCORE = 59,
    F_SLOTS = 60,
    F_COOLDOWNS = 61,
    F_LEVEL = 62,
    F_ZONE_TIMER = 63,
    // tank
    F_TANK_LAYOUT = 65,   // the tank it's drawn as
    F_TANK_FIRED = 66,    // one bit per barrel, flipped each time it fires
    F_TANK_TURRETS = 67,  // auto turrets' aim, in 65536ths of a turn
    F_TANK_SPIN = 68,     // how far a ring of turrets has turned, in 65536ths of a turn
    F_TANK_FLAGS = 69,    // bit 0: just spawned (blinks)
    F_TANK_ALPHA = 70,    // 255 shown .. 0 invisible
    F_TANK_TEAM = 71,     // the camera it fights for
    // shot
    F_SHOT_KIND = 72,
    F_SHOT_TEAM = 73,   // the camera it fights for
    F_SHOT_LOOK = 74,   // sides, 1 a circle
    F_SHOT_COLOR = 75,  // a Color
    // driver
    F_DRIVER_TANK = 76,
    F_DRIVER_LEVEL = 77,
    F_DRIVER_SCORE = 78,
    F_DRIVER_STATS = 79,   // points in each stat
    F_DRIVER_POINTS = 80,  // points to spend
    F_DRIVER_ALIVE = 81,   // seconds the last tank lived
    F_DRIVER_DEATH_TANK = 82,
    F_TANK_COLOR = 83,  // a Color
    F_TANK_SCORE = 84,  // its player's score
    F_SHOT_MOB = 85,    // a claimed mob's type plus one, drawn as that mob; 0 none
};

// what colour a tank or a shot is drawn in
enum Color : int {
    COLOR_TEAM,  // blue for your own side, red for anyone else's
    COLOR_FALLEN,
    COLOR_NECRO,
    COLOR_CRASHER,
    COLOR_SQUARE,
    COLOR_TRIANGLE,
    COLOR_NEUTRAL,
};

// what a tank shot is
enum ShotKind : int {
    SHOT_BULLET,
    SHOT_TRAP,
    SHOT_DRONE,
    SHOT_NECRO,
    SHOT_SWARM,
    SHOT_MINION,
    SHOT_SKIMMER,
    SHOT_ROCKET,
};

// the field that goes on the wire as id 1, ahead of the rest
constexpr int kFirstField = F_RESPAWN_LEVEL;

// what a client sends
enum ClientMessage : int {
    MSG_HELLO = 0,     // version, user agent, language
    MSG_CONTINUE = 4,  // leaving the death screen, or about to spawn
    MSG_SPAWN = 6,     // name
    MSG_TRASH = 8,     // slot, its petal
    MSG_SWAP = 10,     // slot, its petal, other slot, its petal
    MSG_INPUT = 12,    // flags, angle, distance
    MSG_ACTION = 13,
    MSG_STAT = 14,     // tank stat, the level it should reach
    MSG_UPGRADE = 15,  // tank to become
    MSG_DEV = 16,      // test keys, when the server allows them: 0 level up, 1 previous tank, 2 die,
                       // 3 a petal for a flower, its type next
    MSG_KIND = 17,     // what the next spawn is on a tank server: 0 a tank, 1 a flower
};

// input flags past attack (bit 0) and defend (bit 1)
constexpr unsigned kInputUp = 1u << 4;
constexpr unsigned kInputDown = 1u << 5;
constexpr unsigned kInputLeft = 1u << 6;
constexpr unsigned kInputRight = 1u << 7;

// what a server sends after the entities of an update
enum ServerEvent : int {
    EVENT_PLAYER_ID = 9,  // text
    EVENT_HIT = 11,       // entity
    EVENT_PICKUP = 14,    // entity, slot
    EVENT_NOTIFY = 16,    // text, colour, how long it shows in ms
    EVENT_DEV = 17,       // the server allows test keys
};

constexpr int kPacketUpdate = 1;

}  // namespace protocol
