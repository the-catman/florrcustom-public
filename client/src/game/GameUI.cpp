#include "game/GameUI.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "game/Draw.h"
#include "game/DrawTank.h"
#include "game/Ease.h"
#include "game/Game.h"
#include "game/Ui.h"

extern "C" {
int ui_prompt(const char *cur, char *buf, int cap);
void ui_storage_set(const char *key, const char *val);
}

namespace game {

using cp6::Align;
using cp6::Baseline;
using cp6::Color;
using cp6::Context;
using sim::Entity;

static constexpr float TAU = 6.28318531f;
// HUD: in UI units of max(W/1920, H/1080), at least 0.7, so a 16:9 screen is 1920 x 1080 units
// (more in a small window). Everything but the leaderboard is drawn at 85% opacity.
static constexpr float HUD_ALPHA = .85f;  // the HUD layer goes on the screen at this alpha

GameUI::GameUI(Game &game)
    : inventory(game),
      menus(game),
      touch(game),
      game_(game),
      sim_(game.simulation),
      ctx_(game.ctx),
      conn_(game.connection),
      in(game.in) {}

void GameUI::render(Context &c, double now, float dt, float frameMs) {
    c.beginFrame(0x1ea761);
    iris_ = std::min(1.f, std::max(0.f, iris_ + (game_.screen == Screen::Title ? -dt : dt) * .001f));
    bool inner = iris_ >= .0001f;
    if (inner) {
        game_.rendering.drawWorld(c, now, dt);
        drawHud(c, now);
    }
    if (iris_ < 1) {
        float W = float(c.width()), H = float(c.height());
        float r = iris_ * iris_ * iris_ * std::sqrt(W * W * .25f + H * H * .25f);
        c.save();
        if (inner) {
            c.resetTransform();
            c.globalAlpha(1);
            c.clipCircle(W * .5f, H * .5f, r, true);
            c.fillStyle(Color(0x1ea761));
            c.fillRect(0, 0, W, H);
        }
        drawTitle(c, now);
        if (inner) {
            c.resetTransform();
            c.globalAlpha(1);
            c.fillStyle(Color(0x000000));
            c.beginPath();
            c.arc(W * .5f, H * .5f, r + 5 * hudScale() * std::min(1.f, iris_ * 3), 0, TAU);
            c.fill();
        }
        c.restore();
    }
    drawStats(c, frameMs);
    if (game_.mobile && c.height() > c.width() && hudW() < 560) drawRotateHint(c, now);
    c.endFrame();
}

// a phone held upright is too narrow for the controls: ask for it to be turned, with a phone
// outline that turns sideways and back
void GameUI::drawRotateHint(Context &c, double now) {
    float W = float(c.width()), H = float(c.height()), k = W / 400, h = H / k;
    c.resetTransform();
    c.globalAlpha(.6f);
    c.fillStyle(Color(0x000000));
    c.fillRect(0, 0, W, H);
    c.globalAlpha(1);
    c.setTransform(k, 0, 0, k, 0, 0);
    float t = float(std::fmod(now, 3000.0)) / 3000;
    auto smoothStep = [](float x) {
        x = std::clamp(x, 0.f, 1.f);
        return x * x * (3 - 2 * x);
    };
    float turn = (t < .5f ? smoothStep((t - .15f) / .25f) : 1 - smoothStep((t - .65f) / .25f)) * -TAU * .25f;
    c.save();
    c.translate(200, h * .5f - 60);
    c.rotate(turn);
    c.lineCap(cp6::Cap::Round);
    for (int pass = 0; pass < 2; pass++) {
        c.lineWidth(pass ? 5 : 11);
        c.strokeStyle(Color(pass ? 0xffffff : 0x000000));
        roundRect(c, -24, -40, 48, 80, 8);
        c.stroke();
        c.beginPath();
        c.moveTo(-6, 30);
        c.lineTo(6, 30);
        c.stroke();
    }
    c.restore();
    outlinedText(c, "Rotate your device", 200, h * .5f + 25, 30);
    outlinedText(c, "to play", 200, h * .5f + 58, 20);
}

void GameUI::drawTitle(Context &c, double now) {
    float dt = frameTime(titleLast_, now, 100);
    // everything here is in HUD units, laid out from a quarter and a half of the screen height
    float u = hudScale();
    int la = int(hudW()), ia = int(hudH());
    float cx = la * .5f, q = float(ia / 4), half = float(ia / 2);
    bool touch = game_.touchMode();
    auto rand01 = [] { return float(std::rand()) / (float(RAND_MAX) + 1); };

    // petals drift in from the left, one every 200 ms, and bob up and down; bigger ones are faster
    // and drawn on top
    floaterTimer_ -= dt;
    while (floaterTimer_ < 0) {
        floaterTimer_ += 200;
        if (floaters_.size() >= 100) continue;
        Floater f;
        f.x = -50;
        f.y = rand01() * float(ia + 40) - 20;
        f.scale = .5f + rand01() * 1.5f;
        f.phase = rand01() * TAU;
        f.type = menus.floaterPetal();
        f.id = uint32_t(std::rand());
        floaters_.push_back(f);
    }
    for (auto &f : floaters_) f.x += f.scale * 100 * dt * .001f;
    floaters_.erase(
        std::remove_if(floaters_.begin(), floaters_.end(), [&](const Floater &f) { return f.x >= float(la + 50); }),
        floaters_.end());
    std::stable_sort(floaters_.begin(), floaters_.end(),
                     [](const Floater &a, const Floater &b) { return a.scale < b.scale; });
    Look l;
    l.time = now;
    for (auto &f : floaters_) {
        c.setTransform(u, 0, 0, u, 0, 0);
        c.translate(f.x, f.y + std::sin(float(now * .001) + f.phase) * 20);
        c.scale(f.scale);
        l.id = f.id;
        drawPetal(c, f.type, l);
    }
    c.setTransform(u, 0, 0, u, 0, 0);
    // after a death: the level (and any petals kept) the next flower starts with; drawn before the
    // name box, which covers the line when it sits lower for touch controls
    // read while on the title only, so it holds still while the screen opens into the game
    if (game_.screen == Screen::Title) {
        sim::CameraData *cam = sim_.camera();
        titleLevel_ = cam ? cam->respawnLevel : 0;
        titleKept_ = cam ? std::min(10, cam->keptCount) : 0;
        titleKeptTypes_.clear();
        if (cam) titleKeptTypes_.assign(cam->kept.begin(), cam->kept.begin() + titleKept_);
    }
    if (titleLevel_ > 1 || titleKept_ > 0) {
        char buf[64];
        std::snprintf(buf, sizeof buf,
                      titleKept_ ? "You will respawn at level %d with:" : "You will respawn at level %d",
                      std::max(1, titleLevel_));
        outlinedText(c, buf, cx, half + 66.2f, 16);
        int kept = int(titleKeptTypes_.size());
        for (int i = 0; i < kept; i++)
            card(c, cx + (i - (kept - 1) * .5f) * 70, half + 125, 60, titleKeptTypes_[i], now, 1);
    } else if (!touch) {
        outlinedText(c, "How to play", cx, half + 100, 30);
        outlinedText(c, game_.settings.keyboardControl ? "Use WASD or arrow keys to move" : "Use mouse to move", cx,
                     half + 142, 16);
        outlinedText(c, game_.settings.keyboardControl ? "SPACE to attack" : "Left click to attack", cx, half + 164.4f,
                     16);
        outlinedText(c, game_.settings.keyboardControl ? "SHIFT to defend" : "Right click to defend", cx, half + 186.8f,
                     16);
    }
    outlinedText(c, "florr.io", cx, q, game_.mobile ? 60 : 80);

    // until the server answers, "Connecting..." drops in where the name box was and the box goes up
    // once spawning it goes up the same way, and comes back 200 ms after the title returns
    float k9 = ease(.9f, dt);
    connK_ += ((conn_.gotUpdate() ? 0.f : 1.f) - connK_) * k9;
    if (game_.screen != Screen::Title) inGameAt_ = now;
    leaveK_ += ((now - inGameAt_ < 200 ? 1.f : 0.f) - leaveK_) * k9;
    float away = boxAway();
    if (away < .999f) {
        c.save();
        c.translate(0, -(400 + q) * away);
        c.globalAlpha(1 - away);
        // on a phone or tablet the box is taller and sits lower; with touch controls it has a play
        // button beside it
        float bh = game_.mobile ? 70.f : 50.f, bx = cx - 200, by = game_.mobile ? half : q + 200;
        if (game_.tankServer()) drawPlayKind(c, now);
        outlinedText(c, "This pretty little flower is called...", cx, by - 16.8f, 20);
        if (touch) {
            c.lineWidth(6);
            c.fillStyle(Color(0xbb5555));
            c.strokeStyle(Color(0x974545));
            roundRect(c, cx + 210, by, bh, bh, 6);
            c.fill();
            c.stroke();
            // a triangle pointing right
            float r = bh * .3f, px = cx + 210 + bh * .42f, py = by + bh * .5f;
            c.fillStyle(Color(0x974545));
            c.beginPath();
            c.moveTo(px + r, py);
            c.lineTo(px - r * .5f, py + r * .866025f);
            c.lineTo(px - r * .5f, py - r * .866025f);
            c.closePath();
            c.fill();
            c.stroke();
        }
        c.fillStyle(Color(0xeeeeee));
        c.fillRect(bx, by, 400, bh);
        c.strokeStyle(Color(0x000000));
        c.lineWidth(bh * .1f);
        c.strokeRect(bx, by, 400, bh);
        // a long name scrolls so its end and the caret stay in the box
        float in0 = bh * .1f, iw = 400 - bh * .2f;
        c.save();
        c.beginPath();
        c.rect(bx + in0, by + in0, iw, bh * .8f);
        c.clip();
        c.font(bh * .64f);
        c.textAlign(Align::Left);
        c.textBaseline(Baseline::Middle);
        float tw = game_.name.empty() ? 0 : c.measureText(game_.name);
        float tx = bx + in0 + std::min(0.f, iw - tw);
        c.fillStyle(Color(0x000000));
        if (!game_.name.empty()) c.fillText(game_.name, tx, by + bh * .548f);
        if (!game_.mobile) {  // on a phone the name comes from a dialog, so there is no caret
            c.globalAlpha(c.globalAlpha() * std::fabs(std::sin(float(now) * .0065f)));
            c.lineCap(cp6::Cap::Square);
            c.beginPath();
            c.moveTo(tx + tw, by + bh * .18f);
            c.lineTo(tx + tw, by + bh * .82f);
            c.lineWidth(3);
            c.strokeStyle(Color(0x000000));
            c.stroke();
            c.lineWidth(2);
            c.strokeStyle(Color(0xffffff));
            c.stroke();
        }
        c.restore();
        if (!touch) outlinedText(c, "(press enter to spawn)", cx, q + 264.8f, 14);
        c.restore();
    }
    if (connK_ > .001f) {
        c.globalAlpha(connK_);
        outlinedText(c, "Connecting...", cx, connK_ * (q + 400) - 200, 48);
        c.globalAlpha(1);
    }
    menus.draw(c, now);
}

// on a phone the HUD is twice as big and has no lower limit; tablets keep the plain size
float GameUI::hudScale() const {
    float k = std::max(ctx_.width() / 1920.f, ctx_.height() / 1080.f);
    return game_.mobile ? k * (game_.tablet ? 1.f : 2.f) : std::max(.7f, k);
}

float GameUI::hudW() const {
    return std::ceil(ctx_.width() / hudScale() - 1e-3f);
}

float GameUI::hudH() const {
    return std::ceil(ctx_.height() / hudScale() - 1e-3f);
}

void GameUI::drawHud(Context &c, double now) {
    float W = float(c.width()), H = float(c.height());
    float u = hudScale(), Wu = W / u, Hu = H / u;
    c.resetTransform();
    c.setTransform(u, 0, 0, u, 0, 0);
    c.globalAlpha(1);
    // the logo, the panels, the zone warning and the touch controls are drawn as one layer
    c.beginLayer();
    bool touch = game_.touchMode();
    bool tanks = game_.tankMode();
    // in tank mode the upgrade buttons take the top left, and the logo makes way
    if (!touch && slotsFade_ < 1) {
        c.globalAlpha(tanks ? 1 - slotsFade_ : 1.f);
        outlinedText(c, "florr.io", 15, 37.4f, 32, Color(0xffffff), Align::Left);
        c.globalAlpha(1);
    }
    float dtr = frameTime(royaleLast_, now);

    // the panels show while the camera has a flower; once it is dead they slide away, the level
    // bar to the left, the minimap to the right and the inventory down
    Entity *me = sim_.flower();
    if (me && (me->flower || me->tank) && game_.screen != Screen::Title) {
        int level = me->flower ? std::max(1, me->flower->level) : 1;
        // the bar eases toward the progress the server sends, which tops out just short of full
        float dtl = levelLast_ > 0 ? float(now - levelLast_) : 0.f;
        levelLast_ = now;
        if (me->hp() > 0)
            hudSlide_ = 0;
        else
            hudSlide_ += (400 - hudSlide_) * ease(.9f, std::min(dtl, 250.f));
        // these two go with the minimap
        drawWin(c, dtr, me);
        drawZoneWarning(c, now);
        c.setTransform(u, 0, 0, u, 0, 0);
        c.globalAlpha(1);
        c.save();
        c.translate(-hudSlide_, 0);
        if (me->flower) {
            levelShown_ += (float(me->flower->xp) * .000015f - levelShown_) * ease(.8f, dtl);
            float prog = std::min(1.f, std::max(0.f, levelShown_));
            // name, level bar and the next slot unlock; with touch controls they go top left
            float by = touch ? 65.f : Hu - 109.6f;
            c.lineCap(cp6::Cap::Round);
            c.strokeStyle(Color(0x333333));
            c.lineWidth(40);
            c.beginPath();
            c.moveTo(0, by);
            c.lineTo(300, by);
            c.stroke();
            float k = std::min(1.f, prog * 30);
            if (k > 0) {
                c.globalAlpha(k);
                c.strokeStyle(Color(0xf5ff65));
                c.lineWidth(34 * k);
                c.beginPath();
                c.moveTo(0, by);
                c.lineTo(prog * 300, by);
                c.stroke();
                c.globalAlpha(1);
            }
            c.lineCap(cp6::Cap::Butt);
            outlinedText(c, game_.name.empty() ? "" : game_.name, 150, by - 38.2f, 24);
            outlinedText(c, "Lvl " + std::to_string(level) + " Flower", 150, by, 16);
            if (level < 45 && !touch)
                outlinedText(c,
                             "Extra petal slot at level " + std::to_string(level < 15   ? 15
                                                                           : level < 30 ? 30
                                                                                        : 45),
                             150, Hu - 77.7f, 14);
        }
        c.restore();
        if (me->flower) {
            c.save();
            c.translate(0, hudSlide_);
            inventory.draw(c, now, level, Wu, Hu);
            c.restore();
        }
        c.save();
        c.translate(hudSlide_, 0);

        // minimap; with touch controls it is top right, and goes up out of the way of the leaderboard
        float mx = touch ? hudW() - 350 : Wu - 300, my = touch ? 65 - 200 * this->touch.boardK() : Hu - 119.6f;
        const uint32_t zc[4] = {0x1ea761, 0xa7a71e, 0xa7271e, 0x555555};
        const float zw[4] = {75, 75, 75, 25};
        const char *zn[4] = {"Easy", "Medium", "Hard", "?"};
        float zx = mx;
        for (int i = 0; i < 4; i++) {
            c.fillStyle(Color(zc[i]));
            c.fillRect(zx, my, zw[i], 40);
            zx += zw[i];
        }
        zx = mx;
        for (int i = 0; i < 4; i++) {
            outlinedText(c, zn[i], zx + zw[i] * .5f, my + 20, 14);
            zx += zw[i];
        }
        const sim::ArenaData *arena = sim_.arena();
        float mapW = arena && arena->width > 0 ? arena->width : 50000;
        float frac = std::min(1.f, std::max(0.f, me->x / mapW + .5f));
        c.lineWidth(4);
        c.fillStyle(Color(0xffe763));
        c.strokeStyle(Color(0xd3bd46));
        c.beginPath();
        c.arc(mx + 20 + frac * 210, my + 20, 15, 0, TAU);
        c.fill();
        c.stroke();
        c.lineWidth(5);
        c.strokeStyle(Color(0x333333));
        c.strokeRect(mx, my, 250, 40);
        outlinedText(c, "Map", mx + 125, my - 15, 24);
        c.restore();
        if (touch) this->touch.draw(c, hudW(), hudH());
    }
    c.setTransform(u, 0, 0, u, 0, 0);
    c.globalAlpha(1);
    c.endLayer(HUD_ALPHA);

    if (tanks) {
        float dtt = frameTime(tankLast_, now);
        if (game_.screen != Screen::Title || deadHud_) drawTankHud(c, now, dtt);
        c.setTransform(u, 0, 0, u, 0, 0);
        c.globalAlpha(1);
        if (game_.screen != Screen::Title || deadHud_) drawLeaderboard(c, Wu);
        drawRoyale(c, dtr);
        drawTree(c, now);
        drawUpgrades(c, now, dtt);
        drawTankDeath(c, dtt);
        c.resetTransform();
        return;
    }
    if (deadHud_) {
        c.globalAlpha(1);
        drawLeaderboard(c, Wu);
        drawRoyale(c, dtr);
        drawDeath(c, now);
        return;
    }
    c.globalAlpha(1);
    if (game_.screen != Screen::Title) drawLeaderboard(c, Wu);
    if (game_.screen != Screen::Title) drawRoyale(c, dtr);
    inventory.drawTooltips(c, now);
    if (picker_ && game_.screen == Screen::Playing) drawPicker(c, now);

    c.resetTransform();
    drawNotes(c, now);
}

// a box above the flower when it is left of the zone its level belongs to: past the level each zone
// is meant for, attacks stop once the pie runs out; in a closing arena the area itself goes away
void GameUI::drawZoneWarning(Context &c, double now) {
    float dt = frameTime(zoneLast_, now);
    Entity *me = sim_.get(game_.player);
    const sim::ArenaData *arena = sim_.arena();
    bool closing = arena && arena->closing(), show = false;
    float timer = 0;
    if (me && me->flower && arena && game_.screen == Screen::Playing) {
        show = me->x < game_.rendering.zoneEdge(me->flower->level);
        timer = float(me->flower->zoneTimer) / 255.f;
    }
    zoneAlpha_ += ((show ? 1.f : 0.f) - zoneAlpha_) * ease(.7f, dt);
    zoneTimer_ += (timer - zoneTimer_) * ease(.8f, dt);
    if (zoneAlpha_ < .0001f) return;

    BoxLine lines[3] = {{"MOVE RIGHT ->", 20, 0xffaaaa},
                        {closing ? "This area is no longer playable" : "You are overleveled for this zone", 14},
                        {closing ? "You will be destroyed" : "You will be unable to attack", 14}};
    float u = hudScale(), Wu = c.width() / u, Hu = c.height() / u;
    c.setTransform(u, 0, 0, u, 0, 0);
    c.translate(hudSlide_, 0);
    BoxArea a =
        textPanel(c, lines, 3, Wu * .5f, Hu * .5f - 170, BoxPlace::Middle, closing ? 0 : 65, zoneAlpha_, Wu, Hu);
    if (!closing) {  // the pie empties as the time left runs out
        float px = a.x + a.w - 30, py = a.y + a.h * .5f;
        c.fillStyle(Color(0x333333));
        c.beginPath();
        c.arc(px, py, 22, 0, TAU);
        c.fill();
        c.fillStyle(Color(0xaa3333));
        c.beginPath();
        c.moveTo(px, py);
        c.arc(px, py, 18, -TAU / 4, -TAU / 4 - TAU * zoneTimer_, true);
        c.closePath();
        c.fill();
    }
    c.globalAlpha(1);
}

// battle royale boxes in the middle of the screen, sliding down from above as they fade in:
// "YOU WON" while the arena says you won and you are alive (in the HUD layer, moving off with the
// minimap), and "Battle Royale" while the round has not started, with a countdown once it is about to
static void royaleBox(Context &c, float k, float slide, float w, const std::string &a, const std::string &b, float u) {
    float Wu = c.width() / u, Hu = c.height() / u;
    c.setTransform(u, 0, 0, u, 0, 0);
    c.translate(Wu * .5f + slide, Hu * .5f - (1 - k) * Hu);
    c.globalAlpha(std::min(1.f, k) * .5f);
    c.fillStyle(Color(0x000000));
    roundRect(c, -w * .5f, -155, w, 95, 5);
    c.fill();
    c.globalAlpha(std::min(1.f, k));
    outlinedText(c, a, 0, -150 + 38 * .7f, 38);
    outlinedText(c, b, 0, -100 + 24 * .7f, 24);
}

void GameUI::drawWin(Context &c, float dt, Entity *me) {
    const sim::ArenaData *arena = sim_.arena();
    bool won = arena && arena->won() && me->hp() > 0;
    winK_ += ((won ? 1.f : 0.f) - winK_) * ease(.7f, dt);
    if (winK_ > .0001f) royaleBox(c, winK_, hudSlide_, 380, "YOU WON", "Press O to go to the next game", hudScale());
}

void GameUI::drawRoyale(Context &c, float dt) {
    const sim::ArenaData *arena = sim_.arena();
    bool running = !arena || arena->running();
    const sim::RoyaleData *round = nullptr;
    for (const auto &kv : sim_.entities)
        if (kv.second.royale) {
            round = &*kv.second.royale;
            break;
        }
    if (!round || !sim_.camera()) return;
    royaleK_ += ((running ? 0.f : 1.f) - royaleK_) * ease(.9f, dt);
    if (royaleK_ < .0001f) return;
    std::string line = "Waiting for players";
    royaleTime_.set(round->timeLeft, game_.time);  // gliding like positions do
    if (round->starting)
        line = "Starting in " + std::to_string(int(std::ceil(royaleTime_.at(game_.time) * .001f))) + "...";
    royaleBox(c, royaleK_, 0, 300, "Battle Royale", line, hudScale());
    c.setTransform(hudScale(), 0, 0, hudScale(), 0, 0);
    c.globalAlpha(1);
}

// with ; pressed: the heap size, canvas size and frame times over the last 101 frames, and the build,
// in small print in the bottom right
void GameUI::drawStats(Context &c, float ms) {
    if (!stats_) return;
    frameMs_.push_back(ms);
    if (frameMs_.size() > 101) frameMs_.erase(frameMs_.begin());
    fpsShown_ +=
        (std::min(1000.f, 1000.f / std::max(ms, 1e-3f)) - fpsShown_) * std::min(1.f, 1 - std::pow(.95f, ms * .06f));
    if (++statsFrame_ >= 10) {
        statsFrame_ = 0;
        float lo = 1e9f, hi = 0, sum = 0;
        for (float f : frameMs_) {
            lo = std::min(lo, f);
            hi = std::max(hi, f);
            sum += f;
        }
        char buf[160];
        std::snprintf(buf, sizeof buf, "%zu KB - %dx%d - %.1f/%.1f/%.1f ms (min/avg/max) - %.1f fps",
                      size_t(sbrk(0)) >> 10, c.width(), c.height(), lo, sum / frameMs_.size(), hi, fpsShown_);
        statsText_ = buf;
    }
    float u = hudScale(), Wu = c.width() / u, Hu = c.height() / u;
    c.setTransform(u, 0, 0, u, 0, 0);
    c.globalAlpha(1);
    if (!statsText_.empty()) outlinedText(c, statsText_, Wu - 6, Hu - 20 + 12 * .7f, 12, Color(0xffffff), Align::Right);
    outlinedText(c, "Build " __DATE__ " " __TIME__, Wu - 6, Hu - 34 + 12 * .7f, 12, Color(0xffffff), Align::Right);
}

// the world dims, the flower lies crossed out in the middle, and the run's stats sit bottom left;
// they glide in from above and below as the flower dies
void GameUI::drawDeath(Context &c, double now) {
    sim::CameraData *cam = sim_.camera();
    float u = hudScale(), Wu = c.width() / u, Hu = c.height() / u;
    float t = ease(.95f, std::max(0.f, float(now - game_.deadAt)));
    c.resetTransform();
    c.globalAlpha(.25f * t);
    c.fillStyle(Color(0x000000));
    c.fillRect(0, 0, float(c.width()), float(c.height()));
    c.setTransform(u, 0, 0, u, 0, 0);
    c.globalAlpha(1);
    bool touch = game_.touchMode();
    if (!touch) outlinedText(c, "florr.io", 15, 37.4f, 32, Color(0xffffff), Align::Left);
    float slide = (1 - t) * Hu;
    float cx = Wu * .5f, cy = Hu * .5f - slide;
    outlinedText(c, "You were destroyed by:", cx, cy - 121.4f, 18);
    outlinedText(c, game_.lastLife.killer.empty() ? "A mysterious entity" : game_.lastLife.killer, cx, cy - 91.4f, 24);
    outlinedText(c, touch ? "(tap to continue)" : "(press enter to continue)", cx, cy + 88.6f, 18);
    // the flower, tilted, with the petals of its first five slots: three behind it and two in front
    c.save();
    c.translate(cx, cy);
    c.scale(1.5f);
    static const float spot[5][2] = {{35, 20}, {-10, 30}, {-32, 5}, {-10, -15}, {30, -5}};
    Look pl;
    pl.time = now;
    pl.ui = true;
    auto petal = [&](int i) {
        int t = game_.lastLife.petals[i];
        if (t <= 0 || t >= P_COUNT || petalClustered(t)) return;
        c.save();
        c.translate(spot[i][0], spot[i][1]);
        pl.id = uint32_t(i + 1);
        drawPetal(c, t, pl);
        c.restore();
    };
    for (int i = 2; i < 5; i++) petal(i);
    c.save();
    c.rotate(-.2f);
    Look fl;
    fl.dead = true;
    fl.time = now;
    drawFlower(c, 25, fl, 0, 0, -5, 0);
    c.restore();
    petal(1);
    petal(0);
    c.restore();
    // stats
    c.translate(0, slide);
    outlinedText(c, "Stats", 22.9f, Hu - 142.9f, 24, Color(0xffffff), Align::Left);
    char buf[64];
    double pts = cam ? cam->score : 0, ap = std::fabs(pts);
    if (ap < 1000)
        std::snprintf(buf, sizeof buf, "Points: %.0f", pts);
    else if (ap < 1000000)
        std::snprintf(buf, sizeof buf, "Points: %.1fk", pts * 0.001);
    else
        std::snprintf(buf, sizeof buf, "Points: %.1fm", pts * 0.000001);
    outlinedText(c, buf, 22.9f, Hu - 115.7f, 16, Color(0xffffff), Align::Left);
    std::snprintf(buf, sizeof buf, "Level: %d", cam && cam->deathLevel > 0 ? cam->deathLevel : game_.lastLife.level);
    outlinedText(c, buf, 22.9f, Hu - 92.9f, 16, Color(0xffffff), Align::Left);
    // the cards it had: the first row at 60, the second at 50, packed 10 apart; each has its own
    // tooltip, fading in while hovered
    float dt = frameTime(deathLast_, now);
    float k7 = ease(.7f, dt), left = 20, mx = in.mx / u, my = in.my / u - slide;
    float tx[16], ty[16];
    int n = std::min(16, int(game_.lastLife.slots.size()));
    for (int i = 0; i < n; i++) {
        float size = i < int(game_.lastLife.ring.size()) ? 60.f : 50.f, x = left + size * .5f, y = Hu - 44.3f;
        left += size + 10;
        card(c, x - .7f, y, size * .9f, game_.lastLife.slots[i], now, 1);
        bool over = std::fabs(mx - x) <= size * .5f && std::fabs(my - y) <= size * .5f;
        deathTipK_[i] += ((over ? 1.f : 0.f) - deathTipK_[i]) * k7;
        tx[i] = x;
        ty[i] = y - size * .5f - 10 + slide;
    }
    for (int i = n; i < 16; i++) deathTipK_[i] = 0;
    c.setTransform(u, 0, 0, u, 0, 0);
    for (int i = 0; i < n; i++)
        if (deathTipK_[i] > .01f) petalTooltip(c, game_.lastLife.slots[i], tx[i], ty[i] + 12.5f, deathTipK_[i], Wu, Hu);
    c.globalAlpha(1);
    c.resetTransform();
}

// scores count up smoothly; each bar is the score against the first row's
static std::string shortNumber(double v) {
    char buf[32];
    double a = std::fabs(v);
    if (a < 1000)
        std::snprintf(buf, sizeof buf, "%.0f", v);
    else if (a < 1000000)
        std::snprintf(buf, sizeof buf, "%.1fk", v * .001);
    else
        std::snprintf(buf, sizeof buf, "%.1fm", v * .000001);
    return buf;
}

void GameUI::drawLeaderboard(Context &c, float Wu) {
    const sim::ArenaData *arena = sim_.arena();
    if (!arena) return;
    double now = game_.time;
    float dt = frameTime(lbLast_, now);
    int count = std::min(10, arena->leaderRows);
    float k = ease(.8f, dt);
    for (int i = 0; i < count; i++) {
        lbShown_[i] += float(double(arena->leaderScores[i]) - lbShown_[i]) * k;
    }
    float top = 20;
    if (game_.touchMode()) {
        // folded into a green square top right until tapped; it slides down as the square goes up
        c.save();
        c.globalAlpha(1);
        c.lineWidth(6);
        c.fillStyle(Color(0x55bb55));
        c.strokeStyle(Color(0x459745));
        roundRect(c, hudW() - 70, this->touch.boardSquareY(), 50, 50, 6);
        c.fill();
        c.stroke();
        c.restore();
        if (!this->touch.boardOpen() && this->touch.boardK() < .0001f) return;
        top = this->touch.boardK() * 316 - 296;
        Wu = hudW();
    }
    c.save();
    c.translate(Wu - 220, top);
    c.globalAlpha(1);
    c.lineCap(cp6::Cap::Round);
    c.lineWidth(6);
    c.fillStyle(Color(0x555555));
    c.strokeStyle(Color(0x454545));
    roundRect(c, 0, 0, 200, 276, 5);
    c.fill();
    c.stroke();
    for (int i = 0; i < count; i++) {
        float y = 59 + 22.f * i;
        float f = lbShown_[0] != 0 ? std::clamp(lbShown_[i] / lbShown_[0], 0.f, 1.f) : 1.f;
        if (std::isnan(f)) f = 1;
        c.lineWidth(20);
        c.strokeStyle(Color(0x444444));
        c.beginPath();
        c.moveTo(20, y);
        c.lineTo(180, y);
        c.stroke();
        c.lineWidth(16);
        c.strokeStyle(Color(0xfffc61));
        c.beginPath();
        c.moveTo(20, y);
        c.lineTo(20 + 160 * f, y);
        c.stroke();
        // names are cut to 13 bytes, on a character boundary
        std::string nm = arena->leaderNames[i];
        if (nm.size() > 13) {
            size_t n = 13;
            while (n > 0 && (uint8_t(nm[n]) & 0xc0) == 0x80) n--;
            nm.resize(n);
        }
        if (nm.empty()) nm = game_.tankServer() ? "Unnamed Tank" : "Unnamed Flower";
        std::string score = "- " + shortNumber(lbShown_[i]);
        // the name and the score read as one line centred on the bar
        c.font(13);
        float pad = float(int(13 * .24f));
        float x = 100 - (c.measureText(nm) + c.measureText(score) + 2 * pad) * .5f;
        outlinedText(c, nm, x + pad, y, 13, Color(0xffffff), Align::Left);
        outlinedText(c, score, x + c.measureText(nm) + 2 * pad, y, 13, Color(0xffffff), Align::Left);
    }
    c.lineWidth(6);
    c.fillStyle(Color(0x55bb55));
    c.strokeStyle(Color(0x459745));
    c.beginPath();
    c.moveTo(0, 40);
    c.lineTo(0, 5);
    c.quadraticCurveTo(0, 0, 5, 0);
    c.lineTo(195, 0);
    c.quadraticCurveTo(200, 0, 200, 5);
    c.lineTo(200, 40);
    c.closePath();
    c.fill();
    c.stroke();
    int players = int(arena->players);
    const char *kind =
        game_.tankServer() ? (players == 1 ? " Tank" : " Tanks") : (players == 1 ? " Flower" : " Flowers");
    outlinedText(c, std::to_string(players) + kind, 100, 20, 18);
    c.restore();
}

bool GameUI::nameBoxAt(float x, float y) const {
    if (boxAway() > .5f) return false;
    int ia = int(hudH());
    float cx = hudW() * .5f, bh = game_.mobile ? 70.f : 50.f, by = game_.mobile ? float(ia / 2) : float(ia / 4) + 200;
    return inBox(x, y, cx - 200, by, cx + 200, by + bh);
}

bool GameUI::playButtonAt(float x, float y) const {
    if (boxAway() > .5f) return false;
    int ia = int(hudH());
    float cx = hudW() * .5f, bh = game_.mobile ? 70.f : 50.f, by = game_.mobile ? float(ia / 2) : float(ia / 4) + 200;
    return inBox(x, y, cx + 210, by, cx + 210 + bh, by + bh);
}

namespace {
constexpr int kPickCols = 10;
constexpr float kPickCard = 56, kPickGap = 10, kPickPad = 20, kPickTitle = 36;
}  // namespace

int GameUI::pickerAt(float x, float y) const {
    int rows = (kServerPetals - 2) / kPickCols + 1;
    float w = kPickPad * 2 + kPickCols * kPickCard + (kPickCols - 1) * kPickGap;
    float h = kPickPad * 2 + kPickTitle + rows * kPickCard + (rows - 1) * kPickGap;
    float x0 = hudW() * .5f - w * .5f, y0 = 80;
    if (!inBox(x, y, x0, y0, x0 + w, y0 + h)) return -1;
    for (int t = 1; t < kServerPetals; t++) {
        float cx = x0 + kPickPad + float((t - 1) % kPickCols) * (kPickCard + kPickGap);
        float cy = y0 + kPickPad + kPickTitle + float((t - 1) / kPickCols) * (kPickCard + kPickGap);
        if (inBox(x, y, cx, cy, cx + kPickCard, cy + kPickCard)) return t;
    }
    return 0;
}

void GameUI::drawPicker(Context &c, double now) {
    int rows = (kServerPetals - 2) / kPickCols + 1;
    float w = kPickPad * 2 + kPickCols * kPickCard + (kPickCols - 1) * kPickGap;
    float h = kPickPad * 2 + kPickTitle + rows * kPickCard + (rows - 1) * kPickGap;
    float x0 = hudW() * .5f - w * .5f, y0 = 80, u = hudScale();
    int hover = pickerAt(game_.in.mx / u, game_.in.my / u);
    c.save();
    c.globalAlpha(.5f);
    c.fillStyle(Color(0x000000));
    roundRect(c, x0, y0, w, h, 8);
    c.fill();
    c.globalAlpha(1);
    outlinedText(c, "Pick a petal (P to close)", x0 + w * .5f, y0 + kPickPad + 10, 20);
    for (int t = 1; t < kServerPetals; t++) {
        float cx = x0 + kPickPad + float((t - 1) % kPickCols) * (kPickCard + kPickGap) + kPickCard * .5f;
        float cy = y0 + kPickPad + kPickTitle + float((t - 1) / kPickCols) * (kPickCard + kPickGap) + kPickCard * .5f;
        card(c, cx, cy, kPickCard, t, now, 1);
        if (t != hover) continue;
        c.lineWidth(3);
        c.strokeStyle(Color(0xffffff));
        c.strokeRect(cx - kPickCard * .5f - 4, cy - kPickCard * .5f - 4, kPickCard + 8, kPickCard + 8);
    }
    c.restore();
}

// on a tank server, the choice of a tank or a flower, between the logo and the name box
void GameUI::playKindBox(int kind, float &x, float &y, float &w, float &h) const {
    int ia = int(hudH());
    float cx = hudW() * .5f, q = float(ia / 4), by = game_.mobile ? float(ia / 2) : q + 200;
    float top = q + (game_.mobile ? 60 : 80) * .5f + 6, bottom = by - 31;
    h = std::min(44.f, bottom - top - 4);
    w = 130;
    y = (top + bottom - h) * .5f;
    x = kind == 0 ? cx - 6 - w : cx + 6;
}

int GameUI::playKindAt(float x, float y) const {
    if (boxAway() > .5f) return -1;
    for (int kind = 0; kind < 2; kind++) {
        float bx, by, w, h;
        playKindBox(kind, bx, by, w, h);
        if (inBox(x, y, bx, by, bx + w, by + h)) return kind;
    }
    return -1;
}

void GameUI::drawPlayKind(Context &c, double now) {
    for (int kind = 0; kind < 2; kind++) {
        float x, y, w, h;
        playKindBox(kind, x, y, w, h);
        uint32_t fill = (kind == 0) == game_.playTank ? 0x1dd129 : 0x999999;
        c.lineWidth(5);
        c.fillStyle(Color(fill));
        c.strokeStyle(Color(darker(fill)));
        roundRect(c, x, y, w, h, 6);
        c.fill();
        c.stroke();
        float ix = x + h * .55f, iy = y + h * .5f;
        if (kind == 0) {
            drawTankIcon(c, 0, ix, iy, -.785f, h * .0045f, true);
        } else {
            c.save();
            c.translate(ix, iy);
            c.scale(h * .012f);
            Look fl;
            fl.time = now;
            fl.ui = true;
            drawFlower(c, 25, fl, 0, 0, 5, 0);
            c.restore();
        }
        outlinedText(c, kind == 0 ? "Tank" : "Flower", x + h + (w - h) * .5f - 4, iy, 18);
    }
}

// the browser's own dialog, since a phone keyboard only comes up for a text field
void GameUI::askName() {
    char buf[256];
    int n = ui_prompt(game_.name.c_str(), buf, sizeof buf);
    if (n < 0) return;
    game_.name.assign(buf, size_t(n));
    ui_storage_set("florrio_nickname", game_.name.c_str());
}

}  // namespace game
