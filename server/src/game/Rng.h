// Park-Miller minimal standard generator (multiplier 48271) with the integer and real
// distributions written out, so every build draws the same numbers from the same seed.
#pragma once
#include <cstdint>

#include "Math.h"

namespace game {

class Rng {
public:
    explicit Rng(uint32_t seed = 1) {
        seed_ = seed % 2147483647u;
        if (!seed_) seed_ = 1;
    }

    uint32_t state() const { return seed_; }
    void setState(uint32_t s) { seed_ = s; }

    // 1 .. 2147483646
    uint32_t next() {
        seed_ = uint32_t(uint64_t(seed_) * 48271u % 2147483647u);
        return seed_;
    }

    // [0, 1) with float precision
    float unit() { return float(next() - 1) / 2147483648.0f; }
    // [lo, hi)
    float real(float lo, float hi) { return (hi - lo) * unit() + lo; }
    // [0, 2pi)
    float angle() { return unit() * float(TAU); }

    // uniformly in [lo, hi], built from 16..30-bit chunks with rejection
    uint32_t between(uint32_t lo, uint32_t hi) {
        uint32_t range = hi - lo + 1;
        if (range == 1) return lo;
        if (range == 0) return bits(32);
        int w = 31 - __builtin_clz(range);
        if (range & (0xffffffffu >> (32 - w))) w++;
        uint32_t u;
        do u = bits(w);
        while (u >= range);
        return u + lo;
    }

private:
    static constexpr uint32_t kRange = 2147483646u;  // values next() can take
    static constexpr int kBitsPerDraw = 30;

    uint32_t bits(int w) {
        int n = w / kBitsPerDraw + (w % kBitsPerDraw != 0);
        int w0 = w / n;
        uint32_t y0 = w0 < 32 ? (kRange >> w0) << w0 : 0;
        if (kRange - y0 > y0 / uint32_t(n)) {
            n++;
            w0 = w / n;
            y0 = w0 < 32 ? (kRange >> w0) << w0 : 0;
        }
        int n0 = n - w % n;
        uint32_t y1 = w0 < 31 ? (kRange >> (w0 + 1)) << (w0 + 1) : 0;
        uint32_t mask0 = w0 > 0 ? 0xffffffffu >> (32 - w0) : 0;
        uint32_t mask1 = w0 < 31 ? 0xffffffffu >> (32 - (w0 + 1)) : 0xffffffffu;
        uint32_t s = 0;
        for (int k = 0; k < n0; k++) {
            uint32_t u;
            do u = next() - 1;
            while (u >= y0);
            s = (w0 < 32 ? s << w0 : 0) + (u & mask0);
        }
        for (int k = n0; k < n; k++) {
            uint32_t u;
            do u = next() - 1;
            while (u >= y1);
            s = (w0 < 31 ? s << (w0 + 1) : 0) + (u & mask1);
        }
        return s;
    }

    uint32_t seed_;
};

}  // namespace game
