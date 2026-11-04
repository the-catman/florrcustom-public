#include "game/DrawCommon.h"

#include <cmath>

namespace game {

uint32_t darker(uint32_t rgb) {
    auto ch = [](uint32_t v) {
        float f = std::sqrt(v / 255.f) * .9f;
        return uint32_t(std::lround(f * f * 255.f));
    };
    return ch(rgb >> 16 & 255) << 16 | ch(rgb >> 8 & 255) << 8 | ch(rgb & 255);
}

Color hurtColor(uint32_t rgb, float t) {
    return Color(tint(rgb, t));
}

}  // namespace game
