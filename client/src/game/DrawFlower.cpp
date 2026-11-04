#include <algorithm>
#include <cmath>
#include <vector>

#include "game/DrawCommon.h"

namespace game {

void drawPea(Context &c, bool poison, bool big, const Look &l) {
    usePaint(c, poison ? 0xce76db : 0x8ac255, l);
    circlePath(c, 0, 0, big ? 10.f : 7.f);
    c.fill();
    c.stroke();
}

void drawWeb(Context &c) {
    const float step = .483322f;  // a thirteenth of a turn
    c.beginPath();
    for (int r = 10; r <= 50; r += 10) {
        c.moveTo(float(r), 0);
        for (int k = 0; k < 13; k++) {
            float x0 = std::cos(k * step) * r, y0 = std::sin(k * step) * r;
            float x1 = std::cos((k + 1) * step) * r, y1 = std::sin((k + 1) * step) * r;
            float sx = x0 + x1, sy = y0 + y1, len = std::sqrt(sx * sx + sy * sy);
            float nx = len > 0 ? sx / len : 1, ny = len > 0 ? sy / len : 0;
            c.quadraticCurveTo(sx * .5f - nx * 5, sy * .5f - ny * 5, x1, y1);  // sags toward the middle
        }
    }
    for (int k = 0; k < 13; k++) {
        c.moveTo(0, 0);
        c.lineTo(std::cos(k * step) * 55, std::sin(k * step) * 55);
    }
    c.lineWidth(3);
    c.lineCap(cp6::Cap::Round);
    c.strokeStyle(Color(0xffffff));
    c.stroke();
}

void drawFlower(Context &c, float r, const Look &l, float lookX, float lookY, float mouth, float angry) {
    c.save();
    c.scale(r / 25.f);
    c.lineCap(cp6::Cap::Round);
    c.lineWidth(3);
    // yellow, lightened and tinted purple by the flower's state
    auto ch = [&](float base, float purple) {
        float k = .3f * l.light;
        float lit = std::trunc(k * 255 + (1 - k) * base);
        return uint32_t(std::trunc(l.purple * purple + (1 - l.purple) * lit));
    };
    uint32_t body = ch(255, 206) << 16 | ch(231, 118) << 8 | ch(99, 219);
    uint32_t fill = tint(body, l.hurt);
    c.fillStyle(Color(fill));
    c.strokeStyle(hurtColor(darker(fill), l.hurt));
    if (l.lumpy) {
        c.beginPath();
        c.moveTo(25, 0);
        c.quadraticCurveTo(15, 35, 0, 25);
        c.quadraticCurveTo(-25, 15, -25, 0);
        c.quadraticCurveTo(-25, -35, 0, -25);
        c.quadraticCurveTo(18, -25, 25, 0);
        c.closePath();
    } else {
        circlePath(c, 0, 0, 25);
    }
    c.fill();
    c.stroke();
    if (l.antennae) {
        c.save();
        c.translate(0, 6);
        c.rotate(-TAU / 4);
        Color k = hurtColor(0x333333, l.hurt);
        c.fillStyle(k);
        c.strokeStyle(k);
        c.lineWidth(3);
        for (int s = -1; s <= 1; s += 2) {
            c.beginPath();
            c.moveTo(25, 5.f * s);
            c.quadraticCurveTo(40, 10.f * s, 50, 15.f * s);
            c.quadraticCurveTo(40, 5.f * s, 25, 5.f * s);
            c.closePath();
            c.fill();
            c.stroke();
        }
        c.restore();
    }
    float ex = lookX * 2, ey = lookY * 4;
    // one eye at the origin of the current transform; side -1 is the left one
    auto eye = [&](int side) {
        if (l.dead) {
            c.strokeStyle(Color(0x222222));
            c.lineWidth(3);
            c.beginPath();
            c.moveTo(-4, -4);
            c.lineTo(4, 4);
            c.stroke();
            c.beginPath();
            c.moveTo(-4, 4);
            c.lineTo(4, -4);
            c.stroke();
            return;
        }
        (void)side;
        c.save();
        c.scale(1, 2);
        circlePath(c, 0, 0, 3);
        c.fillStyle(Color(0x222222));
        c.fill();
        c.restore();
        // the highlight is clipped to the eye
        c.save();
        c.scale(1, 2);
        c.beginPath();
        c.arc(0, 0, 3, 0, TAU);
        c.clip();
        c.scale(1, .5f);
        c.fillStyle(Color(0xeeeeee));
        circlePath(c, ex, ey, 3);
        c.fill();
        c.restore();
        c.save();
        c.scale(1, 2);
        circlePath(c, 0, 0, 3);
        c.strokeStyle(Color(0x222222));
        c.lineWidth(.5f);
        c.stroke();
        c.restore();
    };
    if (l.lumpy) {
        // eyes and mouth askew, without brows
        c.save();
        c.translate(-10, -10);
        c.rotate(3.078788f);
        eye(-1);
        c.restore();
        c.save();
        c.translate(10, 8);
        c.rotate(.094247f);
        eye(1);
        c.restore();
        c.save();
        c.translate(-8, -3);
        c.rotate(.628319f);
    } else {
        // angry: the eyes are drawn below a V whose point comes down to the middle of the face
        bool brows = angry > .0001f;
        if (brows) {
            c.save();
            c.beginPath();
            c.moveTo(-14, -14);
            c.lineTo(0, angry * 9 - 14);
            c.lineTo(14, -14);
            c.lineTo(14, 6);
            c.lineTo(-14, 6);
            c.closePath();
            c.clip();
        }
        for (int s = -1; s <= 1; s += 2) {
            c.save();
            c.translate(7.f * s, -5);
            eye(s);
            c.restore();
        }
        if (brows) c.restore();
        c.save();
    }
    c.beginPath();
    c.moveTo(-6, 10);
    c.quadraticCurveTo(0, 10 + mouth, 6, 10);
    c.strokeStyle(Color(0x222222));
    c.lineWidth(1.5f);
    c.stroke();
    c.restore();
    c.restore();
}

}  // namespace game
