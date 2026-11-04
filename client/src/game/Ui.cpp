#include "game/Ui.h"

#include <algorithm>
#include <cmath>

#include "game/Draw.h"

namespace game {

using cp6::Align;
using cp6::Baseline;
using cp6::Color;
using cp6::Context;

static constexpr float TAU = 6.28318531f;

void outlinedText(Context &c, const std::string &s, float x, float y, float size, Color fill, Align a) {
    c.font(size);
    c.textAlign(a);
    c.textBaseline(Baseline::Middle);
    c.lineWidth(size * .12f);
    c.strokeStyle(Color(0x000000));
    c.strokeText(s, x, y);
    c.fillStyle(fill);
    c.fillText(s, x, y);
}

// reload: the card darkens, and the dark part turns while it shrinks away
static void reloadWedge(Context &c, float size, uint32_t col, float progress) {
    float th = progress * progress * TAU;
    if (th >= TAU - 1e-3f) return;
    auto ch = [](uint32_t v) {
        float f = std::sqrt(v / 255.f) * .8f;
        return uint32_t(std::lround(f * f * 255.f));
    };
    uint32_t dark = ch(col >> 16 & 255) << 16 | ch(col >> 8 & 255) << 8 | ch(col & 255);
    float s5 = size / 64.17f * 5;
    c.save();
    c.beginPath();
    c.moveTo(0, -s5);
    c.arc(0, -s5, size * 2, th * 5.7f, th * 4.7f + TAU);
    c.closePath();
    c.clip();
    c.fillStyle(Color(dark));
    c.fillRect(-size * .5f, -size * .5f, size, size);
    c.restore();
}

// a slot's card: rarity background, the reload wedge, a darker frame, the petal and its name
void card(Context &c, float x, float y, float size, int type, double now, float alpha, float reload, float wedgeAlpha) {
    uint32_t col = rarityColor(petalRarity(type));
    c.save();
    c.translate(x, y);
    c.globalAlpha(alpha);
    c.fillStyle(Color(col));
    c.fillRect(-size * .5f, -size * .5f, size, size);
    if (reload < 1 && wedgeAlpha > 1e-5f) {
        c.globalAlpha(alpha * wedgeAlpha);
        reloadWedge(c, size, col, reload);
        c.globalAlpha(alpha);
    }
    c.lineWidth(size * .0909f);
    c.strokeStyle(Color(darker(col)));
    c.strokeRect(-size * .5f, -size * .5f, size, size);
    Look l;
    l.time = now;
    drawPetalIcon(c, type, size, l);
    outlinedText(c, petalName(type), 0, size * (17 / 64.17f), petalLabelSize(type) * size / 64.17f);
    c.restore();
}

void roundRect(Context &c, float x, float y, float w, float h, float r) {
    c.beginPath();
    c.moveTo(x + r, y);
    c.lineTo(x + w - r, y);
    c.quadraticCurveTo(x + w, y, x + w, y + r);
    c.lineTo(x + w, y + h - r);
    c.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
    c.lineTo(x + r, y + h);
    c.quadraticCurveTo(x, y + h, x, y + h - r);
    c.lineTo(x, y + r);
    c.quadraticCurveTo(x, y, x + r, y);
    c.closePath();
}

BoxArea textPanel(Context &c, const BoxLine *lines, int count, float x, float y, BoxPlace place, float extraW,
                  float alpha, float Wu, float Hu) {
    float w = 0, h = 0;
    for (int i = 0; i < count; i++) {
        c.font(lines[i].size);
        float lw = (lines[i].text.empty() ? 0 : c.measureText(lines[i].text)) + 2 * int(lines[i].size * .24f);
        w = std::max(w, lw);
        h += lines[i].size * 1.1f;
    }
    w += extraW;
    if (place == BoxPlace::Middle)
        y += h * .5f;
    else if (place == BoxPlace::Above)
        y -= h;
    if (x - w * .5f < 15) x = w * .5f + 15;
    if (x + w * .5f > Wu - 15) x = Wu - 15 - w * .5f;
    x -= w * .5f;
    y = std::max(y, 15.f);
    if (y + h > Hu - 15) y = Hu - 15 - h;
    c.globalAlpha(alpha * .5f);
    c.fillStyle(Color(0x000000));
    roundRect(c, x - 5, y - 5, w + 10, h + 10, 5);
    c.fill();
    c.globalAlpha(alpha);
    float ty = y - 2;
    for (int i = 0; i < count; i++) {
        const BoxLine &l = lines[i];
        if (!l.text.empty())
            outlinedText(c, l.text, x + l.dx + int(l.size * .24f), ty + l.size * .7f, l.size, Color(l.color),
                         Align::Left);
        ty += l.size * 1.1f;
    }
    return {x, y, w, h};
}

void petalTooltip(Context &c, int type, float cx, float top, float alpha, float Wu, float Hu) {
    BoxLine lines[5] = {{petalName(type), 24, 0xffffff, -2},
                        {rarityName(petalRarity(type)), 14, rarityColor(petalRarity(type))},
                        {"", 16},
                        {petalDesc(type), 14}};
    int n = 4;
    if (*petalDesc2(type)) lines[n++] = {petalDesc2(type), 14};
    textPanel(c, lines, n, cx, top - 12.5f, BoxPlace::Above, 0, alpha, Wu, Hu);
    c.globalAlpha(1);
}

}  // namespace game
