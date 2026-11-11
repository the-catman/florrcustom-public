// Diep's level and stat point rules, which the server and the HUD both need.
#pragma once
#include <algorithm>
#include <cmath>

namespace tankdata {

constexpr int kMaxLevel = 45;

// score to reach a level
inline double levelScore(int level) {
    static double table[kMaxLevel] = {};
    static bool ready = false;
    if (!ready) {
        for (int i = 1; i < kMaxLevel; i++)
            table[i] = table[i - 1] + 40.0 / 9 * std::pow(1.06, i - 1) * std::min(31, i);
        ready = true;
    }
    if (level >= kMaxLevel) return table[kMaxLevel - 1];
    if (level <= 0) return 0;
    return table[level - 1];
}

// stat points a tank has had by a level
inline int statPoints(int level) {
    if (level <= 0) return 0;
    if (level <= 28) return level - 1;
    return level / 3 + 18;
}

}  // namespace tankdata
