// Drawing of tanks, bosses and what they fire, the way diep draws them: in world units around the
// origin, which the caller puts at the entity.
#pragma once
#include <cstdint>
#include <string>

#include "cp6/Context.h"

namespace game {

// how a tank or a shot looks this frame
struct TankPaint {
    uint32_t fill = 0;               // body colour
    float flash = 0;                 // 1 right after a hit, to 0 over 120 ms
    bool blink = false;              // the lighter half of the spawn blink
    double ticks = 0;                // diep ticks of local time, for guards that spin
    float angle = 0;                 // the body's
    float spin = 0;                  // the ring of auto turrets'
    const float *turrets = nullptr;  // each auto turret's aim
    const double *kickAt = nullptr;  // when each barrel last fired, ms, 0 never; turrets from 16
    double now = 0;
    float reload = 15;     // the tank's reload in ticks, which sets how long a barrel kicks
    float border = 3.75f;  // outline width, world units
};

uint32_t tankColor(int color, bool ours);
// a tank or anything built like one; k is world units per diep unit at its size
void drawTank(cp6::Context &c, int layout, float k, const TankPaint &p);
// a tank's picture on a button, scale pixels per diep unit, turned to angle: around its middle
// when centred, else around the middle of its body
void drawTankIcon(cp6::Context &c, int tank, float x, float y, float angle, float scale, bool centred);
// a bullet, trap, drone and so on, r its radius in world units; sides 1 is a circle
void drawShot(cp6::Context &c, int sides, bool star, float r, const TankPaint &p);
// a tank's name and score above it, and its health bar below, in world units
void drawTankName(cp6::Context &c, const std::string &name, float score, float x, float y, float r, int sides,
                  float alpha);
void drawTankBar(cp6::Context &c, float x, float y, float r, float health, float alpha);

}  // namespace game
