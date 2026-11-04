#include "game/Inventory.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "game/Ease.h"
#include "game/Game.h"
#include "game/Ui.h"

using namespace protocol;

namespace game {

using cp6::Color;
using cp6::Context;
using sim::Entity;

static constexpr float SLOT = 64.17f, SLOT2 = 45.83f;

Inventory::Inventory(Game &game)
    : game_(game), sim_(game.simulation), ctx_(game.ctx), conn_(game.connection), in(game.in) {}

int Inventory::primarySlots(int level) {
    return level < 15 ? 5 : level < 30 ? 6 : level < 45 ? 7 : 8;
}

void Inventory::notePickup(int slot, float x, float y, double now) {
    Pickup &p = pickup_[slot];
    p.on = true;
    p.at = now;
    p.x = x;
    p.y = y;
}

static void emptySlot(Context &c, float x, float y, float size, uint32_t fill, uint32_t frame, float alpha) {
    float lw = size * .0909f;
    c.globalAlpha(alpha * .8f);
    c.fillStyle(Color(fill));
    c.fillRect(x - size * .5f + lw * .5f, y - size * .5f + lw * .5f, size - lw, size - lw);
    c.globalAlpha(alpha);
    c.lineWidth(lw);
    c.strokeStyle(Color(frame));
    c.strokeRect(x - size * .5f, y - size * .5f, size, size);
}

void Inventory::onKey(const char *code) {
    int n = primaryCount();
    bool digit = !std::strncmp(code, "Digit", 5) && code[5] >= '0' && code[5] <= '9' && !code[6];
    bool next = !std::strcmp(code, "KeyE"), prev = !std::strcmp(code, "KeyQ");
    if (digit || next || prev) {
        selKeyAt_ = game_.time;
        if (!selOn_) {
            // the first key picks the first card in the second row
            for (int s = n; s < n + 8; s++)
                if (slotType(s)) {
                    selOn_ = true;
                    selSlot_ = s;
                    break;
                }
        } else if (next || prev) {
            for (int k = 1; k <= 8; k++) {
                int s = n + ((selSlot_ - n + (next ? k : -k)) % 8 + 8) % 8;
                if (slotType(s)) {
                    selSlot_ = s;
                    break;
                }
            }
        }
        if (digit && selOn_) {
            int i = code[5] == '0' ? 9 : code[5] - '1';
            if (i < n) sendCard(selSlot_, i);
        }
    } else if (!std::strcmp(code, "KeyT")) {
        if (!selOn_) return;
        selKeyAt_ = game_.time;
        sendCard(selSlot_, 16);
        selSlot_++;
    } else if (!std::strcmp(code, "Escape")) {
        selOn_ = false;
    } else if (!std::strcmp(code, "KeyX")) {
        // swap the whole loadout with the row below
        for (int i = 0; i < n; i++) sendCard(i, n + i);
    }
}

void Inventory::layout(float Wu, float Hu, int level, float *px, float *py, float *sx, float *sy, int &n) const {
    n = primarySlots(level);
    if (game_.touchMode()) {  // bottom left, out of the way of the buttons
        float ia = game_.ui.hudH();
        for (int i = 0; i < 8; i++) {
            px[i] = 45 + 90.f * i;
            py[i] = ia - 110;
        }
        for (int i = 0; i < 9; i++) {
            sx[i] = 35 + 65.f * i;
            sy[i] = ia - 35;
        }
        return;
    }
    for (int i = 0; i < 8; i++) {
        px[i] = Wu * .5f + (i - (n - 1) * .5f) * 90.f;
        py[i] = Hu - 144.6f;
    }
    for (int i = 0; i < 9; i++) {
        sx[i] = Wu * .5f + (i - 4) * 65.f;
        sy[i] = Hu - 69.6f;
    }
}

// each card has its own description, fading in while the card is hovered and the mouse button is
// up; it sits 10 units over the card and follows it
void Inventory::drawTooltips(Context &c, double now) {
    float u = game_.ui.hudScale(), Wu = c.width() / u, Hu = c.height() / u;
    int over = drag_.active || in.left || game_.screen != Screen::Playing ? -1 : cardAt(in.mx / u, in.my / u);
    float dt = frameTime(tipTime_, now);
    float k7 = ease(.7f, dt);
    c.setTransform(u, 0, 0, u, 0, 0);
    for (int i = 0; i < 16; i++) {
        Card &k = cards_[i];
        k.tipK += ((i == over && k.type ? 1.f : 0.f) - k.tipK) * k7;
        if (k.tipK < .01f || !k.type) continue;
        // card sizes here are the inner square; the game's own sizes include the frame (x 70/64.17)
        petalTooltip(c, k.type, k.x, k.y - k.size * (70 / SLOT) * .5f + 2.5f, k.tipK, Wu, Hu);
    }
    c.globalAlpha(1);
}

int Inventory::primaryCount() const {
    const Entity *me = sim_.get(game_.player);
    return primarySlots(me ? std::max(1, me->level()) : game_.lastLife.level);
}

int Inventory::slotType(int slot) const {
    const Entity *me = sim_.get(game_.player);
    if (!me || !me->flower || slot < 0 || slot >= int(me->flower->slots.size())) return 0;
    return me->flower->slots[slot];
}

int Inventory::slotAt(float x, float y) const {
    const Entity *me = sim_.get(game_.player);
    if (!me || !me->flower) return -1;
    float u = game_.ui.hudScale(), Wu = ctx_.width() / u, Hu = ctx_.height() / u;
    x /= u;
    y /= u;
    float px[8], py[8], sx[9], sy[9];
    int n;
    layout(Wu, Hu, std::max(1, me->level()), px, py, sx, sy, n);
    for (int i = 0; i < n; i++)
        if (std::fabs(x - px[i]) <= SLOT * .5f && std::fabs(y - py[i]) <= SLOT * .5f) return i;
    for (int i = 0; i < 9; i++)
        if (std::fabs(x - sx[i]) <= SLOT2 * .5f && std::fabs(y - sy[i]) <= SLOT2 * .5f) return i == 8 ? 16 : n + i;
    return -1;
}

bool Inventory::slotPlace(int slot, float &x, float &y, float &size) const {
    const Entity *me = sim_.get(game_.player);
    if (!me || !me->flower) return false;
    float u = game_.ui.hudScale(), Wu = ctx_.width() / u, Hu = ctx_.height() / u;
    float px[8], py[8], sx[9], sy[9];
    int n;
    layout(Wu, Hu, std::max(1, me->level()), px, py, sx, sy, n);
    if (slot < n) {
        x = px[slot];
        y = py[slot];
        size = SLOT;
    } else if (slot < n + 8) {
        x = sx[slot - n];
        y = sy[slot - n];
        size = SLOT2;
    } else {
        x = sx[8];
        y = sy[8];
        size = SLOT2;
    }
    return true;
}

// the card heads for its new slot (and the one there for the old) while the server is asked;
// after two seconds without an answer they go back
void Inventory::sendCard(int from, int to) {
    if (from < 0 || from >= 16 || to == from) return;
    int t = slotType(from);
    net::Writer w;
    if (to == 16) {
        if (!t) return;
        w.u8(uint8_t(from));
        w.u8(uint8_t(t));
        conn_.send(MSG_TRASH, w.b);
    } else {
        int tt = slotType(to);
        if (!t && !tt) return;
        w.u8(uint8_t(from));
        w.u8(uint8_t(t));
        w.u8(uint8_t(to));
        w.u8(uint8_t(tt));
        conn_.send(MSG_SWAP, w.b);
        Card &o = cards_[to];
        if (o.type) {
            o.target = from;
            o.until = game_.time + 2000;
            if (o.overK < .001f) slotPlace(to, o.ox, o.oy, o.os);
        }
    }
    Card &k = cards_[from];
    if (!k.type) return;
    k.target = to;
    k.until = game_.time + 2000;
    if (k.overK < .001f) slotPlace(to, k.ox, k.oy, k.os);
}

// a slot waiting on the server takes no other card
bool Inventory::slotBusy(int slot) const {
    for (int i = 0; i < 16; i++) {
        const Card &k = cards_[i];
        if (k.type && k.target >= 0 && game_.time < k.until && (i == slot || k.target == slot)) return true;
    }
    return false;
}

int Inventory::cardAt(float x, float y) const {
    for (int i = 15; i >= 0; i--) {
        const Card &k = cards_[i];
        if (!k.type || k.alpha < .5f || (drag_.active && drag_.from == i)) continue;
        if (std::fabs(x - k.x) <= k.size * .5f && std::fabs(y - k.y) <= k.size * .5f) return i;
    }
    return -1;
}

void Inventory::updateCards(double now, float dt, const float *px, const float *py, const float *sx, const float *sy,
                            int n) {
    auto place = [&](int i, float &x, float &y, float &size) {
        if (i < n) {
            x = px[i];
            y = py[i];
            size = SLOT;
        } else if (i < n + 8) {
            x = sx[i - n];
            y = sy[i - n];
            size = SLOT2;
        } else {
            x = sx[8];
            y = sy[8];
            size = SLOT2;
        }
    };
    if (cardsOwner_ != game_.player) {
        for (Card &k : cards_) k = Card();
        leaving_.clear();
        cardsOwner_ = game_.player;
        cardsPrimary_ = n;
    }
    if (cardsPrimary_ != n) {  // a new slot opened: the second row moves along in the slot list
        Card moved[16];
        for (int i = 0; i < std::min(n, cardsPrimary_); i++) moved[i] = cards_[i];
        for (int i = 0; i < 8 && cardsPrimary_ + i < 16 && n + i < 16; i++) moved[n + i] = cards_[cardsPrimary_ + i];
        for (Card &k : moved) k.target = -1;
        std::copy(moved, moved + 16, cards_);
        cardsPrimary_ = n;
    }
    // swaps and moves the server has made: the cards trade places
    for (int i = 0; i < 16; i++) {
        int j = cards_[i].target;
        if (!cards_[i].type || j < 0 || j >= 16 || now > cards_[i].until) continue;
        const Card &b = cards_[j];
        if (slotType(j) == cards_[i].type && slotType(i) == b.type && (!b.type || b.target == i)) {
            std::swap(cards_[i], cards_[j]);
            cards_[i].target = cards_[j].target = -1;
        }
    }
    // anything else that changed went away or is new
    for (int i = 0; i < 16; i++) {
        int t = slotType(i);
        Card &k = cards_[i];
        if (k.type == t) continue;
        if (k.type) leaving_.push_back(k);
        k = Card();
        k.type = t;
        k.born = now;
        float size;
        if (t) place(i, k.x, k.y, size);
    }
    // a card picked up from the ground starts where it was collected
    for (int i = 0; i < 16; i++) {
        Pickup &p = pickup_[i];
        if (!p.on) continue;
        Card &k = cards_[i];
        if (k.type && now - k.born < 250) {
            k.x = p.x;
            k.y = p.y;
            k.size = 0;
            p.on = false;
        } else if (now - p.at > 1000)
            p.on = false;
    }

    float k8 = ease(.8f, dt);
    float k6 = ease(.6f, dt);
    float ke = std::min(1.f, 1 - std::exp(-dt * .06f));
    float u = game_.ui.hudScale(), mx = in.mx / u, my = in.my / u;
    const Entity *me = sim_.get(game_.player);
    static const std::vector<uint32_t> none;
    const std::vector<uint32_t> &cds = me && me->flower ? me->flower->cooldowns : none;
    dragOver_ = -1;
    for (int i = 0; i < 16; i++) {
        Card &k = cards_[i];
        if (!k.type) continue;
        bool held = drag_.active && drag_.from == i;
        if (held) {
            k.px = mx;
            k.py = my;
            // the nearest slot within a slot's width that could take it
            int g = -1;
            float best = 0;
            for (int d = 0; d < 17; d++) {
                if (d >= n + 8 && d != 16) continue;
                if (d != 16 && (d == i || slotType(d) == k.type || slotBusy(d))) continue;
                float x, y, size;
                place(d, x, y, size);
                float d2 = (x - mx) * (x - mx) + (y - my) * (y - my);
                if (d2 >= size * size || (g >= 0 && d2 >= best)) continue;
                g = d;
                best = d2;
            }
            dragOver_ = g;
            if (g < 0)
                k.overK -= k.overK * k6;
            else {
                float x, y, size;
                place(g, x, y, size);
                if (k.overK < .001f) {
                    k.ox = x;
                    k.oy = y;
                    k.os = size;
                } else {
                    k.ox += (x - k.ox) * k6;
                    k.oy += (y - k.oy) * k6;
                    k.os += (size - k.os) * k6;
                }
                k.overK += (1 - k.overK) * k6;
            }
        } else if (k.target >= 0 && now < k.until) {
            float x, y, size;
            place(k.target, x, y, size);
            k.ox += (x - k.ox) * k6;
            k.oy += (y - k.oy) * k6;
            k.os += (size - k.os) * k6;
            k.overK += (1 - k.overK) * k6;
        } else {
            k.target = -1;
            k.overK -= k.overK * ke;
        }
        k.dragK += ((held ? 1.f : 0.f) - k.dragK) * k8;
        // reload: the server's value for a card in the first row; a card on its way into the first
        // row starts empty, one on its way out is full
        bool pending = k.target >= 0 && now < k.until;
        bool primary = (pending ? k.target : i) < n;
        float want = pending                          ? (primary ? 0.f : 1.f)
                     : primary && i < int(cds.size()) ? std::min(1.f, float(cds[i]) / 65535.f)
                                                      : 1.f;
        float from = std::min(want, k.reload);
        k.reload = from + (want - from) * k8;
        k.wedgeOff += ((primary ? 0.f : 1.f) - k.wedgeOff) * ke;
        float hx, hy, base;
        place(i, hx, hy, base);
        float d = std::clamp(k.dragK, 0.f, 1.f), o = std::clamp(k.overK, 0.f, 1.f),
              sel = std::clamp(selWiggle_[i], 0.f, 1.f);
        float bx = hx + (k.px - hx) * d, by = hy + (k.py - hy) * d;
        float tx = bx + (k.ox - bx) * o, ty = by + (k.oy - by) * o;
        float s0 = base + base * .2f * d;
        float s1 = s0 + (base * 1.3f - s0) * sel;
        float ts = s1 + (k.os - s1) * o;
        k.x += (tx - k.x) * k8;
        k.y += (ty - k.y) * k8;
        k.size += (ts - k.size) * k8;
        k.alpha += (1 - k.alpha) * k8;
    }
    for (Card &k : leaving_) {
        k.size -= k.size * k8;
        k.alpha -= k.alpha * k8;
    }
    leaving_.erase(std::remove_if(leaving_.begin(), leaving_.end(), [](const Card &k) { return k.alpha < .01f; }),
                   leaving_.end());
}

void Inventory::update(double now) {
    if (selOn_ && (game_.screen != Screen::Playing || now - selKeyAt_ > 5000)) selOn_ = false;
    if (selOn_) {
        // when the picked card leaves its slot, the next card along the row is picked instead
        int n = primaryCount();
        if (selSlot_ < n || selSlot_ >= n + 8 || !slotType(selSlot_)) {
            int from = std::max(n, selSlot_);
            selOn_ = false;
            for (int k = 0; k < 8; k++) {
                int s = n + (from - n + k) % 8;
                if (slotType(s)) {
                    selSlot_ = s;
                    selOn_ = true;
                    break;
                }
            }
        }
    }
    {
        // the strip opens when the mouse reaches the bottom 40 units, and stays open while the
        // mouse is over the inventory or carrying a card
        float u = game_.ui.hudScale(), Wu = ctx_.width() / u, Hu = ctx_.height() / u, x = in.mx / u, y = in.my / u;
        float wide = std::max(635.f, primaryCount() * 90.f + 50);
        bool was = stripOpen_;
        if (game_.screen != Screen::Playing || game_.settings.keyboardControl || game_.touchMode())
            stripOpen_ = false;
        else if (stripOpen_)
            stripOpen_ = drag_.active || (std::fabs(x - Wu * .5f) <= wide * .5f && y >= Hu - 200);
        else
            stripOpen_ = std::fabs(x - Wu * .5f) <= 160 && y >= Hu - 40;
        if (stripOpen_ != was) game_.forceInput = true;
    }
}

void Inventory::draw(Context &c, double now, int level, float Wu, float Hu) {
    bool touch = game_.touchMode();
    float px[8], py[8], sx[9], sy[9];
    int n;
    layout(Wu, Hu, level, px, py, sx, sy, n);
    float fdt = frameTime(flyLast_, now, 100);
    float k7 = ease(.7f, fdt);
    // the movement strip, behind the cards so it can grow over them
    if (!game_.settings.keyboardControl && !touch) {
        stripK_ += ((stripOpen_ ? 1.f : 0.f) - stripK_) * k7;
        float wide = std::max(635.f, n * 90.f + 50);
        float w = 300 + (wide - 300) * stripK_, T = Hu - 34.6f - 165.4f * stripK_;
        float L = Wu * .5f - w * .5f, R = L + w;
        c.globalAlpha(.5f);
        c.fillStyle(Color(0x000000));
        c.beginPath();
        c.moveTo(L + 10, T);
        c.lineTo(R - 10, T);
        c.quadraticCurveTo(R, T, R, T + 10);
        c.lineTo(R, Hu + 1);
        c.lineTo(L, Hu + 1);
        c.lineTo(L, T + 10);
        c.quadraticCurveTo(L, T, L + 10, T);
        c.closePath();
        c.fill();
        if (stripK_ < .999f) {
            c.globalAlpha((1 - stripK_));
            outlinedText(c, "Move mouse here to disable movement", Wu * .5f, Hu - 15.7f, 14);
        }
        if (stripK_ > .001f) {
            c.globalAlpha(stripK_);
            outlinedText(c, "You can also use [Q] and [E] to modify the inventory", Wu * .5f, Hu - 15.7f, 14);
        }
        c.globalAlpha(1);
    }
    // the card picked with the keyboard grows and wobbles
    selAlpha_ += ((selOn_ ? 1.f : 0.f) - selAlpha_) * ease(.6f, fdt);
    trashAlpha_ += ((selOn_ || drag_.active ? 1.f : 0.f) - trashAlpha_) * k7;
    for (int i = 0; i < 16; i++) selWiggle_[i] += ((selOn_ && selSlot_ == i ? 1.f : 0.f) - selWiggle_[i]) * k7;
    updateCards(now, fdt, px, py, sx, sy, n);
    // empty slots show wherever no card sits
    auto covered = [&](float x, float y, float size) {
        for (const Card &k : cards_)
            if (k.type && k.alpha > .99f && std::fabs(k.x - x) < .5f && std::fabs(k.y - y) < .5f &&
                std::fabs(k.size - size) < .5f)
                return true;
        return false;
    };
    for (int i = 0; i < n; i++)
        if (!covered(px[i], py[i], SLOT)) emptySlot(c, px[i], py[i], SLOT, 0xffffff, 0xcfcfcf, 1);
    for (int i = 0; i < 8; i++)
        if (!covered(sx[i], sy[i], SLOT2)) emptySlot(c, sx[i], sy[i], SLOT2, 0xffffff, 0xcfcfcf, 1);
    emptySlot(c, sx[8], sy[8], SLOT2, 0xff7777, 0xcf6060, 1);
    if (trashAlpha_ > .001f) {
        c.globalAlpha(trashAlpha_);
        outlinedText(c, "Delete", sx[8], sy[8], 11);
    }
    // a held card wobbles until it settles over a slot
    float wobble = std::sin(float(now) * .01f) * .1f;
    auto drawCard = [&](const Card &k, float turn) {
        if (k.size < .01f || k.alpha < .001f) return;
        float a = std::min(1.f, k.alpha), wa = 1 - std::clamp(k.wedgeOff, 0.f, 1.f);
        if (std::fabs(turn) < 1e-4f) {
            card(c, k.x, k.y, k.size, k.type, now, a, k.reload, wa);
            return;
        }
        c.save();
        c.translate(k.x, k.y);
        c.rotate(turn);
        card(c, 0, 0, k.size, k.type, now, a, k.reload, wa);
        c.restore();
    };
    for (const Card &k : leaving_) drawCard(k, 0);
    int held = drag_.active ? drag_.from : -1;
    for (int pass = 0; pass < 2; pass++)  // the held card goes on top
        for (int i = 0; i < 16; i++) {
            const Card &k = cards_[i];
            if (!k.type || (i == held) != (pass == 1)) continue;
            drawCard(k, std::max(selWiggle_[i], k.dragK * (1 - k.overK)) * wobble);
        }
    // which key does what, while picking with the keyboard
    if (selAlpha_ > .001f) {
        c.globalAlpha(selAlpha_);
        for (int i = 0; i < n; i++)
            outlinedText(c, "[" + std::to_string(i + 1) + "]", px[i], py[i] - SLOT * .5f - 15, 16);
        if (level < 15)
            outlinedText(c, "[T]", sx[8], sy[8] - SLOT2 * .5f - 15, 16);
        else
            outlinedText(c, "[T]", sx[8] + SLOT2 * .5f + 15, sy[8] - SLOT2 * .5f + 8, 16);
    }
    c.globalAlpha(1);
}

bool Inventory::onButton(bool down) {
    if (down) {
        int s = slotAt(in.mx, in.my);
        if (s < 0) return false;
        int t = s < 16 ? slotType(s) : 0;
        if (t) {
            drag_.active = true;
            drag_.from = s;
            drag_.type = t;
            dragOver_ = -1;
        }
        return true;
    }
    if (!drag_.active) return false;
    drag_.active = false;
    // it lands in the slot it was held over; anywhere else it goes back
    if (dragOver_ >= 0) sendCard(drag_.from, dragOver_);
    dragOver_ = -1;
    return true;
}

}  // namespace game
