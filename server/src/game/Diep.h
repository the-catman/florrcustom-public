// Diep's numbers in florr's world. Diep moves things once every 40 ms tick and draws a level 1 tank
// 50 units across its middle; here time is in ms and that tank is 25 across, and health runs
// twice as high to match florr's.
#pragma once
#include <algorithm>
#include <cmath>

#include "TankRules.h"

namespace game::diep {

constexpr double kLength = 0.5;    // florr units per diep unit
constexpr double kTickMs = 40;     // one diep tick
constexpr double kHealth = 2;      // florr health per diep health
constexpr double kScorePerXp = 4;  // a mob's xp, as diep score

// what a diep velocity keeps of itself each ms (0.9 a tick)
inline double decay() {
    static const double d = std::pow(0.9, 1 / kTickMs);
    return d;
}
// florr's bodies keep 0.992 a ms
constexpr double kFlorrDecay = 0.992;

// A diep velocity change of v units a tick as a change in florr velocity per ms, for a body that
// keeps `keep` of its velocity each ms: it carries the body as far in all as v carries a diep one.
inline double push(double v, double keep) {
    return v * 10 * kLength * (1 - keep);
}

using tankdata::kMaxLevel;
using tankdata::levelScore;
using tankdata::statPoints;

}  // namespace game::diep
