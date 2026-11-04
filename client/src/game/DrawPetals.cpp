#include <cmath>

#include "game/DrawCommon.h"

namespace game {

// Each petal is drawn at its own fixed size; the world radius doesn't scale it.

static void petalCircle(Context &c, float x, float y, float r, uint32_t fill, const Look &l) {
    usePaint(c, fill, l);
    circlePath(c, x, y, r);
    c.fill();
    c.stroke();
}

// spin while out in the world; a fixed pose on cards
static void spin(Context &c, const Look &l, float rate, float uiAngle = 0) {
    if (l.ui) {
        if (uiAngle) c.rotate(uiAngle);
        return;
    }
    c.rotate(float(std::fmod(l.time * rate + (l.id % 97) * 0.37, double(TAU))));
}

// jagged polygon, reseeded from the entity id every time so it never changes
static void jagged(Context &c, int n, float r, const Look &l) {
    ShapeRng rng(l.id);
    c.beginPath();
    for (int i = 0; i < n; i++) {
        float a = i * TAU / n;
        double d1 = rng.next(), d2 = rng.next(), d3 = rng.next(), d4 = rng.next();
        float jx = float(((d2 - 1) * 2147483646.0 + (d1 - 1)) * 4.33681e-20 + .85);
        float jy = float(((d4 - 1) * 2147483646.0 + (d3 - 1)) * 4.33681e-20 + .85);
        float x = jx * r * std::cos(a), y = jy * r * std::sin(a);
        if (!i)
            c.moveTo(x, y);
        else
            c.lineTo(x, y);
    }
    c.closePath();
}

// points on a circle of radius r joined by curves bent through radius rc
static void star(Context &c, int n, float r, float rc) {
    c.beginPath();
    c.moveTo(r, 0);
    for (int i = 0; i < n; i++) {
        float a0 = (i + .5f) * TAU / n, a1 = (i + 1) * TAU / n;
        c.quadraticCurveTo(rc * std::cos(a0), rc * std::sin(a0), r * std::cos(a1), r * std::sin(a1));
    }
    c.closePath();
}

static void peas(Context &c, const Look &l, float off, float r, uint32_t col) {
    spin(c, l, 0.001f, 1);
    for (int sy = 1; sy >= -1; sy -= 2)
        for (int sx = 1; sx >= -1; sx -= 2) petalCircle(c, off * sx, off * sy, r, col, l);
}

static void cactusPetal(Context &c, const Look &l, uint32_t col, uint32_t inner) {
    spin(c, l, 0.0007f);
    usePaint(c, col, l);
    star(c, 8, 15, 11.087f);
    c.fill();
    c.stroke();
    c.fillStyle(hurtColor(inner, l.hurt));
    circlePath(c, 0, 0, 8);
    c.fill();
}

// sparkles trailing the unique Basic: one every 50 ms, flying out at 70 units/s and fading
static void sparkles(Context &c, const Look &l) {
    if (l.ui) return;
    float a0 = c.globalAlpha();
    c.fillStyle(Color(l.gray ? 0xfefefe : 0xffffff));
    double now = l.time;
    long long k0 = (long long)(now / 50);
    for (int k = 0; k < 22; k++) {
        double age = now - (k0 - k) * 50.0;
        float alpha = float(std::pow(0.9, age * 0.06));
        if (alpha < 0.01f) break;
        uint32_t h = uint32_t((k0 - k) * 2654435761u) ^ l.id * 40503u;
        h = h * 1103515245u + 12345u;
        float ang = (h >> 8) * (TAU / 16777216.f), d = float(age) * 0.07f;
        c.globalAlpha(a0 * alpha);
        circlePath(c, std::cos(ang) * d, std::sin(ang) * d, 3);
        c.fill();
    }
    c.globalAlpha(a0);
}

static void rockPetal(Context &c, int type, const Look &l) {
    bool rock = type == P_ROCK;
    Paint p = rock ? paint(0x777777, l) : paint(0xffffff, l);
    c.lineWidth(3);
    c.fillStyle(p.fill);
    c.strokeStyle(p.stroke);
    jagged(c, rock ? 5 : 7, rock ? 14 : 12, l);
    c.fill();
    c.stroke();
}

static void squarePetal(Context &c, const Look &l) {
    spin(c, l, 0.0003f, 1);
    usePaint(c, 0xffe869, l, 2.25f);
    c.beginPath();
    c.rect(-10.607f, -10.607f, 21.213f, 21.213f);
    c.fill();
    c.stroke();
}

static void stingerPetal(Context &c, const Look &l) {
    spin(c, l, 0.0003f);
    usePaint(c, 0x333333, l);
    c.beginPath();
    c.moveTo(7, 0);
    c.lineTo(-3.5f, 6.062f);
    c.lineTo(-3.5f, -6.062f);
    c.closePath();
    c.fill();
    c.stroke();
}

static void wingPetal(Context &c, const Look &l) {
    if (l.ui) c.rotate(1);
    usePaint(c, 0xffffff, l);
    c.beginPath();
    c.arc(0, 0, 15, -PI / 2, PI / 2);
    c.quadraticCurveTo(10, 0, 0, -15);
    c.closePath();
    c.fill();
    c.stroke();
}

static void missilePetal(Context &c, const Look &l) {
    if (l.ui) c.rotate(1);
    Color k = hurtColor(0x333333, l.hurt);
    c.lineWidth(5);
    c.fillStyle(k);
    c.strokeStyle(k);
    c.beginPath();
    c.moveTo(11, 0);
    c.lineTo(-11, -6);
    c.lineTo(-11, 6);
    c.closePath();
    c.fill();
    c.stroke();
}

static void epicRosePetal(Context &c, const Look &l) {
    spin(c, l, 0.0005f);
    usePaint(c, 0xff94c9, l);
    c.beginPath();
    c.moveTo(10, 0);
    c.quadraticCurveTo(10, 17.321f, -5, 8.66f);
    c.quadraticCurveTo(-20, 0, -5, -8.66f);
    c.quadraticCurveTo(10, -17.321f, 10, 0);
    c.closePath();
    c.fill();
    c.stroke();
}

static void bubblePetal(Context &c, const Look &l) {
    float a0 = c.globalAlpha();
    Color w = hurtColor(0xffffff, l.hurt);
    c.globalAlpha(a0 * .7f);
    c.lineWidth(3);
    c.strokeStyle(w);
    circlePath(c, 0, 0, 12);
    c.stroke();
    c.globalAlpha(a0 * .35f);
    c.fillStyle(w);
    circlePath(c, 0, 0, 10.5f);
    c.fill();
    circlePath(c, 4, -4, 3);
    c.fill();
}

static void dandelionPetal(Context &c, const Look &l) {
    if (l.ui) c.rotate(1);
    c.lineCap(cp6::Cap::Round);
    c.lineWidth(7);
    c.strokeStyle(Color(0x333333));
    c.beginPath();
    c.moveTo(0, 0);
    c.lineTo(-16, 0);
    c.stroke();
    c.lineCap(cp6::Cap::Butt);
    petalCircle(c, 0, 0, 9, 0xffffff, l);
}

static void eggPetal(Context &c, int type, const Look &l) {
    float s = type == P_EGG ? 0 : 2;
    usePaint(c, 0xfff0b8, l);
    c.beginPath();
    c.ellipse(0, 0, 11 + s, 15 + s, 0, 0, TAU);
    c.fill();
    c.stroke();
}

// only ever shown on cards
static void antennaePetal(Context &c, const Look &l) {
    c.translate(0, 35);
    c.rotate(-PI / 2);
    Color k = hurtColor(0x333333, l.hurt);
    c.lineWidth(3);
    c.fillStyle(k);
    c.strokeStyle(k);
    for (int s = -1; s <= 1; s += 2) {
        c.beginPath();
        c.moveTo(25, 5.f * s);
        c.quadraticCurveTo(40, 10.f * s, 50, 15.f * s);
        c.quadraticCurveTo(40, 5.f * s, 25, 5.f * s);
        c.closePath();
        c.fill();
        c.stroke();
    }
}

static void heaviestPetal(Context &c, const Look &l) {
    petalCircle(c, 0, 0, 16, 0x333333, l);
    c.fillStyle(Color(0xcccccc));
    circlePath(c, 6, -6, 4.6f);
    c.fill();
}

static void yinYangPetal(Context &c, const Look &l) {
    Color w = hurtColor(0xffffff, l.hurt), k = hurtColor(0x333333, l.hurt);
    Color wo(darker(tint(0xffffff, l.hurt))), ko(darker(tint(0x333333, l.hurt)));
    c.lineWidth(3);
    c.fillStyle(w);
    circlePath(c, 0, 0, 12);
    c.fill();
    c.fillStyle(k);
    c.beginPath();
    c.arc(0, 0, 12, -PI / 2, PI / 2);
    c.fill();
    circlePath(c, 0, 6, 6);
    c.fill();
    c.fillStyle(w);
    circlePath(c, 0, -6, 6);
    c.fill();
    c.strokeStyle(ko);
    c.beginPath();
    c.arc(0, 0, 12, -PI / 2, PI / 2);
    c.stroke();
    c.strokeStyle(wo);
    c.beginPath();
    c.arc(0, 0, 12, PI / 2, PI * 1.5f);
    c.stroke();
    c.strokeStyle(ko);
    c.beginPath();
    c.arc(0, 6, 6, PI / 2, PI * 1.5f);
    c.stroke();
    c.strokeStyle(wo);
    c.beginPath();
    c.arc(0, -6, 6, -PI / 2, PI / 2);
    c.stroke();
}

static void webPetal(Context &c, const Look &l) {
    spin(c, l, 0.003f, 1);
    usePaint(c, 0xffffff, l);
    star(c, 5, 11, 5.34f);
    c.fill();
    c.stroke();
}

static void honeyPetal(Context &c, const Look &l) {
    spin(c, l, 0.001f);
    usePaint(c, 0xf7cf2f, l);
    c.beginPath();
    for (int i = 0; i < 6; i++) {
        float a = i * TAU / 6;
        if (!i)
            c.moveTo(11 * std::cos(a), 11 * std::sin(a));
        else
            c.lineTo(11 * std::cos(a), 11 * std::sin(a));
    }
    c.closePath();
    c.fill();
    c.stroke();
}

static void leafPetal(Context &c, const Look &l) {
    if (l.ui) c.rotate(-1);
    usePaint(c, 0x39b54a, l);
    c.beginPath();
    c.moveTo(-15, 0);
    c.lineTo(-20, 0);
    c.stroke();
    c.beginPath();
    c.moveTo(-15, 0);
    c.bezierCurveTo(-10, -12, 5, -12, 15, 0);
    c.bezierCurveTo(5, 12, -10, 12, -15, 0);
    c.closePath();
    c.fill();
    c.stroke();
    c.beginPath();
    c.moveTo(-9, 0);
    c.quadraticCurveTo(0, -1.5f, 7.5f, 0);
    c.stroke();
}

static void ricePetal(Context &c, const Look &l) {
    spin(c, l, 0.002f, .7f);
    c.lineCap(cp6::Cap::Round);
    for (int pass = 0; pass < 2; pass++) {
        c.lineWidth(pass ? 4 : 10);
        c.strokeStyle(pass ? hurtColor(0xffffff, l.hurt) : Color(darker(tint(0xffffff, l.hurt))));
        c.beginPath();
        c.moveTo(-8, 0);
        c.quadraticCurveTo(0, -3, 8, 0);
        c.stroke();
    }
}

static void cornPetal(Context &c, const Look &l) {
    spin(c, l, 0.002f, .7f);
    usePaint(c, 0xffe419, l);
    c.beginPath();
    c.moveTo(-5, 8);
    c.quadraticCurveTo(-15, -8, 0, -8);
    c.quadraticCurveTo(15, -8, 5, 8);
    c.quadraticCurveTo(0, 2, -5, 8);
    c.closePath();
    c.fill();
    c.stroke();
}

void drawPetal(Context &c, int type, const Look &l) {
    c.save();
    c.lineCap(cp6::Cap::Butt);
    switch (type) {
        case P_FAST:
        case P_TWIN:
        case P_TRIPLET: petalCircle(c, 0, 0, 7, 0xffffff, l); break;
        case P_HEAVY: petalCircle(c, 0, 0, 12, 0xbbbbbb, l); break;
        case P_ROCK:
        case P_SALT: rockPetal(c, type, l); break;
        case P_SQUARE: squarePetal(c, l); break;
        case P_ROSE: petalCircle(c, 0, 0, 10, 0xff94c9, l); break;
        case P_ROSE_TRIO: petalCircle(c, 0, 0, 7, 0xff94c9, l); break;
        case P_STINGER:
        case P_STINGER_TRIO: stingerPetal(c, l); break;
        case P_IRIS: petalCircle(c, 0, 0, 6, 0xce76db, l); break;
        case P_WING: wingPetal(c, l); break;
        case P_MISSILE: missilePetal(c, l); break;
        case P_PEAS: peas(c, l, 6, 7, 0x8ac255); break;
        case P_POISON_PEAS: peas(c, l, 6, 7, 0xce76db); break;
        case P_BIG_PEAS: peas(c, l, 7, 10, 0xce76db); break;
        case P_CACTUS:
        case P_CACTUS_TRIO: cactusPetal(c, l, 0x38c75f, 0x74d68f); break;
        case P_POISON_CACTUS: cactusPetal(c, l, 0xb438c7, 0xc274d6); break;
        case P_FASTER: petalCircle(c, 0, 0, 7, 0xfeffc9, l); break;
        case P_EPIC_ROSE: epicRosePetal(c, l); break;
        case P_BUBBLE: bubblePetal(c, l); break;
        case P_POLLEN: petalCircle(c, 0, 0, 7, 0xffe763, l); break;
        case P_DANDELION: dandelionPetal(c, l); break;
        case P_EGG:
        case P_BEETLE_EGG: eggPetal(c, type, l); break;
        case P_ANTENNAE: antennaePetal(c, l); break;
        case P_HEAVIEST: heaviestPetal(c, l); break;
        case P_YIN_YANG: yinYangPetal(c, l); break;
        case P_WEB:
        case P_WEB_TRIO: webPetal(c, l); break;
        case P_HONEY: honeyPetal(c, l); break;
        case P_LEAF: leafPetal(c, l); break;
        case P_UNIQUE_BASIC:
            sparkles(c, l);
            petalCircle(c, 0, 0, 10, 0xffffff, l);
            break;
        case P_RICE: ricePetal(c, l); break;
        case P_CORN: cornPetal(c, l); break;
        default: petalCircle(c, 0, 0, 10, 0xffffff, l); break;  // Basic
    }
    c.restore();
}

void drawPetalIcon(Context &c, int type, float size, const Look &l) {
    Look ul = l;
    ul.ui = true;
    c.save();
    c.scale(size / 64.17f);
    c.translate(0, -5);
    int n = petalCount(type);
    for (int i = 0; i < n; i++) {
        c.save();
        if (n > 1) {
            float a = i * TAU / n;
            c.translate(10 * std::cos(a), 10 * std::sin(a));
        }
        drawPetal(c, type, ul);
        c.restore();
    }
    c.restore();
}

}  // namespace game
