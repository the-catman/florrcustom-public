#include "game/TouchControls.h"

#include <algorithm>
#include <cmath>

#include "game/Ease.h"
#include "game/Game.h"
#include "game/Ui.h"

namespace game {

using cp6::Align;
using cp6::Baseline;
using cp6::Color;
using cp6::Context;
using sim::Entity;

static constexpr float TAU = 6.28318531f;

TouchControls::TouchControls(Game &game) : game_(game) {}

void TouchControls::onTouch(int phase, int id, float x, float y) {
    float u = game_.ui.hudScale(), hx = x / u, hy = y / u;
    if (phase == 0) {
        Touch t{id, x, y, x, y, R_NONE};
        if (game_.screen == Screen::Title) {
            // one finger at a time works the title screen like a mouse
            bool busy = false;
            for (const Touch &o : touches_) busy |= o.role == R_UI;
            if (!busy) {
                t.role = R_UI;
                game_.in.mx = x;
                game_.in.my = y;
                game_.onButton(0, true);
                game_.ui.menus.onFingerDown(id, hx, hy, y);
            }
        } else if (game_.screen == Screen::Dead) {
            if (game_.time - game_.deadAt > 500) {
                game_.connection.send(protocol::MSG_CONTINUE);
                game_.screen = Screen::Title;
            }
        } else {
            float Wu = game_.ui.hudW(), ia = game_.ui.hudH(), by = boardSquareY();
            bool onA = inBox(hx, hy, Wu - 310, ia - 230, Wu - 150, ia - 70),
                 onB = inBox(hx, hy, Wu - 130, ia - 130, Wu - 30, ia - 30);
            bool onBoard = boardOpen_ ? inBox(hx, hy, Wu - 220, boardK_ * 316 - 296, Wu - 20, boardK_ * 316 - 20)
                                      : inBox(hx, hy, Wu - 70, by, Wu - 20, by + 50);
            if (touchAlpha_ > .0001f && (onA || onB))
                t.role = R_BUTTON;
            else if (onBoard && game_.simulation.arena())
                t.role = R_BOARD;
            else if ((game_.tankMode() && game_.ui.overTankHud(x, y)) ||
                     (game_.ui.inventory.slotAt(x, y) >= 0 && !game_.ui.inventory.dragging())) {
                t.role = R_SLOT;
                game_.in.mx = x;
                game_.in.my = y;
                game_.onButton(0, true);
            } else if (joyId_ < 0) {
                t.role = R_JOY;
                joyId_ = id;
                joyX_ = hx;
                joyY_ = hy;
            }
        }
        touches_.push_back(t);
        return;
    }
    auto it = std::find_if(touches_.begin(), touches_.end(), [&](const Touch &t) { return t.id == id; });
    if (it == touches_.end()) return;
    it->x = x;
    it->y = y;
    if (it->role == R_SLOT || it->role == R_UI) {
        game_.in.mx = x;
        game_.in.my = y;
    }
    game_.ui.menus.onFingerMove(id, y);
    if (phase == 1) return;
    game_.ui.menus.onFingerUp(id);
    Touch t = *it;
    touches_.erase(it);
    if (t.role == R_JOY)
        joyId_ = -1;
    else if (t.role == R_BOARD) {
        float Wu = game_.ui.hudW(), by = boardSquareY();
        bool still = boardOpen_ ? inBox(hx, hy, Wu - 220, boardK_ * 316 - 296, Wu - 20, boardK_ * 316 - 20)
                                : inBox(hx, hy, Wu - 70, by, Wu - 20, by + 50);
        if (still) boardOpen_ = !boardOpen_;
    } else if (t.role == R_SLOT || t.role == R_UI) {
        bool took = game_.onButton(0, false);
        if (t.role == R_UI && !took && game_.screen == Screen::Title) {
            float sx = t.x0 / u, sy = t.y0 / u;
            if (game_.mobile && game_.ui.nameBoxAt(sx, sy) && game_.ui.nameBoxAt(hx, hy))
                game_.ui.askName();
            else if (game_.ui.playButtonAt(sx, sy) && game_.ui.playButtonAt(hx, hy))
                game_.onEnter();
        }
        game_.in.mx = game_.in.my = -1e5f;  // nothing stays hovered once the finger is gone
    }
}

void TouchControls::update(double now) {
    float dt = frameTime(touchLast_, now);
    if (!game_.touchMode()) {  // without touch controls the leaderboard is always out, and stays out if they come on
        boardOpen_ = true;
        boardK_ = 1;
        touches_.clear();
        joyId_ = -1;
        aDown_ = bDown_ = false;
        joyStrength_ = 0;
        return;
    }
    float u = game_.ui.hudScale(), Wu = game_.ui.hudW(), ia = game_.ui.hudH();
    Entity *me = game_.simulation.get(game_.player);
    bool alive = me && (me->flower || me->tank) && game_.screen == Screen::Playing;
    touchAlpha_ += ((alive ? 1.f : 0.f) - touchAlpha_) * ease(.7f, dt);
    boardK_ += ((boardOpen_ ? 1.f : 0.f) - boardK_) * ease(.8f, dt);
    // attack and defend are held while a finger that started on a button is over it
    aDown_ = bDown_ = false;
    for (const Touch &t : touches_) {
        if (t.role != R_BUTTON) continue;
        float x = t.x / u, y = t.y / u;
        aDown_ |= inBox(x, y, Wu - 310, ia - 230, Wu - 150, ia - 70);
        bDown_ |= inBox(x, y, Wu - 130, ia - 130, Wu - 30, ia - 30);
    }
    if (!alive) aDown_ = bDown_ = false;
    // when the joystick finger lifts, another finger already down that is not on a control takes over
    const Touch *jt = nullptr;
    for (Touch &t : touches_) {
        if (joyId_ < 0 && t.role == R_NONE && game_.screen == Screen::Playing) {
            t.role = R_JOY;
            joyId_ = t.id;
            joyX_ = t.x / u;
            joyY_ = t.y / u;
        }
        if (t.id == joyId_) jt = &t;
    }
    if (!jt) joyId_ = -1;
    if (jt && alive) {
        // the stick reaches full strength 135 units out; past that the base follows the finger,
        // unless it is fixed
        float fx = jt->x / u, fy = jt->y / u, d = std::hypot(fx - joyX_, fy - joyY_), k;
        if (d > 135 && !game_.settings.fixedJoystick) {
            joyX_ = fx + (joyX_ - fx) * 135 / d;
            joyY_ = fy + (joyY_ - fy) * 135 / d;
            k = 1.5f;
        } else {
            k = d / 90;
        }
        joyStrength_ = k * k * 1.1f;
        joyAngle_ = std::atan2(fy - joyY_, fx - joyX_);
    } else {
        joyStrength_ = 0;
    }
    joyAlpha_ += ((jt && alive ? 1.f : 0.f) - joyAlpha_) * ease(.6f, dt);
}

// the attack and defend buttons and the joystick base, all dark see-through circles
void TouchControls::draw(Context &c, float Wu, float ia) {
    if (touchAlpha_ < .0001f) return;
    float a = touchAlpha_ * .6f;
    auto button = [&](const char *label, float x, float y, float r) {
        c.globalAlpha(a);
        c.fillStyle(Color(0x000000));
        c.beginPath();
        c.arc(x, y, r, 0, TAU);
        c.fill();
        c.font(48);
        c.textAlign(Align::Center);
        c.textBaseline(Baseline::Middle);
        c.fillStyle(Color(0xffffff));
        c.fillText(label, x, y);
    };
    button("A", Wu - 230, ia - 150, 80);
    button("B", Wu - 80, ia - 80, 50);
    if (joyAlpha_ > .0001f) {
        c.globalAlpha(a * joyAlpha_);
        c.fillStyle(Color(0x000000));
        c.beginPath();
        c.arc(joyX_, joyY_, 90, 0, TAU);
        c.fill();
    }
    c.globalAlpha(1);
}

}  // namespace game
