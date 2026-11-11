#include "game/DrawTank.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "Protocol.h"
#include "TankData.h"

using namespace protocol;
using namespace tankdata;

namespace game {

using cp6::Color;
using cp6::Context;

static constexpr float kPi = 3.14159265f;
static constexpr float kLength = .5f;  // world units per diep unit
static constexpr uint32_t kBarrel = 0x999999;
static constexpr uint32_t kGuard = 0x555555;

uint32_t tankColor(int color, bool ours) {
    switch (color) {
        case COLOR_TEAM: return ours ? 0x00b2e1 : 0xf14e54;
        case COLOR_FALLEN: return 0xc0c0c0;
        case COLOR_NECRO: return 0xfcc376;
        case COLOR_CRASHER: return 0xf177dd;
        case COLOR_SQUARE: return 0xffe869;
        case COLOR_TRIANGLE: return 0xfc7677;
        default: return 0xffe869;
    }
}

static uint32_t mix(uint32_t a, uint32_t b, float t) {
    auto ch = [&](int s) {
        float x = float(a >> s & 255), y = float(b >> s & 255);
        return uint32_t(std::clamp(int(std::lround(x + (y - x) * t)), 0, 255)) << s;
    };
    return ch(16) | ch(8) | ch(0);
}

// a hit flashes the fill and the outline white and then red; the spawn blink lightens the fill
static void paint(Context &c, uint32_t rgb, const TankPaint &p) {
    uint32_t fill = rgb, stroke = mix(rgb, 0, .25f);
    if (p.flash > 0) {
        uint32_t to = p.flash >= .75f ? 0xffffff : 0xff0000;
        fill = mix(fill, to, p.flash);
        stroke = mix(stroke, to, p.flash);
    }
    if (p.blink) {
        auto ch = [&](int s) { return uint32_t(std::min(255.f, float(fill >> s & 255) * .7f + 76.5f)) << s; };
        fill = ch(16) | ch(8) | ch(0);
    }
    c.fillStyle(Color(fill));
    c.strokeStyle(Color(stroke));
    c.lineWidth(p.border);
}

// one of diep's shapes: a circle (sides 1), a barrel (sides 2, size long and width across, the far
// end widened when a trapezoid), or a polygon whose corners are size * sqrt 2 out
static void shape(Context &c, int sides, float size, float width, float x, float y, float a, bool trapezoid,
                  bool star = false) {
    if (sides == 1) {
        c.circle(x, y, size, true, true);
        return;
    }
    float ca = std::cos(a), sa = std::sin(a);
    bool first = true;
    auto pt = [&](float px, float py) {
        float wx = x + px * ca - py * sa, wy = y + px * sa + py * ca;
        if (first)
            c.moveTo(wx, wy);
        else
            c.lineTo(wx, wy);
        first = false;
    };
    c.beginPath();
    if (sides == 2) {
        float far = trapezoid ? width * .875f : width * .5f;
        pt(-size * .5f, -width * .5f);
        pt(size * .5f, -far);
        pt(size * .5f, far);
        pt(-size * .5f, width * .5f);
    } else {
        int count = sides << (star ? 1 : 0);
        float radius = size * 1.414214f, offset = count == 4 ? kPi / 4 : 0;
        for (int i = 0; i < count; i++) {
            float r = star && !(i & 1) ? radius * .4f : radius;
            float t = offset + i * 2 * kPi / count;
            pt(std::cos(t) * r, std::sin(t) * r);
        }
    }
    c.closePath();
    c.fill();
    c.stroke();
}

// how far a barrel of this width has kicked back since it fired: a bump over as many ticks as it
// takes to reload, from one to seven
static float kick(const TankPaint &p, int i, float width, float reload) {
    if (!p.kickAt || i >= 24 || p.kickAt[i] == 0) return 0;
    float ticks = float(int(std::clamp(reload, 1.f, 7.f)));
    float t = float(p.now - p.kickAt[i]) / 40.f / ticks;
    if (t < 0 || t >= 1) return 0;
    return std::sin(t * kPi) * 5 * (width / -42.f);
}

struct Guard {
    int sides;
    float ratio, offset, perTick;
};

static int guards(Addon a, Guard *out) {
    switch (a) {
        case A_DOMBASE: out[0] = {6, 1.24f, 0, 0}; return 1;
        case A_SMASHER:
        case A_AUTOSMASHER: out[0] = {6, 1.15f, 0, .1f}; return 1;
        case A_LANDMINE:
            out[0] = {6, 1.15f, 0, .1f};
            out[1] = {6, 1.15f, 0, .05f};
            return 2;
        case A_SPIKE:
            for (int i = 0; i < 4; i++)
                out[i] = {3, 1.3f, (i == 0 ? 0 : i == 1 ? kPi / 3 : i == 2 ? kPi / 6 : kPi / 2), .17f};
            return 4;
        default: return 0;
    }
}

static int ringCount(Addon a) {
    return a == A_AUTO2 ? 2 : a == A_AUTO3 ? 3 : a == A_AUTO5 ? 5 : a == A_AUTO7 ? 7 : 0;
}

void drawTank(Context &c, int lay, float k, const TankPaint &p) {
    const TankDef *def = layout(lay);
    if (!def) return;
    float base = def->baseSize, ca = std::cos(p.angle), sa = std::sin(p.angle);
    // a point on the tank, from diep units around it to world units
    auto at = [&](float lx, float ly, float &wx, float &wy) {
        wx = (lx * ca - ly * sa) * k;
        wy = (lx * sa + ly * ca) * k;
    };
    auto part = [&](float lx, float ly, float la, float size, float width, bool trapezoid) {
        float wx, wy;
        at(lx, ly, wx, wy);
        shape(c, 2, size * k, width * k, wx, wy, p.angle + la, trapezoid);
    };
    // an auto turret: its barrel, kicking along its aim, then its round base
    auto turret = [&](int i, float wx, float wy) {
        float ta = p.turrets ? p.turrets[i] : 0;
        float d = 27.5f + kick(p, 16 + i, 29.4f, p.reload * kAutoTurret.reload);
        paint(c, kBarrel, p);
        shape(c, 2, 55 * k, 29.4f * k, wx + std::cos(ta) * d * k, wy + std::sin(ta) * d * k, ta, false);
        c.circle(wx, wy, 25 * k, true, true);
    };
    int turrets = 0;
    auto addonBelow = [&](Addon a) {
        Guard g[4];
        int n = guards(a, g);
        paint(c, kGuard, p);
        for (int i = 0; i < n; i++)
            shape(c, g[i].sides, base * g[i].ratio * .7071068f * k, 0, 0, 0,
                  g[i].offset + float(g[i].perTick * p.ticks), false);
        paint(c, kBarrel, p);
        if (a == A_LAUNCHER) {
            float size = 65.5f * 1.414214f / 50 * base;
            part(size / 2, 0, 0, size, 33.6f / 50 * base, true);
        } else if (a == A_PRONOUNCED) {
            part(40.f / 50 * base, 0, kPi, base, 42.f / 50 * base, true);
        } else if (a == A_DOMPRONOUNCED) {
            part(base, 0, kPi, 22.f / 50 * base, 35.f / 50 * base, true);
        }
        int ring = ringCount(a);
        for (int i = 0; i < ring; i++) {
            float slot = 2 * kPi * i / ring, r = base * .8f * k;
            turret(turrets++, std::cos(slot + p.spin) * r, std::sin(slot + p.spin) * r);
        }
        if (a == A_AUTOTURRET || a == A_AUTOSMASHER) turrets++;
    };

    int single = turrets;
    addonBelow(def->preAddon);
    paint(c, kBarrel, p);
    for (int i = 0; i < def->barrelCount; i++) {
        const BarrelDef &b = def->barrels[i];
        float a = b.angle, la = a + b.trapezoidDirection;
        float lx = std::cos(a) * b.size / 2 - std::sin(a) * b.offset;
        float ly = std::sin(a) * b.size / 2 + std::cos(a) * b.offset;
        float off = kick(p, i, b.width, p.reload * b.reload);
        lx += std::cos(la + b.trapezoidDirection) * off;
        ly += std::sin(la + b.trapezoidDirection) * off;
        if (b.addon == BA_TRAP_LAUNCHER) {
            float sz = b.width * 20 / 42, d = (b.size + sz) / 2;
            part(lx + std::cos(la) * d, ly + std::sin(la) * d, la, sz, b.width, true);
        }
        part(lx, ly, la, b.size, b.width, b.trapezoid);
    }
    int afterPre = turrets;
    addonBelow(def->postAddon);

    paint(c, p.fill, p);
    shape(c, def->sides, base * k, 0, 0, 0, p.angle, false);

    // above the body: a single auto turret, and the Defender's three
    if (def->preAddon == A_AUTOTURRET || def->preAddon == A_AUTOSMASHER) turret(single, 0, 0);
    if (def->postAddon == A_AUTOTURRET || def->postAddon == A_AUTOSMASHER) turret(afterPre, 0, 0);
    if (lay == L_DEFENDER)
        for (int i = 0; i < 3; i++) {
            float wx, wy, a = 2 * kPi * i / 3;
            at(std::cos(a) * 60, std::sin(a) * 60, wx, wy);
            turret(i, wx, wy);
        }
}

// the box around a tank at angle 0 as its picture shows it, in diep units
static void tankBounds(const TankDef &def, float &x0, float &y0, float &x1, float &y1) {
    x0 = y0 = x1 = y1 = 0;
    auto box = [&](float px, float py, float a, float size, float width, bool trapezoid) {
        float ca = std::cos(a), sa = std::sin(a), wide = trapezoid ? width * .875f : width / 2;
        const float xs[4] = {-size / 2, -size / 2, size / 2, size / 2}, ys[4] = {-width / 2, width / 2, -wide, wide};
        for (int k = 0; k < 4; k++) {
            float rx = px + xs[k] * ca - ys[k] * sa, ry = py + xs[k] * sa + ys[k] * ca;
            x0 = std::min(x0, rx), x1 = std::max(x1, rx), y0 = std::min(y0, ry), y1 = std::max(y1, ry);
        }
    };
    auto round = [&](float px, float py, float r) {
        x0 = std::min(x0, px - r), x1 = std::max(x1, px + r), y0 = std::min(y0, py - r), y1 = std::max(y1, py + r);
    };
    auto turret = [&](float px, float py, float a) {
        box(px + std::cos(a) * 27.5f, py + std::sin(a) * 27.5f, a, 55, 29.4f, false);
        round(px, py, 25);
    };
    float base = def.baseSize;
    round(0, 0, def.sides == 1 ? base : base * 1.414214f);
    for (Addon a : {def.preAddon, def.postAddon}) {
        Guard g[4];
        int n = guards(a, g);
        for (int i = 0; i < n; i++) round(0, 0, base * g[i].ratio);
        if (a == A_LAUNCHER) {
            float size = 65.5f * 1.414214f / 50 * base;
            box(size / 2, 0, 0, size, 33.6f / 50 * base, true);
        } else if (a == A_PRONOUNCED) {
            box(40.f / 50 * base, 0, kPi, base, 42.f / 50 * base, true);
        } else if (a == A_DOMPRONOUNCED) {
            box(base, 0, kPi, 22.f / 50 * base, 35.f / 50 * base, true);
        } else if (a == A_AUTOTURRET || a == A_AUTOSMASHER) {
            turret(0, 0, kPi);
        }
        int ring = ringCount(a);
        for (int i = 0; i < ring; i++) {
            float s = 2 * kPi * i / ring;
            turret(std::cos(s) * base * .8f, std::sin(s) * base * .8f, s);
        }
    }
    for (int i = 0; i < def.barrelCount; i++) {
        const BarrelDef &b = def.barrels[i];
        float a = b.angle, la = a + b.trapezoidDirection;
        float bx = std::cos(a) * b.size / 2 - std::sin(a) * b.offset,
              by = std::sin(a) * b.size / 2 + std::cos(a) * b.offset;
        box(bx, by, la, b.size, b.width, b.trapezoid);
        if (b.addon == BA_TRAP_LAUNCHER) {
            float sz = b.width * 20 / 42, d = (b.size + sz) / 2;
            box(bx + std::cos(la) * d, by + std::sin(la) * d, la, sz, b.width, true);
        }
    }
}

void drawTankIcon(Context &c, int tank, float x, float y, float angle, float scale, bool centred) {
    const TankDef *def = layout(tank);
    if (!def) return;
    float aims[8] = {};
    int n = 0;
    for (Addon a : {def->preAddon, def->postAddon}) {
        if (a == A_AUTOTURRET || a == A_AUTOSMASHER) aims[n++] = kPi;
        int ring = ringCount(a);
        for (int i = 0; i < ring && n < 8; i++) aims[n++] = 2 * kPi * i / ring;
    }
    TankPaint p;
    p.fill = 0x00b2e1;
    p.turrets = aims;
    p.border = 7.5f * scale;
    c.save();
    c.translate(x, y);
    c.rotate(angle);
    if (centred) {
        float x0, y0, x1, y1;
        tankBounds(*def, x0, y0, x1, y1);
        c.translate(-(x0 + x1) / 2 * scale, -(y0 + y1) / 2 * scale);
    }
    drawTank(c, tank, scale, p);
    c.restore();
}

void drawShot(Context &c, int sides, bool star, float r, const TankPaint &p) {
    paint(c, p.fill, p);
    shape(c, sides, r, 0, 0, 0, p.angle, false, star);
}

static std::string formatScore(float v) {
    char buf[32];
    if (v < 1000)
        std::snprintf(buf, sizeof buf, "%.0f", v);
    else if (v < 1000000)
        std::snprintf(buf, sizeof buf, "%.1fk", v / 1000);
    else
        std::snprintf(buf, sizeof buf, "%.1fm", v / 1000000);
    return buf;
}

static void label(Context &c, const std::string &s, float x, float y, float size) {
    c.font(size);
    c.textAlign(cp6::Align::Center);
    c.textBaseline(cp6::Baseline::Middle);
    c.lineWidth(size * .2f);
    c.strokeStyle(Color(0x555555));
    c.strokeText(s, x, y);
    c.fillStyle(Color(0xffffff));
    c.fillText(s, x, y);
}

void drawTankName(Context &c, const std::string &name, float score, float x, float y, float r, int sides, float alpha) {
    float size = r / kLength, reach = sides == 1 ? r : r * 1.414214f;
    float top = y - 1.5f * reach, scale = size / 50 * kLength;
    c.globalAlpha(alpha);
    if (score > 0) {
        label(c, formatScore(score), x, top, 25 * scale);
        top -= 25 * scale * 1.4f;
    }
    label(c, name, x, top, 40 * scale);
}

void drawTankBar(Context &c, float x, float y, float r, float health, float alpha) {
    float w = 2 * r, by = y + r + 35 * kLength;
    c.globalAlpha(alpha);
    c.lineCap(cp6::Cap::Round);
    c.lineWidth(16 * kLength);
    c.strokeStyle(Color(0x555555));
    c.beginPath();
    c.moveTo(x - w / 2, by);
    c.lineTo(x + w / 2, by);
    c.stroke();
    c.lineWidth(10 * kLength);
    c.strokeStyle(Color(0x85e37d));
    c.beginPath();
    c.moveTo(x - w / 2, by);
    c.lineTo(x - w / 2 + w * std::clamp(health, 0.f, 1.f), by);
    c.stroke();
    c.lineCap(cp6::Cap::Butt);
}

}  // namespace game
