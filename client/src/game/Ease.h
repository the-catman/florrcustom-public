// How animated values move toward where they're going, whatever the frame rate.
#pragma once
#include <algorithm>
#include <cmath>

namespace game {

// How far along the gap to its target a value moves in dt ms, as x += (target - x) * ease(rate, dt).
// rate is how much of the gap is still left after one frame at 60 fps.
inline float ease(float rate, float dt) { return std::min(1.f, 1 - std::pow(rate, dt * .06f)); }

// ms since this timer last ran, at most limit; 16 the first time
inline float frameTime(double &last, double now, float limit = 250.f) {
    float dt = last > 0 ? std::min(limit, float(now - last)) : 16.f;
    last = now;
    return dt;
}

}  // namespace game
