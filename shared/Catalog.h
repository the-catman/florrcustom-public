// What kinds of petals and mobs there are, and what the client and server both need to know about
// them. The types go on the wire.
#pragma once
#include <cstdint>

namespace catalog {

enum PetalType : int {
    P_NONE = 0, P_BASIC, P_FAST, P_HEAVY, P_ROCK, P_SQUARE, P_ROSE, P_STINGER, P_IRIS, P_WING, P_MISSILE,
    P_PEAS, P_POISON_PEAS, P_CACTUS, P_FASTER, P_EPIC_ROSE, P_TWIN, P_TRIPLET, P_BUBBLE, P_POLLEN,
    P_DANDELION, P_POISON_CACTUS, P_EGG, P_BEETLE_EGG, P_ROSE_TRIO, P_ANTENNAE, P_HEAVIEST, P_STINGER_TRIO,
    P_YIN_YANG, P_WEB, P_WEB_TRIO, P_HONEY, P_LEAF, P_CACTUS_TRIO, P_BIG_PEAS, P_UNIQUE_BASIC, P_SALT, P_RICE,
    P_CORN, P_COUNT
};

// the server has every petal up to here; the last ones are only known to the client
constexpr int kServerPetals = P_CORN;

enum MobType : int {
    M_NONE = 0, M_ROCK, M_BOULDER, M_CACTUS, M_LADYBUG, M_MASSIVE_LADYBUG, M_BEE, M_BABY_ANT, M_WORKER_ANT,
    M_SOLDIER_ANT, M_QUEEN_ANT, M_ANT_HOLE, M_BEETLE, M_MASSIVE_BEETLE, M_HORNET, M_CENTIPEDE,
    M_CENTIPEDE_BODY, M_EVIL_CENTIPEDE, M_EVIL_CENTIPEDE_BODY, M_SQUARE, M_DARK_LADYBUG, M_SHINY_LADYBUG,
    M_SPIDER, M_COUNT
};

struct Drop {
    uint8_t petal;
    float chance;  // 0 to 1
};

struct MobDef {
    const char *name;
    const char *desc;
    uint8_t rarity;
    float xp;
    uint8_t dropCount;
    Drop drops[8];  // likeliest last
};

struct PetalDef {
    const char *name;
    const char *desc;
    uint8_t rarity;
    uint8_t count;  // petals per slot
    float reload;   // ms
    float label;    // font size of the name on its card
    bool passive;   // works just by being in a slot, with no petal out on the ring
    float cluster;  // when above 0: its petals orbit together, as one, this far from their middle
    const char *desc2 = "";

