// Drawing of mobs, petals and flowers in local space: the caller translates and rotates to the
// entity; each function scales from its design radius to the given radius.
#pragma once
#include <cstdint>

#include "cp6/Context.h"
#include "game/Data.h"

namespace game {

// state that changes how a body is drawn
struct Look {
    float hurt = 0;   // 1 right after taking damage, fading to 0
    uint32_t id = 0;  // entity id, seeds the random shapes
    double time = 0;  // ms, for animations
    double walk = 0;  // mobs: how far their legs have stepped so far
    double anim = 0;  // mobs: ms of their own animation so far
    float chase = 0;  // mobs: 0 wandering .. 1 chasing a target
    bool gray = false;
    bool ui = false;              // drawn on a card instead of in the world: fixed pose, no spin
    bool dead = false;            // flowers: crossed-out eyes
    float purple = 0, light = 0;  // flowers: tinted toward purple, lightened by up to 30%
    bool antennae = false;        // flowers: two antennae on the head
    bool lumpy = false;           // flowers: a lumpy body with the face askew
};

// colour after a hit: white for the first moments, then red fading back
cp6::Color hurtColor(uint32_t rgb, float hurt);
uint32_t darker(uint32_t rgb);  // the outline colour the game derives from a fill

void drawMob(cp6::Context &c, int type, float r, const Look &l);
void drawPetal(cp6::Context &c, int type, const Look &l);
// a petal's picture as it appears on a card of the given size, centred on the origin
void drawPetalIcon(cp6::Context &c, int type, float size, const Look &l);
// face: (lookX, lookY) is where the eyes look, at most 1 long; mouth bends from -5 (frown) to 5
// (smile); angry 0..1 lowers the brows, cutting the inner top of the eyes
void drawFlower(cp6::Context &c, float r, const Look &l, float lookX, float lookY, float mouth, float angry);
// a pea in flight: green, or purple when poisonous; radius 7, or 10 when big
void drawPea(cp6::Context &c, bool poison, bool big, const Look &l);
// a web on the ground: five sagging rings and 13 spokes, 55 units across the middle, in white
void drawWeb(cp6::Context &c);

}  // namespace game
