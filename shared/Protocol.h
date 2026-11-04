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
    C_LOOT = 30,   // server only: what a mob drops and the xp it gives
    C_CATCH = 31,  // server only: a web that slows what's inside it
};

// the components clients are told about, in the order a full state sends them; the others carry no fields
constexpr unsigned kWireComps = 0x3fffffffu;

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
};

// what a server sends after the entities of an update
enum ServerEvent : int {
    EVENT_PLAYER_ID = 9,  // text
    EVENT_HIT = 11,       // entity
    EVENT_PICKUP = 14,    // entity, slot
};

constexpr int kPacketUpdate = 1;

}  // namespace protocol
