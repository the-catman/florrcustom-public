#pragma once
#include <cstdint>
#include <limits>

inline constexpr double PI = 3.141592653589793;
inline constexpr double HALF_PI = 1.5707963267948966;
inline constexpr double TAU = 6.283185307179586;
inline constexpr double EPSILON = std::numeric_limits<double>::epsilon();
inline constexpr float FLOAT_EPSILON = std::numeric_limits<float>::epsilon();

// the whole part, or 0 when it doesn't fit
inline uint32_t toU32(double v) {
    return v < 4294967296.0 && v >= 0 ? uint32_t(v) : 0;
}
