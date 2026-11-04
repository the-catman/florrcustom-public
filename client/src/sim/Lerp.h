#pragma once
#include <algorithm>
#include <cmath>

namespace sim {

// A value the server moves in steps: each new value starts a straight 120 ms glide from wherever
// the value is shown now. With updates every 50 ms, steady motion comes out at a steady speed.
struct Lerp {
    float from = 0;
    float to = 0;
    double t = 0;
    bool angle = false;  // goes the short way round

    float delta() const { return angle ? std::remainder(to - from, 6.2831853f) : to - from; }
    float at(double now) const { return from + delta() * std::clamp(float(now - t) / 120.f, 0.f, 1.f); }
    void snap(float v, double now) {
        from = to = v;
        t = now;
    }
    void set(float v, double now) {
        if (v == to) return;
        from = at(now);
        to = v;
        t = now;
    }
};

}  // namespace sim
