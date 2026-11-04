// What the drawing code for mobs, petals and flowers has in common.
#pragma once
#include <cstdint>

#include "game/Draw.h"

namespace game {

using cp6::Color;
using cp6::Context;

inline constexpr float PI = 3.14159265f;
inline constexpr float TAU = 6.28318531f;

// a colour tinted by damage: white for the first moments, then red fading back
inline uint32_t tint(uint32_t rgb, float t) {
    if (t <= 0) return rgb;
    uint32_t to = t >= .9f ? 0xffffff : 0xff0000, out = 0;
    for (int s = 0; s <= 16; s += 8) out |= uint32_t(t * float(to >> s & 255) + (1 - t) * float(rgb >> s & 255)) << s;
    return out;
}

// a fill tinted by damage, and the outline darkened from it
struct Paint {
    Color fill, stroke;
};
inline Paint paint(uint32_t rgb, const Look &l) {
    uint32_t f = tint(rgb, l.hurt);
    return {Color(f), Color(darker(f))};
}
inline void usePaint(Context &c, uint32_t rgb, const Look &l, float lineWidth = 3) {
    Paint p = paint(rgb, l);
    c.lineWidth(lineWidth);
    c.fillStyle(p.fill);
    c.strokeStyle(p.stroke);
}

// minstd, as the game uses for shapes seeded by an entity id
struct ShapeRng {
    uint32_t s;
    explicit ShapeRng(uint32_t id) {
        s = ((id & 16383) ^ id >> 14) % 2147483647u;
        if (!s) s = 1;
    }
    uint32_t next() {
        s = uint32_t(uint64_t(s) * 48271 % 2147483647u);
        return s;
    }
};

inline void circlePath(Context &c, float x, float y, float r) {
    c.beginPath();
    c.arc(x, y, r, 0, TAU);
}

}  // namespace game
