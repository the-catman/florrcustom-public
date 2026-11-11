// The HUD of a tank, the way diep shows it: the score and level bars with the name over them,
// the stat panel, the upgrade buttons, the class tree, notifications and the death screen. It is
// laid out in pixels, at a scale of max(W/1920, H/1080).
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "TankData.h"
#include "TankRules.h"
#include "game/DrawTank.h"
#include "game/Game.h"
#include "game/GameUI.h"

using namespace protocol;
using namespace tankdata;

namespace game {

using cp6::Align;
using cp6::Color;
using cp6::Context;
using sim::Entity;

static constexpr float kPi = 3.14159265f;
static constexpr uint32_t kStatColors[8] = {0x43fff9, 0x82ff43, 0xff4343, 0xffde43,
                                            0x437fff, 0x8543ff, 0xf943ff, 0xfcad76};
enum { HIT_NONE, HIT_STAT, HIT_UPGRADE, HIT_IGNORE };

static float approach(float v, float to, float dt, float rate) {
    return v + (to - v) * (1 - std::pow(1 - rate, dt / 16.6667f));
}

static uint32_t blend(uint32_t rgb, float mul, float add) {
    auto ch = [&](int s) { return uint32_t(std::clamp(float(rgb >> s & 255) * mul + add, 0.f, 255.f)) << s; };
    return ch(16) | ch(8) | ch(0);
}

static uint32_t pastel(uint32_t rgb) {
    return blend(rgb, .6f, 102);
}

static std::string commas(long long v) {
    std::string s = std::to_string(v);
    for (int i = int(s.size()) - 3; i > (v < 0 ? 1 : 0); i -= 3) s.insert(size_t(i), ",");
    return s;
}

static std::string timeAlive(double seconds) {
    long long t = (long long)seconds, h = t / 3600, m = t / 60 % 60, s = t % 60;
    std::string out;
    if (h) out += std::to_string(h) + "h ";
    if (h || m) out += std::to_string(m) + "m ";
    return out + std::to_string(s) + 's';
}

// a line of text with its outline, as diep sizes it: the box is the text plus twice a fifth of
// its size each side, and 1.4 times its size tall
struct TextBox {
    float w, h, margin;
};

static TextBox measure(Context &c, const std::string &s, float size) {
    c.font(size);
    float margin = float(int(.4f * size));
    return {std::max(1.f, float(int(c.measureText(s) + 2 * margin))), std::max(1.f, float(int(size * 1.4f))), margin};
}

static void text(Context &c, const std::string &s, float x, float y, float size, Align a = Align::Center,
                 bool outline = true) {
    if (s.empty() || size <= 0) return;
    c.font(size);
    c.textAlign(a);
    c.textBaseline(cp6::Baseline::Middle);
    if (outline) {
        c.lineWidth(size * .2f);
        c.strokeStyle(Color(0x000000));
        c.strokeText(s, x, y);
    }
    c.fillStyle(Color(0xffffff));
    c.fillText(s, x, y);
}

// a round-ended bar on black, filled perc of the way
static void bar(Context &c, float perc, float cx, float cy, float w, float h, uint32_t fill) {
    float inner = std::max(w - h, 1.f), x = cx - inner / 2, y = cy + .5f;
    c.lineCap(cp6::Cap::Round);
    c.lineWidth(h);
    c.strokeStyle(Color(0x000000));
    c.beginPath();
    c.moveTo(x + .5f, y);
    c.lineTo(inner + x + .5f, y);
    c.stroke();
    c.lineWidth(h * .75f);
    c.strokeStyle(Color(fill));
    c.beginPath();
    c.moveTo(x + .5f, y);
    c.lineTo(x + std::max(inner * std::clamp(perc, 0.f, 1.f), 1.f) + .5f, y);
    c.stroke();
    c.lineCap(cp6::Cap::Butt);
}

float GameUI::tankUnit() const {
    return std::max(ctx_.width() / 1920.f, ctx_.height() / 1080.f);
}

void GameUI::notify(const std::string &s, uint32_t color, float ms, const std::string &id) {
    double now = game_.time;
    if (!id.empty())
        for (Note &n : notes_)
            if (n.id == id && !(n.expiry < now + 300)) n.expiry = now + 300;
    notes_.push_back({s, id, color, now, ms > 0 ? now + ms : 0});
}

const sim::DriverData *GameUI::driver() const {
    const Entity *cam = sim_.get(sim_.cameraId);
    return cam && cam->driver ? &*cam->driver : nullptr;
}

bool GameUI::hasTank() const {
    const Entity *t = sim_.get(sim_.camera() ? sim_.camera()->player : 0);
    return t && t->tank && t->deadAt < 0 && game_.screen == Screen::Playing;
}

// a point into a stat: shown at once, and sent with the level it should get to
void GameUI::applyStat(int stat, int target) {
    Entity *cam = sim_.get(sim_.cameraId);
    if (stat < 0 || stat >= 8 || !cam || !cam->driver || !hasTank()) return;
    sim::DriverData &d = *cam->driver;
    const TankDef *def = tank(d.tank);
    if (!def || d.points < 1) return;
    int level = d.stats[stat];
    if (level >= def->stats[stat].max || (target != -1 && level >= target)) return;
    d.stats[stat] = level + 1;
    d.points--;
    net::Writer w;
    w.u8(uint8_t(stat));
    w.u8(uint8_t(target == -1 ? level + 1 : target));
    game_.connection.send(MSG_STAT, w.b);
}

// U held adds the stat to the build to spend into as points come; M fills it up to its max
void GameUI::queueStat(int stat, bool toMax) {
    const sim::DriverData *d = driver();
    const TankDef *def = d ? tank(d->tank) : nullptr;
    if (!def || stat < 0 || stat >= 8) return;
    int limit = def->stats[stat].max;
    if (limit <= 0) return;
    int counts[8] = {};
    for (char q : statBuild_) {
        int s = '8' - q;
        if (s >= 0 && s < 8) counts[s]++;
    }
    int committed = 0;
    for (int s = 0; s < 8; s++) committed += std::max(d->stats[s], counts[s]);
    int level = d->stats[stat], count = counts[stat];
    do {
        if (committed > 32 || std::max(count, level) >= limit) return;
        for (; count < level; count++) statBuild_ += char('8' - stat);
        statBuild_ += char('8' - stat);
        count++;
        committed++;
        queuedSinceU_ = true;
    } while (toMax);
}

void GameUI::spendStat(int stat) {
    if (!hasTank()) return;
    if (in.keyM)
        queueStat(stat, true);
    else if (in.keyU)
        queueStat(stat, false);
    else
        applyStat(stat, -1);
}

const GameUI::Hit *GameUI::hitAt(float x, float y) const {
    for (auto it = hits_.rbegin(); it != hits_.rend(); ++it)
        if (x >= it->x && y >= it->y && x < it->x + it->w && y < it->y + it->h) return &*it;
    return nullptr;
}

bool GameUI::overTankHud(float x, float y) const {
    return hitAt(x, y) != nullptr;
}

bool GameUI::pressed(int action, int arg) const {
    return pressing_ && pressAction_ == action && pressArg_ == arg;
}

// a press holds the button under the mouse; letting go over it does what it does
bool GameUI::onTankButton(bool down) {
    const Hit *h = hitAt(in.mx, in.my);
    if (down) {
        if (!h || !h->action) return false;
        pressing_ = true;
        pressAction_ = h->action;
        pressArg_ = h->arg;
        return true;
    }
    if (!pressing_) return false;
    pressing_ = false;
    if (!h || h->action != pressAction_ || h->arg != pressArg_) return false;
    if (h->action == HIT_STAT) {
        spendStat(h->arg);
    } else if (h->action == HIT_UPGRADE) {
        net::Writer w;
        w.u8(uint8_t(h->arg));
        game_.connection.send(MSG_UPGRADE, w.b);
    } else if (h->action == HIT_IGNORE) {
        if (const sim::DriverData *d = driver()) {
            ignored_ = true;
            ignoreUntil_ = d->level - d->level % 15 + 15;
        }
    }
    return true;
}

void GameUI::drawTankHud(Context &c, double now, float dt) {
    hits_.clear();
    c.resetTransform();
    c.globalAlpha(1);
    c.beginLayer();
    drawTankBars(c, dt);
    drawTankStats(c, dt);
    drawNotes(c, now);
    c.resetTransform();
    c.globalAlpha(1);
    c.endLayer(.7f);
}

// the score and level bars at the bottom, with your name over them
void GameUI::drawTankBars(Context &c, float dt) {
    const sim::DriverData *d = driver();
    Entity *me = sim_.flower();
    if (!d || !me || !me->tank || me->deadAt >= 0) return;
    float u = tankUnit(), H = float(c.height()), cx = float(c.width() / 2);
    int level = d->level;
    if (levelSeen_ >= 0 && level != levelSeen_) levelFill_ = true;
    levelSeen_ = level;
    // the fill creeps along at a bar a second up to the progress; a new level runs it on to full,
    // drops it to nothing and holds it there until the bar has drained
    levelCreep_ += dt / 1000;
    if (levelFill_) {
        if (levelCreep_ >= 1.75f) {
            levelFill_ = false;
            levelDrain_ = true;
            levelCreep_ = 0;
        }
    } else if (levelDrain_) {
        levelCreep_ = 0;
        if (levelPerc_ < .005f) levelDrain_ = false;
    } else if (level < kMaxLevel) {
        float from = float(levelScore(level)), to = float(levelScore(level + 1));
        levelCreep_ = std::min(levelCreep_, (d->score - from) / (to - from));
    }
    levelPerc_ = (levelCreep_ + levelPerc_ * 9) / 10;
    float levelY = float(int(float(int(H - 30 * u)) - 4 * u));
    bar(c, levelPerc_, cx, levelY, 424 * u, 24 * u, 0xffde43);
    if (!levelFill_) {
        const TankDef *def = tank(d->tank);
        levelText_ = "Lvl " + std::to_string(level) + ' ' + (def ? def->name : "");
    }
    text(c, levelText_, cx, levelY, 16 * u);

    const sim::ArenaData *arena = sim_.arena();
    bool board = arena && arena->leaderRows > 0;
    leaderShown_ = approach(leaderShown_, board ? float(arena->leaderScores[0]) : 0, dt, .1f);
    float t = board ? (leaderShown_ > 0 ? d->score / leaderShown_ : 1) : 0;
    scorePerc_ += (t - scorePerc_) * (1 - std::pow(.9f, dt / 16.666667f));
    float scoreY = float(int(levelY - 24 * u));
    bar(c, scorePerc_, cx, scoreY, 320 * u, 20 * u, 0x43ff91);
    text(c, "Score: " + commas((long long)d->score), cx, scoreY, 15 * u);
    float nameY = float(int(scoreY - 20 * u));
    if (me->name && !me->name->name.empty()) text(c, me->name->name, cx, nameY - 12 * u, 40 * u);
}

// the stats, bottom left: it slides out while there are points to spend, while the mouse is
// over that corner and while U or M is held
void GameUI::drawTankStats(Context &c, float dt) {
    Entity *cam = sim_.get(sim_.cameraId);
    if (!cam || !cam->driver) return;
    sim::DriverData &d = *cam->driver;
    const TankDef *def = tank(d.tank);
    bool alive = hasTank();
    float u = tankUnit(), h = float(c.height()), mx = in.mx, my = in.my;
    if (!alive && hadTank_) statBuild_.clear();
    hadTank_ = alive;
    if (in.keyU && !uWas_) queuedSinceU_ = false;
    if (!in.keyU && uWas_ && !queuedSinceU_) statBuild_.clear();
    uWas_ = in.keyU;

    // the build is spent in order, sending again whatever the server hasn't done yet
    int counts[8] = {}, spendable = d.points;
    for (char q : statBuild_) {
        int s = '8' - q;
        if (s < 0 || s > 7 || !def) continue;
        int k = ++counts[s];
        if (!alive || spendable < 1 || d.stats[s] > k - 1 || k - 1 >= def->stats[s].max) continue;
        applyStat(s, k);
        spendable--;
    }

    bool points = d.points > 0, keys = in.keyU || in.keyM, dead = game_.screen == Screen::Dead;
    float rate = .05f, target;
    if (dead) {
        statsLatch_ = true;
        target = 1;
    } else {
        if (!(mx <= 250 * u && my >= h - 392 * u)) statsLatch_ = false;
        if (mx < 80 * u && my > h - 342 * u) statsLatch_ = true;
        if (keys || (points && alive)) statsLatch_ = true;
        if (keys) rate = .15f;
        if (!alive) statsLatch_ = false;
        target = statsLatch_ ? 1.f : 0.f;
    }
    statsOpen_ = approach(statsOpen_, target, dt, rate);
    if (statsOpen_ < .001f || !def) return;

    float u24 = 24 * u, u30 = 30 * u, segW = u24 - 4 * u, segH = 16 * u, gap = 4 * u;
    float xf = 10 * u + u30 + (1 - statsOpen_) * -400 * u;
    float x = float(int(xf)), maxLen = 0;
    int rows = 0;
    for (int i = 0; i < 8; i++) {
        int limit = def->stats[i].max;
        if (limit <= 0) continue;
        int level = std::min(d.stats[i], limit);
        float y = float(int(h - 10 * u - u30 - rows * u24));
        float barLen = limit * u24 + u30, barEnd = barLen + x;
        uint32_t color = kStatColors[i];
        c.lineCap(cp6::Cap::Round);
        c.lineWidth(20 * u);
        c.strokeStyle(Color(0x000000));
        c.beginPath();
        c.moveTo(x, y);
        c.lineTo(barEnd, y);
        c.stroke();
        c.lineCap(cp6::Cap::Butt);
        // the first piece is round on the left
        auto pieces = [&](int from, int to, uint32_t rgb) {
            c.fillStyle(Color(rgb));
            float sx = (segW + gap) * from + x;
            for (int s = from; s < std::min(limit, to); s++, sx += segW + gap) {
                if (s == 0) {
                    c.beginPath();
                    c.arc(x, y, segH / 2, kPi / 2, -kPi / 2);
                    c.lineTo(segW + x, y - segH / 2);
                    c.lineTo(segW + x, y + segH / 2);
                    c.closePath();
                    c.fill();
                } else {
                    c.fillRect(sx, y - segH / 2, segW, segH);
                }
            }
        };
        pieces(0, level, color);
        if (counts[i] > level) pieces(level, counts[i], blend(color, .5f, 0));

        bool can = points && level < limit && alive;
        float zx = barEnd - u30, zy = y - segH / 2, zw = segH / 2 + u30;
        bool over = mx >= zx && mx <= zx + zw && my >= zy && my <= zy + segH;
        uint32_t bc = can ? color : 0x888888;
        if (can && over && pressed(HIT_STAT, i))
            bc = blend(bc, .8f, 0);
        else if (can && over)
            bc = blend(bc, .8f, 51);
        c.fillStyle(Color(bc));
        c.beginPath();
        c.arc(barEnd, y, 8 * u, -kPi / 2, kPi / 2);
        c.lineTo(barEnd - u30, y + segH / 2);
        c.lineTo(barEnd - u30, y - segH / 2);
        c.lineTo(barEnd, y - segH / 2);
        c.fill();
        float half = float(int(segH * .3f)), px = segH * .25f + barEnd - u30 / 2;
        c.lineCap(cp6::Cap::Square);
        c.strokeStyle(Color(0x000000));
        c.lineWidth(4 * u);
        c.beginPath();
        c.moveTo(px, y - half);
        c.lineTo(px, y + half);
        c.moveTo(px - half, y);
        c.lineTo(px + half, y);
        c.stroke();
        c.lineCap(cp6::Cap::Butt);

        text(c, def->stats[i].name, (barLen - u30) / 2 + x, y, 13 * u);
        char hint[8];
        std::snprintf(hint, sizeof hint, "[%d]", 8 - i);
        text(c, hint, barLen + x - u30 - gap - segW / 2, y, 10 * u);
        if (alive) hits_.push_back({zx, zy, zw, segH, HIT_STAT, i});
        maxLen = std::max(maxLen, barLen);
        rows++;
    }
    // "xN" hangs off the top of the longest bar
    if (d.points >= 2) {
        std::string s = "x" + std::to_string(d.points);
        TextBox box = measure(c, s, 24 * u);
        c.save();
        c.translate(maxLen + xf, h - 10 * u - u30 - rows * 24 * u + 15 * u - box.h / 2);
        c.rotate(-.3926991f);
        text(c, s, box.margin, box.h / 2, 24 * u, Align::Left);
        c.restore();
    }
}

void GameUI::drawNotes(Context &c, double now) {
    float u = tankUnit(), y = 10 * u;
    c.save();
    c.translate(float(c.width() / 2), 24 * u);
    for (size_t i = 0; i < notes_.size();) {
        Note &n = notes_[i];
        if (n.expiry > 0 && now >= n.expiry) {
            notes_.erase(notes_.begin() + long(i));
            continue;
        }
        float f = std::clamp(float(now - n.created) / 300, 0.f, 1.f);
        if (n.expiry > 0) f = std::min(f, std::clamp(float(n.expiry - now) / 300, 0.f, 1.f));
        f = f * f * (3 - 2 * f);
        float size = f * 20 * u;
        TextBox box = measure(c, n.text, size);
        float bw = box.w + 10 * u;
        c.globalAlpha(f * .5f);
        c.fillStyle(Color(n.color));
        c.fillRect(-bw / 2, y - box.h / 2, bw, box.h);
        c.globalAlpha(f);
        text(c, n.text, 0, y, size, Align::Center, false);
        y += f * 5 * u + box.h;
        i++;
    }
    c.globalAlpha(1);
    c.restore();
}

// a diep button: pastel, a dark border, its bottom shaded, or its top while pressed
static void button(Context &c, float x, float y, float w, float h, uint32_t rgb, bool enabled, bool hover, bool down,
                   const std::string &label, float u) {
    uint32_t fill = pastel(rgb);
    if (down)
        fill = blend(fill, .8f, 0);
    else if (enabled && hover)
        fill = blend(fill, .8f, 51);
    x = float(int(x - 15)) + 15;
    y = float(int(y - 15)) + 15;
    c.lineWidth(10 * u);
    c.strokeStyle(Color(0x555555));
    c.strokeRect(x, y, w, h);
    c.fillStyle(Color(fill));
    c.fillRect(x, y, w, h);
    float a = c.globalAlpha();
    c.globalAlpha(a * .2f);
    c.fillStyle(Color(0x000000));
    if (down)
        c.fillRect(x, y, w, h * 7 / 12);
    else
        c.fillRect(x, y + h * 7 / 12, w, h * 5 / 12);
    c.globalAlpha(a);
    if (!label.empty()) text(c, label, x + w / 2, y + h / 2, h * .56f);
}

// the tanks you can become, top left in two columns, and Ignore under them
void GameUI::drawUpgrades(Context &c, double now, float dt) {
    const sim::DriverData *d = driver();
    bool alive = d && hasTank();
    float u = tankUnit(), h = float(c.height()), mx = in.mx, my = in.my;
    const TankDef *def = d ? tank(d->tank) : nullptr;
    int level = d ? d->level : 0;
    if (d && ignored_) {
        if (level >= ignoreUntil_) ignored_ = false;
        if (mx < 30 * u && my < h / 2) ignored_ = false;
    }
    auto listed = [&](int id) {
        for (int i = 0; def && i < def->upgradeCount; i++)
            if (def->upgrades[i] == id) return true;
        return false;
    };
    auto open = [&](const Slot &s) {
        const TankDef *t = tank(s.tank);
        return alive && !ignored_ && listed(s.tank) && t && level >= t->levelRequirement;
    };
    if (alive && !ignored_ && def) {
        for (int i = 0; i < def->upgradeCount; i++) {
            int id = def->upgrades[i];
            const TankDef *t = tank(id);
            if (!t || level < t->levelRequirement) continue;
            bool have = false;
            for (const Slot &s : slots_) have |= s.tank == id;
            if (!have) slots_.push_back({id, 0, -1, -1, kStatColors[i & 7]});
        }
    }
    if (slots_.empty()) {
        slotsFade_ = 0;
        return;
    }
    float left = 200 * u, top0 = 75 * u, shift = 100 * u, box = 95 * u, gap = 15 * u, step = 115 * u;
    float most = 0, maxX = -shift, maxBottom = top0;
    bool any = false;
    c.resetTransform();
    c.globalAlpha(1);
    c.beginLayer();
    for (size_t i = 0; i < slots_.size();) {
        Slot &s = slots_[i];
        bool ok = open(s);
        s.fade = approach(s.fade, ok ? 1.f : 0.f, dt, .1f);
        if (!ok && s.fade < .001f) {
            slots_.erase(slots_.begin() + long(i));
            continue;
        }
        any |= ok;
        most = std::max(most, s.fade);
        if (s.col < 0) {
            auto taken = [&](int col, int row) {
                for (const Slot &o : slots_)
                    if (o.col == col && o.row == row && open(o)) return true;
                return false;
            };
            for (int row = 0;; row++) {
                if (!taken(0, row)) {
                    s.col = 0, s.row = row;
                    break;
                }
                if (!taken(1, row)) {
                    s.col = 1, s.row = row;
                    break;
                }
            }
        }
        float cx = s.fade * (s.col * step + left) - shift, top = (box + gap) * s.row + top0, bottom = top + box;
        bool over = mx >= cx - box / 2 && mx <= cx + box / 2 && my >= top && my <= bottom;
        bool usable = s.fade > .9f && ok;
        button(c, cx - box / 2, top, box, box, s.color, usable, over, over && pressed(HIT_UPGRADE, s.tank), "", u);
        drawTankIcon(c, s.tank, cx, top + box / 2, float(now / 3000), .4f * u, true);
        const TankDef *t = tank(s.tank);
        TextBox nb = measure(c, t ? t->name : "", 14 * u);
        text(c, t ? t->name : "", cx, bottom - nb.h / 2, 14 * u);
        if (usable) hits_.push_back({cx - box / 2, top, box, box, HIT_UPGRADE, s.tank});
        maxX = std::max(maxX, cx);
        if (ok) maxBottom = std::max(maxBottom, bottom + gap);
        i++;
    }
    float last = slotsFade_;
    slotsFade_ = most < .01f ? 0 : most;
    if (most >= .01f) {
        float wTo = (last * left - shift) / u, bTo = maxBottom / u;
        if (last == 0) {
            ignoreW_ = wTo;
            ignoreB_ = bTo;
        } else {
            float k = std::clamp(1 - std::pow(.9f, dt / 16.666667f), 0.f, 1.f);
            ignoreW_ = k * wTo + (1 - k) * ignoreW_;
            if (any) ignoreB_ = k * bTo + (1 - k) * ignoreB_;
        }
        float ix = (maxX + ignoreW_ * u) / 2, iy = ignoreB_ * u, iw = 80 * u, ih = 25 * u, il = ix - iw / 2;
        text(c, "Upgrades", std::round(ix), std::round(40 * u), 24 * u);
        bool usable = most > .9f && any;
        bool over = mx >= il && mx <= il + iw && my >= iy && my <= iy + ih;
        button(c, il, iy, iw, ih, 0x777777, usable, over, over && pressed(HIT_IGNORE, 0), "Ignore", u);
        if (usable) hits_.push_back({il, iy, iw, ih, HIT_IGNORE, 0});
    }
    c.resetTransform();
    c.globalAlpha(1);
    c.endLayer(.9f);
}

int GameUI::treeNode(int id, int sibling, int depth) {
    int index = int(tree_.size());
    tree_.emplace_back();
    const TankDef *t = tank(id);
    tree_[index].tank = id;
    tree_[index].level = t ? t->levelRequirement : 0;
    tree_[index].sibling = sibling;
    if (depth >= 16 || !t) return index;
    for (int i = 0; i < t->upgradeCount; i++) {
        int child = treeNode(t->upgrades[i], i, depth + 1);
        tree_[index].children.push_back(child);
    }
    return index;
}

// every class, out from the Tank in the middle by level, while Y is held
void GameUI::drawTree(Context &c, double now) {
    if (in.keyY && hasTank()) {
        treeFade_ = (treeFade_ * 9 + 1) / 10;
    } else {
        if (treeFade_ > .99f) treeFade_ = .99f;
        treeFade_ = std::pow(treeFade_, 1.25f);
    }
    if (treeFade_ < .01f) return;
    if (tree_.empty()) {
        treeNode(0, 0, 0);
        int leaves = 0;
        for (const TreeNode &n : tree_) leaves += n.children.empty();
        // leaves get equal slices in order, and parents span their children
        int at = 0;
        std::vector<int> post;
        auto walk = [&](auto &self, int i) -> void {
            for (int ch : tree_[i].children) self(self, ch);
            TreeNode &n = tree_[i];
            if (n.children.empty()) {
                double slice = 6.283185 / leaves;
                n.start = float(slice * at);
                n.end = float(slice * (at + 1));
                at++;
            } else {
                n.start = tree_[n.children.front()].start;
                n.end = tree_[n.children.back()].end;
            }
            post.push_back(i);
        };
        walk(walk, 0);
        std::stable_sort(post.begin(), post.end(), [&](int a, int b) { return tree_[a].level > tree_[b].level; });
        int index = 0;
        auto add = [&](const TreeSector &s) {
            auto ring =
                std::find_if(rings_.begin(), rings_.end(), [&](const TreeRing &r) { return r.inner == s.inner; });
            if (ring == rings_.end()) {
                rings_.emplace_back();
                ring = rings_.end() - 1;
                ring->inner = s.inner;
            }
            ring->sectors.push_back(s);
        };
        for (int i : post) {
            const TreeNode &n = tree_[i];
            int outer = 60;
            for (int ch : n.children) outer = std::min(outer, tree_[ch].level);
            for (int ch : n.children)
                if (tree_[ch].level > outer) add({-1, tree_[ch].start, tree_[ch].end, outer, tree_[ch].level, index++});
            if (n.level != 0) add({i, n.start, n.end, n.level, outer, index++});
        }
        std::stable_sort(rings_.begin(), rings_.end(),
                         [](const TreeRing &a, const TreeRing &b) { return a.inner > b.inner; });
    }
    float u = tankUnit(), radius = u * 400, line = u * 5;
    for (const TreeRing &ring : rings_) {
        // spun in from half a turn back and grown with the fade, the outer rings lagging
        float l = ring.inner / 15.f, p = std::pow(treeFade_, l);
        c.resetTransform();
        c.globalAlpha(1);
        c.beginLayer();
        c.translate(float(c.width() / 2), float(c.height() / 2));
        c.rotate(float(now / 15000));
        c.rotate((1 - p) * -kPi);
        c.scale(p);
        c.lineWidth(line);
        c.strokeStyle(Color(0x555555));
        for (const TreeSector &s : ring.sectors) {
            float in0 = s.inner / 60.f * radius, out0 = s.outer / 60.f * radius;
            c.fillStyle(Color(s.node < 0 ? 0x555555 : pastel(kStatColors[tree_[s.node].sibling & 7])));
            c.beginPath();
            c.arc(0, 0, in0, s.start, s.end);
            c.lineTo(out0 * std::cos(s.end), out0 * std::sin(s.end));
            c.arc(0, 0, out0, s.end, s.start, true);
            c.closePath();
            c.fill();
            c.stroke();
        }
        for (const TreeSector &s : ring.sectors) {
            if (s.node < 0) continue;
            float mid = (s.start + s.end) / 2, r = (s.inner / 60.f * radius + s.outer / 60.f * radius) / 2;
            drawTankIcon(c, tree_[s.node].tank, r * std::cos(mid), r * std::sin(mid), mid, u * .25f, true);
            c.lineWidth(line);
            c.strokeStyle(Color(0x555555));
        }
        c.resetTransform();
        c.globalAlpha(1);
        c.endLayer(.7f * std::pow(treeFade_, l * .3f));
    }
}

// who killed you, what you had and for how long, over a darkened screen
void GameUI::drawTankDeath(Context &c, float dt) {
    bool dead = game_.screen == Screen::Dead;
    dimK_ = approach(dimK_, dead ? 1.f : 0.f, dt, dead ? .05f : .1f);
    deathK_ = approach(deathK_, dead ? 1.f : 0.f, dt, dead ? .05f : .1f);
    c.resetTransform();
    if (dimK_ > .01f) {
        c.globalAlpha(dimK_ * .4f);
        c.fillStyle(Color(0x000000));
        c.fillRect(0, 0, float(c.width()), float(c.height()));
    }
    sim::CameraData *cam = sim_.camera();
    const sim::DriverData *d = driver();
    if (deathK_ <= .01f || !cam || !d) {
        c.globalAlpha(1);
        return;
    }
    float u = tankUnit(), H = float(c.height());
    float cy = float(int(deathK_ * (float(int(H) / 2) + 200 * u) - 200 * u));
    c.globalAlpha(deathK_);
    c.translate(float(c.width() / 2), cy);
    bool unnamed = cam->killer.empty();
    const std::string mystery = "(they seem to prefer to keep an air of mystery about them)";
    std::string killer = unnamed ? "An unnamed tank" : cam->killer;
    TextBox by = measure(c, "You were killed by:", 18 * u), kb = measure(c, killer, 24 * u),
            mb = measure(c, mystery, 14 * u);
    float top = -150 * u - (unnamed ? mb.h : 0);
    text(c, "You were killed by:", 0, std::round(top - by.h / 2) + by.h / 2, 18 * u);
    text(c, killer, 0, std::round(top + by.h - kb.h / 2) + kb.h / 2, 24 * u);
    if (unnamed) text(c, mystery, 0, std::round(top + by.h + kb.h - 5 * u - mb.h / 2) + mb.h / 2, 14 * u);

    drawTankIcon(c, d->deathTank, 100 * u, -20 * u, -kPi / 4, .7f * u, false);
    const TankDef *t = tank(d->deathTank);
    text(c, t ? t->name : "", 100 * u, -20 * u + 75 * u, 18 * u);

    float rx = float(int(-100 * u)), ry = float(int(-50 * u));
    auto row = [&](const std::string &label, const std::string &value) {
        TextBox lb = measure(c, label, 18 * u), vb = measure(c, value, 24 * u);
        text(c, label, rx - lb.margin, float(int(ry - lb.h / 2)) + lb.h / 2, 18 * u, Align::Right);
        text(c, value, rx + vb.margin, float(int(ry - vb.h / 2)) + vb.h / 2, 24 * u, Align::Left);
        ry += vb.h;
    };
    row("Score:", commas((long long)cam->score));
    row("Level:", std::to_string(cam->deathLevel > 0 ? cam->deathLevel : d->level));
    if (d->alive > 0) row("Time Alive:", timeAlive(d->alive));
    TextBox hb = measure(c, "(press enter to continue)", 18 * u);
    text(c, "(press enter to continue)", 0, std::round(100 * u - hb.h / 2) + hb.h / 2, 18 * u);
    c.resetTransform();
    c.globalAlpha(1);
}

}  // namespace game
