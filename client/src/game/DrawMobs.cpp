#include <algorithm>
#include <cmath>
#include <vector>

#include "game/DrawCommon.h"

namespace game {

static void rock(Context &c, float r, const Look &l, uint32_t col) {
    ShapeRng rng(l.id);
    int n = int(r * 0.142857f + 3);
    if (n < 3) n = 3;
    c.lineWidth(7);
    Paint p = paint(col, l);
    c.fillStyle(p.fill);
    c.strokeStyle(p.stroke);
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
    c.fill();
    c.stroke();
}

static void ladybug(Context &c, float r, const Look &l, uint32_t body, uint32_t spots) {
    c.save();
    c.scale(r / 30.f);
    c.lineWidth(7);
    Paint head = paint(0x111111, l);
    c.fillStyle(head.fill);
    c.strokeStyle(head.stroke);
    circlePath(c, 15, 0, 15);
    c.fill();
    c.stroke();
    cp6::Path2D shell;
    shell.detail = c.matrix().scale();
    shell.arc(0, 0, 30, .6f, 5.683185f);
    shell.quadraticCurveTo(10, 0, 24.760069f, 16.939274f);
    shell.closePath();
    Paint b = paint(body, l);
    c.fillStyle(b.fill);
    c.strokeStyle(b.stroke);
    c.fill(shell);
    c.save();
    c.clip(shell);
    Paint s = paint(spots, l);
    c.fillStyle(s.fill);
    ShapeRng rng(l.id);
    int n = r > 30 ? 5 : 3;
    for (int i = 0; i < n; i++) {
        float x = float((rng.next() - 1) * 2.32831e-08 - 25), y = float((rng.next() - 1) * 2.32831e-08 - 25);
        float rr = float((rng.next() - 1) * 3.25963e-09 + 3);
        circlePath(c, x, y, rr);
        c.fill();
    }
    c.restore();
    c.strokeStyle(b.stroke);
    c.stroke(shell);
    c.restore();
}

static void beeLike(Context &c, const Look &l, bool hornet) {
    c.save();
    if (hornet) c.scale(1.3f);
    c.lineWidth(5);
    c.lineCap(cp6::Cap::Round);
    Paint dark = paint(0x333333, l);
    if (hornet) dark.stroke = dark.fill;
    c.fillStyle(dark.fill);
    c.strokeStyle(dark.stroke);
    c.beginPath();
    if (hornet) {
        c.moveTo(-47, 0);
        c.lineTo(-25, -6);
        c.lineTo(-25, 6);
    } else {
        c.moveTo(-37, 0);
        c.lineTo(-25, -9);
        c.lineTo(-25, 9);
    }
    c.closePath();
    c.fill();
    c.stroke();
    uint32_t yellow = hornet ? 0xffd363 : 0xffe763;
    c.beginPath();
    c.ellipse(0, 0, 30, 20, 0, 0, TAU);
    c.fillStyle(hurtColor(yellow, l.hurt));
    c.fill();
    c.save();
    c.clip();
    c.fillStyle(hurtColor(0x333333, l.hurt));
    c.fillRect(10, -20, 10, 40);
    c.fillRect(-10, -20, 10, 40);
    c.fillRect(-30, -20, 10, 40);
    c.restore();
    c.beginPath();
    c.ellipse(0, 0, 30, 20, 0, 0, TAU);
    c.strokeStyle(hurtColor(hornet ? 0xd3ad46 : 0xd3bd46, l.hurt));
    c.stroke();
    Color d = hurtColor(0x333333, l.hurt);
    c.strokeStyle(d);
    c.fillStyle(d);
    c.lineWidth(3);
    for (int s = -1; s <= 1; s += 2) {
        if (hornet) {
            c.beginPath();
            c.moveTo(25, 5 * s);
            c.quadraticCurveTo(40, 10 * s, 50, 15 * s);
            c.quadraticCurveTo(40, 5 * s, 25, 5 * s);
            c.closePath();
            c.fill();
            c.stroke();
        } else {
            c.beginPath();
            c.moveTo(25, 5 * s);
            c.quadraticCurveTo(35, 5 * s, 40, 15 * s);
            c.stroke();
            circlePath(c, 40, 15 * s, 5);
            c.fill();
        }
    }
    c.restore();
}

static void centipede(Context &c, const Look &l, bool head, bool evil) {
    c.save();
    c.lineWidth(7);
    c.lineCap(cp6::Cap::Round);
    c.fillStyle(hurtColor(0x333333, l.hurt));
    circlePath(c, 0, -30, 15);
    c.fill();
    circlePath(c, 0, 30, 15);
    c.fill();
    Paint b = paint(evil ? 0x905db0 : 0x8ac255, l);
    c.fillStyle(b.fill);
    c.strokeStyle(b.stroke);
    circlePath(c, 0, 0, 35);
    c.fill();
    c.stroke();
    if (head) {
        Color d = hurtColor(0x333333, l.hurt);
        c.strokeStyle(d);
        c.fillStyle(d);
        c.lineWidth(3);
        for (int s = -1; s <= 1; s += 2) {
            c.beginPath();
            c.moveTo(25, 10 * s);
            c.quadraticCurveTo(45, 10 * s, 55, 30 * s);
            c.stroke();
            circlePath(c, 55, 30 * s, 5);
            c.fill();
        }
    }
    c.restore();
}

// how far legs bend in, wings open and pincers close, 0..1; three times as fast while chasing
static float gait(const Look &l) {
    float slow = float(.5 - .5 * std::cos(l.anim * .01)), fast = float(.5 - .5 * std::cos(l.anim * .03));
    return slow + (fast - slow) * std::clamp(l.chase, 0.f, 1.f);
}

static void beetle(Context &c, float r, const Look &l) {
    c.save();
    c.scale(r / 40.f);
    c.lineWidth(7);
    c.lineCap(cp6::Cap::Round);
    Color d = hurtColor(0x333333, l.hurt);
    float flex = gait(l);
    for (int s = -1; s <= 1; s += 2) {
        c.save();
        c.translate(35, 0);
        c.rotate(.2f * flex * s);
        c.beginPath();
        c.moveTo(-10, -15 * s);
        c.quadraticCurveTo(15, -30 * s, 35, -15 * s);
        c.quadraticCurveTo(15, -20 * s, -10, -5 * s);
        c.closePath();
        c.fillStyle(d);
        c.strokeStyle(d);
        c.fill();
        c.stroke();
        c.restore();
    }
    Paint b = paint(0x905db0, l);
    c.beginPath();
    c.moveTo(0, -30);
    c.quadraticCurveTo(40, -30, 40, 0);
    c.quadraticCurveTo(40, 30, 0, 30);
    c.quadraticCurveTo(-40, 30, -40, 0);
    c.quadraticCurveTo(-40, -30, 0, -30);
    c.closePath();
    c.fillStyle(b.fill);
    c.strokeStyle(b.stroke);
    c.fill();
    c.stroke();
    c.beginPath();
    c.moveTo(-20, 0);
    c.quadraticCurveTo(0, -3, 20, 0);
    c.stroke();
    c.fillStyle(b.stroke);
    const float dots[6][2] = {{-17, -12}, {-17, 12}, {0, -15}, {0, 15}, {17, -12}, {17, 12}};
    for (auto &p : dots) {
        circlePath(c, p[0], p[1], 5);
        c.fill();
    }
    c.restore();
}

static void antHead(Context &c, float x, float s, float flex, const Paint &b, const Look &l) {
    c.save();
    c.translate(x, 0);
    c.strokeStyle(Color(darker(tint(0x333333, l.hurt))));
    for (int d = -1; d <= 1; d += 2) {
        c.beginPath();
        c.moveTo(0, 7 * s * d);
        c.quadraticCurveTo(11 * s, (10 - flex) * s * d, 22 * s, (5 - flex) * s * d);
        c.stroke();
    }
    c.fillStyle(b.fill);
    c.strokeStyle(b.stroke);
    circlePath(c, 0, 0, 14 * s);
    c.fill();
    c.stroke();
    c.restore();
}

static void antWings(Context &c, float flex, const Look &l) {
    float a = c.globalAlpha();
    c.globalAlpha(a * .5f);
    c.fillStyle(hurtColor(0xeeeeee, l.hurt));
    for (int d = -1; d <= 1; d += 2) {
        c.save();
        c.rotate(.314159f * flex * d);
        c.beginPath();
        c.ellipse(-7, 8 * d, 15, 7, -.314159f * d, 0, TAU);
        c.fill();
        c.restore();
    }
    c.globalAlpha(a);
}

static void ant(Context &c, int type, const Look &l) {
    c.save();
    c.lineWidth(7);
    c.lineCap(cp6::Cap::Round);
    Paint b = paint(0x555555, l);
    float flex = gait(l);
    auto segment = [&](float x, float r) {
        c.fillStyle(b.fill);
        c.strokeStyle(b.stroke);
        circlePath(c, x, 0, r);
        c.fill();
        c.stroke();
    };
    if (type == M_BABY_ANT) {
        antHead(c, 0, 1, flex, b, l);
    } else if (type == M_QUEEN_ANT) {
        segment(-25, 30);
        segment(0, 25);
        c.save();
        c.scale(2);
        antWings(c, flex, l);
        c.restore();
        antHead(c, 25, 1.5f, flex, b, l);
    } else {
        segment(-8, 10);
        if (type == M_SOLDIER_ANT) antWings(c, flex, l);
        antHead(c, 8, 1, flex, b, l);
    }
    c.restore();
}

static void spider(Context &c, const Look &l) {
    c.save();
    c.lineWidth(5);
    c.lineCap(cp6::Cap::Round);
    c.strokeStyle(Color(0x333333));
    float ph = float(l.walk * .015);
    for (int e = -1; e <= 1; e += 2)
        for (int f = 0; f < 4; f++) {
            float s = .5f - std::cos(ph + (e + f) * 1.570796f) * .5f;
            float off = f * .6f - .9f + (2 * s - 1) * .2f;
            float a = e == -1 ? -PI - off : off;
            float x = std::sin(a) * 35, y = std::cos(a) * 35;
            c.beginPath();
            c.moveTo(0, 0);
            c.quadraticCurveTo(x * .8f, y * .5f, x, y);
            c.stroke();
        }
    Paint b = paint(0x4f412e, l);
    c.fillStyle(b.fill);
    c.strokeStyle(b.stroke);
    circlePath(c, 0, 0, 15);
    c.fill();
    c.stroke();
    c.restore();
}

static void antHole(Context &c, const Look &l) {
    c.save();
    c.fillStyle(hurtColor(0xb58500, l.hurt));
    circlePath(c, 0, 0, 45);
    c.fill();
    c.fillStyle(hurtColor(0x946d00, l.hurt));
    circlePath(c, 0, 0, 30);
    c.fill();
    c.fillStyle(hurtColor(0x6b4f00, l.hurt));
    circlePath(c, 0, 0, 15);
    c.fill();
    c.restore();
}

// A star through the points of the body and halfway in between, stroked with sharp corners whose
// tips stick out past the body as spikes; then the body, points on a circle joined by curves
// bowed in a little. The corners are built as the stroke's outline, filled.
static void cactus(Context &c, float r, const Look &l) {
    int n = std::max(3, int(double(r) * 0.142857 + 3.0));
    std::vector<float> px(n + 1), py(n + 1);
    for (int d = 0; d <= n; d++) {
        float a = float(double(d) * 6.283185 * (1.0 / n));
        px[d] = r * std::cos(a);
        py[d] = r * std::sin(a);
    }
    std::vector<float> sx(2 * n), sy(2 * n);
    for (int d = 0; d < n; d++) {
        sx[2 * d] = px[d];
        sy[2 * d] = py[d];
        sx[2 * d + 1] = (px[d] + px[d + 1]) * .25f;
        sy[2 * d + 1] = (py[d] + py[d + 1]) * .25f;
    }
    const float hw = 3.5f;
    int m = 2 * n;
    std::vector<float> ox(m), oy(m);
    for (int i = 0; i < m; i++) {
        int a = (i + m - 1) % m, b = (i + 1) % m;
        float ux = sx[i] - sx[a], uy = sy[i] - sy[a], vx = sx[b] - sx[i], vy = sy[b] - sy[i];
        float lu = std::hypot(ux, uy), lv = std::hypot(vx, vy);
        float n1x = -uy / lu, n1y = ux / lu, n2x = -vy / lv, n2y = vx / lv;
        float k = hw / (1 + n1x * n2x + n1y * n2y);
        ox[i] = (n1x + n2x) * k;
        oy[i] = (n1y + n2y) * k;
    }
    c.save();
    c.lineWidth(7);
    c.fillStyle(Color(darker(tint(0x333333, l.hurt))));
    c.beginPath();
    for (int i = 0; i < m; i++) {
        if (!i)
            c.moveTo(sx[i] + ox[i], sy[i] + oy[i]);
        else
            c.lineTo(sx[i] + ox[i], sy[i] + oy[i]);
    }
    c.closePath();
    for (int i = 0; i < m; i++) {
        if (!i)
            c.moveTo(sx[i] - ox[i], sy[i] - oy[i]);
        else
            c.lineTo(sx[i] - ox[i], sy[i] - oy[i]);
    }
    c.closePath();
    c.fill(true);
    uint32_t body = tint(0x32a852, l.hurt);
    c.fillStyle(Color(body));
    c.strokeStyle(Color(darker(body)));
    c.beginPath();
    c.moveTo(px[0], py[0]);
    for (int d = 1; d <= n; d++)
        c.quadraticCurveTo((px[d - 1] + px[d]) * .45f, (py[d - 1] + py[d]) * .45f, px[d], py[d]);
    c.closePath();
    c.fill();
    c.stroke();
    c.restore();
}

static void square(Context &c, const Look &l) {
    c.save();
    c.lineWidth(5.25f);
    Paint b = paint(0xffe869, l);
    c.fillStyle(b.fill);
    c.strokeStyle(b.stroke);
    c.beginPath();
    c.rect(-24.749f, -24.749f, 49.497f, 49.497f);
    c.fill();
    c.stroke();
    c.restore();
}

void drawMob(Context &c, int type, float r, const Look &l) {
    switch (type) {
        case M_ROCK:
        case M_BOULDER: rock(c, r, l, 0x777777); break;
        case M_LADYBUG: ladybug(c, r, l, 0xeb4034, 0x111111); break;
        case M_MASSIVE_LADYBUG: ladybug(c, r, l, 0xeb4034, 0x111111); break;
        case M_DARK_LADYBUG: ladybug(c, r, l, 0x962921, 0xbe342a); break;
        case M_SHINY_LADYBUG: ladybug(c, r, l, 0xebeb34, 0x111111); break;
        case M_BEE: beeLike(c, l, false); break;
        case M_HORNET: beeLike(c, l, true); break;
        case M_CENTIPEDE: centipede(c, l, true, false); break;
        case M_CENTIPEDE_BODY: centipede(c, l, false, false); break;
        case M_EVIL_CENTIPEDE: centipede(c, l, true, true); break;
        case M_EVIL_CENTIPEDE_BODY: centipede(c, l, false, true); break;
        case M_BEETLE:
        case M_MASSIVE_BEETLE: beetle(c, r, l); break;
        case M_BABY_ANT:
        case M_WORKER_ANT:
        case M_SOLDIER_ANT:
        case M_QUEEN_ANT: ant(c, type, l); break;
        case M_SPIDER: spider(c, l); break;
        case M_ANT_HOLE: antHole(c, l); break;
        case M_CACTUS: cactus(c, r, l); break;
        case M_SQUARE: square(c, l); break;
        default: {
            Paint p = paint(0xcccccc, l);
            c.fillStyle(p.fill);
            c.strokeStyle(p.stroke);
            c.lineWidth(5);
            circlePath(c, 0, 0, r);
            c.fill();
            c.stroke();
        }
    }
}

}  // namespace game