    bool clustered() const { return cluster > 0; }
};

// clang-format off
inline constexpr MobDef kMobs[M_COUNT] = {
    /* M_NONE */ {"None", "", 0, 0.0f, 0, {}},
    /* M_ROCK */ {"Rock", "A rock. It doesn't do much.", 0, 1.0f, 3, {{P_ROCK, 0.0052214065f}, {P_FAST, 0.034809377f}, {P_HEAVY, 0.104428135f}}},
    /* M_BOULDER */ {"Boulder", "A bigger rock. It doesn't do much either.", 1, 10.0f, 3, {{P_HEAVIEST, 0.0033121947f}, {P_ROCK, 0.041402433f}, {P_HEAVY, 0.82804865f}}},
    /* M_CACTUS */ {"Cactus", "Avoid touching it, it hurts.", 0, 2.0f, 5, {{P_CACTUS_TRIO, 0.000052757634f}, {P_POISON_CACTUS, 0.0010551526f}, {P_MISSILE, 0.008792939f}, {P_CACTUS, 0.026378816f}, {P_STINGER, 0.052757632f}}},
    /* M_LADYBUG */ {"Ladybug", "Cute and harmless.", 0, 3.0f, 5, {{P_WING, 0.0019854326f}, {P_BUBBLE, 0.003745328f}, {P_TWIN, 0.018574625f}, {P_FAST, 0.06750471f}, {P_ROSE, 0.11113633f}}},
    /* M_MASSIVE_LADYBUG */ {"Massive Ladybug", "This one is really big, but still cute.", 3, 50.0f, 4, {{P_ROSE, 1.0f}, {P_BUBBLE, 1.0f}, {P_ROSE_TRIO, 1.0f}, {P_EPIC_ROSE, 1.0f}}},
    /* M_BEE */ {"Bee", "It stings. Don't touch it.", 0, 3.0f, 6, {{P_WING, 0.0034244447f}, {P_BUBBLE, 0.006459886f}, {P_HONEY, 0.011643112f}, {P_TWIN, 0.03203724f}, {P_STINGER, 0.06985867f}, {P_FAST, 0.11643112f}}},
    /* M_BABY_ANT */ {"Baby Ant", "Weak and defenseless, but big dreams.", 0, 1.0f, 5, {{P_TRIPLET, 0.000568353f}, {P_RICE, 0.0052288473f}, {P_TWIN, 0.11989766f}, {P_LEAF, 0.26144236f}, {P_FAST, 0.43573728f}}},
    /* M_WORKER_ANT */ {"Worker Ant", "It's a bit temperamental, probably from working all the time.", 0, 2.0f, 5, {{P_TRIPLET, 0.0006008149f}, {P_CORN, 0.0055274973f}, {P_TWIN, 0.12674572f}, {P_LEAF, 0.27637485f}, {P_FAST, 0.46062475f}}},
    /* M_SOLDIER_ANT */ {"Soldier Ant", "It's got wings and it's ready to use them.", 1, 5.0f, 5, {{P_TRIPLET, 0.00037516782f}, {P_WING, 0.008459667f}, {P_FASTER, 0.029625f}, {P_TWIN, 0.07914403f}, {P_IRIS, 0.1159136f}}},
    /* M_QUEEN_ANT */ {"Queen Ant", "You must have done something really bad if she's chasing you.", 2, 20.0f, 6, {{P_STINGER_TRIO, 0.006382623f}, {P_TRIPLET, 0.013875268f}, {P_EGG, 0.06382623f}, {P_WING, 0.3128737f}, {P_TWIN, 1.0f}, {P_FASTER, 1.0f}}},
    /* M_ANT_HOLE */ {"Ant Hole", "Ants go in, and come out. Can't explain that.", 2, 20.0f, 3, {{P_EGG, 0.06382623f}, {P_WING, 0.3128737f}, {P_IRIS, 1.0f}}},
    /* M_BEETLE */ {"Beetle", "It's hungry and flowers are its favorite meal.", 1, 5.0f, 4, {{P_TRIPLET, 0.00027750537f}, {P_WING, 0.006257474f}, {P_SALT, 0.06382623f}, {P_IRIS, 0.08573935f}}},
    /* M_MASSIVE_BEETLE */ {"Massive Beetle", "Someone overfed this one, you might be next.", 2, 50.0f, 4, {{P_BEETLE_EGG, 0.006382623f}, {P_TRIPLET, 0.013875268f}, {P_WING, 0.3128737f}, {P_IRIS, 1.0f}}},
    /* M_HORNET */ {"Hornet", "These aren't quite as nice as the little bees.", 1, 10.0f, 6, {{P_ANTENNAE, 0.0002824664f}, {P_WING, 0.027692787f}, {P_BUBBLE, 0.052239783f}, {P_MISSILE, 0.094155475f}, {P_HONEY, 0.094155475f}, {P_DANDELION, 0.1412332f}}},
    /* M_CENTIPEDE */ {"Centipede", "It's just there doing its thing.", 1, 3.0f, 6, {{P_TRIPLET, 0.00011612784f}, {P_POISON_PEAS, 0.00053418806f}, {P_TWIN, 0.024497906f}, {P_PEAS, 0.026709404f}, {P_LEAF, 0.053418808f}, {P_FAST, 0.089031346f}}},
    /* M_CENTIPEDE_BODY */ {"Centipede", "It's just there doing its thing.", 1, 3.0f, 6, {{P_TRIPLET, 0.00011612784f}, {P_POISON_PEAS, 0.00053418806f}, {P_TWIN, 0.024497906f}, {P_PEAS, 0.026709404f}, {P_LEAF, 0.053418808f}, {P_FAST, 0.089031346f}}},
    /* M_EVIL_CENTIPEDE */ {"Evil Centipede", "This one loves flowers.", 2, 5.0f, 4, {{P_BIG_PEAS, 0.0012274276f}, {P_TRIPLET, 0.002668321f}, {P_POISON_PEAS, 0.0122742755f}, {P_IRIS, 0.8244168f}}},
    /* M_EVIL_CENTIPEDE_BODY */ {"Evil Centipede", "This one loves flowers.", 2, 5.0f, 4, {{P_BIG_PEAS, 0.0012274276f}, {P_TRIPLET, 0.002668321f}, {P_POISON_PEAS, 0.0122742755f}, {P_IRIS, 0.8244168f}}},
    /* M_SQUARE */ {"Square", "This shape... it looks familiar...", 5, 10.0f, 1, {{P_SQUARE, 1.0f}}},
    /* M_DARK_LADYBUG */ {"Ladybug", "Cute and harmless... if left unprovoked.", 1, 5.0f, 6, {{P_EPIC_ROSE, 0.001621223f}, {P_TRIPLET, 0.0019384188f}, {P_YIN_YANG, 0.017833455f}, {P_WING, 0.043709446f}, {P_BUBBLE, 0.08245367f}, {P_ROSE_TRIO, 0.35729924f}}},
    /* M_SHINY_LADYBUG */ {"Ladybug", "Shiny, cute and mostly harmless.", 1, 5.0f, 6, {{P_TRIPLET, 0.0018350482f}, {P_WING, 0.04137854f}, {P_BUBBLE, 0.07805664f}, {P_ROSE_TRIO, 0.33824542f}, {P_TWIN, 0.38711506f}, {P_ROSE, 1.0f}}},
    /* M_SPIDER */ {"Spider", "Spooky.", 1, 5.0f, 5, {{P_WEB_TRIO, 0.00017588958f}, {P_FASTER, 0.03019361f}, {P_WEB, 0.08794479f}, {P_IRIS, 0.118138395f}, {P_STINGER, 0.17588958f}}},
};

inline constexpr PetalDef kPetals[P_COUNT] = {
    /* P_NONE */ {"None", "", 0, 1, 0.0f, 14.0f, false, 0.0f},
    /* P_BASIC */ {"Basic", "A nice petal, not too strong but not too weak", 0, 1, 2500.0f, 14.0f, false, 0.0f},
    /* P_FAST */ {"Fast", "Weaker than most petals, but recharges very quickly", 0, 1, 500.0f, 14.0f, false, 0.0f},
    /* P_HEAVY */ {"Heavy", "Very resilient and deals more damage, but recharges very slowly", 0, 1, 5500.0f, 14.0f, false, 0.0f},
    /* P_ROCK */ {"Rock", "Extremely durable, but takes a long time to recharge", 2, 1, 10000.0f, 14.0f, false, 0.0f},
    /* P_SQUARE */ {"Square", "This shape... it looks familiar...", 5, 1, 2500.0f, 14.0f, false, 0.0f},
    /* P_ROSE */ {"Rose", "Its healing properties are amazing. Not so good at combat though", 1, 1, 3500.0f, 14.0f, false, 0.0f},
    /* P_STINGER */ {"Stinger", "It really hurts, but it's very fragile", 1, 1, 4000.0f, 14.0f, false, 0.0f},
    /* P_IRIS */ {"Iris", "Very poisonous, but takes a little while to do its work", 1, 1, 6000.0f, 14.0f, false, 0.0f},
    /* P_WING */ {"Wing", "It comes and goes", 2, 1, 1250.0f, 14.0f, false, 0.0f},
    /* P_MISSILE */ {"Missile", "You can actually shoot this one", 2, 1, 3000.0f, 14.0f, false, 0.0f},
    /* P_PEAS */ {"Peas", "4 in 1 deal", 2, 1, 1000.0f, 14.0f, false, 0.0f},
    /* P_POISON_PEAS */ {"Peas", "4 in 1 deal, now with a secret ingredient: poison", 3, 1, 1000.0f, 14.0f, false, 0.0f},
    /* P_CACTUS */ {"Cactus", "Not very strong, but somehow increases your maximum health", 2, 1, 1000.0f, 14.0f, false, 0.0f},
    /* P_FASTER */ {"Faster", "It's so light it makes your other petals spin faster", 2, 1, 500.0f, 14.0f, false, 0.0f},
    /* P_EPIC_ROSE */ {"Rose", "Extremely powerful rose, almost unheard of", 3, 1, 3500.0f, 14.0f, false, 0.0f},
    /* P_TWIN */ {"Twin", "Why stop at one? Why not TWO?!", 1, 2, 1000.0f, 14.0f, false, 0.0f},
    /* P_TRIPLET */ {"Triplet", "How about THREE?!", 3, 3, 1000.0f, 14.0f, false, 0.0f},
    /* P_BUBBLE */ {"Bubble", "You can right click to pop it and propel your flower", 2, 1, 3500.0f, 14.0f, false, 0.0f},
    /* P_POLLEN */ {"Pollen", "Asthmatics beware", 3, 3, 1000.0f, 14.0f, false, 0.0f},
    /* P_DANDELION */ {"Dandelion", "Its interesting properties prevent healing effects on affected units", 2, 1, 2000.0f, 11.0f, false, 0.0f},
    /* P_POISON_CACTUS */ {"Cactus", "Increases your maximum health and makes your flower toxic", 3, 1, 1000.0f, 14.0f, false, 0.0f, "Enemies hit by your flower will get poisoned"},
    /* P_EGG */ {"Egg", "Something interesting might pop out of this", 3, 1, 1000.0f, 14.0f, false, 0.0f},
    /* P_BEETLE_EGG */ {"Egg", "Something interesting might pop out of this", 4, 1, 1000.0f, 14.0f, false, 0.0f},
    /* P_ROSE_TRIO */ {"Rose", "Its healing properties are amazing. Not so good at combat though", 2, 3, 3500.0f, 14.0f, false, 11.0f},
    /* P_ANTENNAE */ {"Antennae", "Allows your flower to sense foes farther away", 4, 1, 0.0f, 12.0f, true, 0.0f},
    /* P_HEAVIEST */ {"Heaviest", "This thing is so heavy that nothing gets in the way", 3, 1, 20000.0f, 12.0f, false, 0.0f},
    /* P_STINGER_TRIO */ {"Stinger", "It really hurts, but it's very fragile", 4, 3, 4000.0f, 14.0f, false, 11.0f},
    /* P_YIN_YANG */ {"Yin Yang", "This mysterious petal reverses the rotation direction of your petals", 3, 1, 1000.0f, 12.0f, false, 0.0f},
    /* P_WEB */ {"Web", "It's really sticky", 2, 1, 3000.0f, 14.0f, false, 0.0f},
    /* P_WEB_TRIO */ {"Web", "It's really sticky", 4, 3, 3000.0f, 14.0f, false, 13.0f},
    /* P_HONEY */ {"Honey", "This is so sticky it slows down whatever it hits", 2, 1, 1700.0f, 14.0f, false, 0.0f},
    /* P_LEAF */ {"Leaf", "Gathers energy from the sun to heal your flower passively", 1, 1, 1000.0f, 14.0f, false, 0.0f},
    /* P_CACTUS_TRIO */ {"Cactus", "Not very strong, but somehow increases your maximum health", 4, 3, 1000.0f, 14.0f, false, 15.0f},
    /* P_BIG_PEAS */ {"Peas", "4 in 1 deal, now with a secret ingredient: poison", 4, 1, 1000.0f, 14.0f, false, 0.0f},
    /* P_UNIQUE_BASIC */ {"Basic", "Something incredibly rare and useless", 5, 1, 2500.0f, 14.0f, false, 0.0f},
    /* P_SALT */ {"Salt", "Reflects some of the damage you take back to the flower that dealt it", 2, 1, 2500.0f, 14.0f, false, 0.0f, "Does not stack with itself"},
    /* P_RICE */ {"Rice", "Spawns instantly, but not very strong", 3, 1, 0.0f, 14.0f, false, 0.0f},
    /* P_CORN */ {"Corn", "Takes a long time to spawn, but has a lot of health", 3, 1, 45000.0f, 14.0f, false, 0.0f},
};
// clang-format on

}  // namespace catalog
