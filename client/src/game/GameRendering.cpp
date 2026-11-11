#include "game/GameRendering.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>
#include <vector>

#include "game/DrawTank.h"
#include "game/Ease.h"
#include "game/Game.h"
#include "game/Ui.h"
#include "TankData.h"

using namespace protocol;

namespace game {

using cp6::Color;
using cp6::Context;
using sim::Entity;

GameRendering::GameRendering(Game &game) : game_(game), sim_(game.simulation), ctx_(game.ctx) {}

// Position, angle and size glide to each new server value over 120 ms (see Lerp).
void GameRendering::smooth(Entity &e, float dt, double now) {
    float tx = e.position ? e.position->x : 0, ty = e.position ? e.position->y : 0;
    float ta = e.position ? e.position->angle : 0, tr = e.size ? e.size->radius : 0;
    if (e.tank) {
        sim::TankData &t = *e.tank;
        auto glide = [&](sim::Lerp &g, float v) {
            g.angle = true;
            if (e.placed)
                g.set(v, now);
            else
                g.snap(v, now);
        };
        for (size_t i = 0; i < t.turrets.size(); i++) glide(t.turretGlide[i], t.turrets[i]);
        glide(t.spinGlide, t.spin);
    }
    if (!e.placed) {
        e.lerpX.snap(tx, now);
        e.lerpY.snap(ty, now);
        e.lerpAngle.snap(ta, now);
        e.lerpR.snap(tr, now);
        e.x = tx;
        e.y = ty;
        e.angle = ta;
        e.r = tr;
        e.placed = true;
        // drops that were already there when they came into view show at once; new ones grow in
        if (e.drop && !e.fresh) e.drop->appear = e.drop->goingAway() ? 0.f : 1.f;
        return;
    }
    e.lerpX.set(tx, now);
    e.lerpY.set(ty, now);
    e.lerpAngle.set(ta, now);
    e.lerpR.set(tr, now);
    e.x = e.lerpX.at(now);
    e.y = e.lerpY.at(now);
    e.angle = e.lerpAngle.at(now);
    e.r = e.lerpR.at(now);
    e.hurt = std::clamp(1 - float(now - e.hurtAt) / 150.f, 0.f, 1.f);
    if (e.flower) {
        sim::FlowerData &f = *e.flower;
        float mh = e.maxHp();
        float hp = mh > 0 ? e.hp() / mh : 1;
        // the bar slides to the new health; the red part follows once the bar has got there. It shows
        // while hurt and alive.
        float k8 = ease(.8f, dt);
        float showBar = hp < 1 && e.hp() > 0 ? 1.f : 0.f;
        if (!e.drawn) {
            f.healthBar = f.healthTrail = hp;
            f.healthAlpha = showBar;
        } else {
            f.healthBar += (hp - f.healthBar) * k8;
            if (f.healthBar - hp < .001f) f.healthTrail += (hp - f.healthTrail) * ease(.9f, dt);
            f.healthAlpha += (showBar - f.healthAlpha) * .2f;  // a fifth each frame
        }
        int status = e.status ? e.status->flags : 0;
        bool dead = e.hp() <= 0, glum = dead || (status & 7);
        float k6 = ease(.6f, dt);
        float mouth = glum || f.attacking() ? -5.f : f.defending() ? -3.f : 5.f;
        f.mouth += (mouth - f.mouth) * k6;
        if (!glum && f.attacking())
            f.angry += (1 - f.angry) * k6;
        else
            f.angry -= f.angry * k8;
        f.purple += ((status & 1 ? 1.f : 0.f) - f.purple) * k8;
        f.light += ((status & 2 ? 1.f : 0.f) - f.light) * k8;
        float eyes = float(f.eyes) * .02464f;
        f.lookX += (std::cos(eyes) - f.lookX) * k8;
        f.lookY += (std::sin(eyes) - f.lookY) * k8;
        f.nameAlpha += ((dead ? 0.f : 1.f) - f.nameAlpha) * ease(.7f, dt);
    }
    if (e.tank && !e.shot) {
        sim::TankData &t = *e.tank;
        if (e.hp() < e.maxHp())
            t.bar = std::min(1.f, t.bar + dt / 50);
        else
            t.bar = std::max(0.f, t.bar - dt / 300);
    }
    if (e.drop) {
        float target = e.drop->goingAway() ? 0.f : 1.f;
        e.drop->appear += (target - e.drop->appear) * ease(0.8f, dt);
    }
    if (e.mob) {
        sim::MobData &m = *e.mob;
        m.animT += dt;
        float to = e.moves && e.moves->chasing() ? 1.f : 0.f;
        m.chaseK += (to - m.chaseK) * ease(.7f, dt);
        if (dt > 0) {
            if (m.walkSeen)
                m.walkSpeed += (std::hypot(e.x - m.walkX, e.y - m.walkY) / dt - m.walkSpeed) * ease(.6f, dt);
            m.walkSeen = true;
            m.walkX = e.x;
            m.walkY = e.y;
            m.walk += double(m.walkSpeed * 4 * dt);
        }
    }
}

// Your own flower leans up to 5 units toward where it is steering (fully from 100 units out),
// held inside the map, and the view goes with it. For 200 ms after it is hit the view shakes.
// With a flower to follow the view sits right on its spot; without one it drifts there.
void GameRendering::updateCamera(sim::CameraData *cam, Entity *p, float dt, double now) {
    if (!cam) return;
    const sim::ArenaData *arena = sim_.arena();
    float mapW = arena ? arena->width : 0, mapH = arena ? arena->height : 0;
    auto place = [&](float bx, float by) {
        p->x = bx + leanX_;
        p->y = by + leanY_;
        if (mapW > 0)
            p->x = std::clamp(p->x, std::min(p->r - mapW * .5f, mapW * .5f - p->r),
                              std::max(p->r - mapW * .5f, mapW * .5f - p->r));
        if (mapH > 0)
            p->y = std::clamp(p->y, std::min(p->r - mapH * .5f, mapH * .5f - p->r),
                              std::max(p->r - mapH * .5f, mapH * .5f - p->r));
    };
    if (p) {
        float bx = p->x, by = p->y;
        place(bx, by);
        if (game_.screen == Screen::Playing && !game_.touchMode() && !game_.tankMode()) {
            float a = 0, z = 0;
            if (game_.settings.keyboardControl) {
                float dx = float(game_.in.rightKey) - float(game_.in.leftKey),
                      dy = float(game_.in.down) - float(game_.in.up);
                if (dx || dy) {
                    a = std::atan2(dy, dx);
                    z = 1;
                }
            } else {
                float s = worldScale();
                float dx = camX_ + (game_.in.mx - ctx_.width() * .5f) / s - p->x,
                      dy = camY_ + (game_.in.my - ctx_.height() * .5f) / s - p->y;
                a = std::atan2(dy, dx);
                z = std::min(1.f, std::hypot(dx, dy) * .01f);
            }
            if (!arena || !arena->running()) z = 0;
            float k = ease(.95f, dt);
            leanX_ += (std::cos(a) * z * 5 - leanX_) * k;
            leanY_ += (std::sin(a) * z * 5 - leanY_) * k;
            place(bx, by);
        }
    }
    if (sim_.cameraId != camId_) {
        camId_ = sim_.cameraId;
        camLx_.snap(cam->x, now);
        camLy_.snap(cam->y, now);
    } else {
        camLx_.set(cam->x, now);
        camLy_.set(cam->y, now);
    }
    shakeK_ += ((now - hurtAt_ < 200 ? 1.f : 0.f) - shakeK_) * ease(.8f, dt);
    float amp = float(std::sin(now * .05)) * 5 * shakeK_, sa = float(now * .01);
    float tx = camLx_.at(now) + leanX_ + std::cos(sa) * amp, ty = camLy_.at(now) + leanY_ + std::sin(sa) * amp;
    float k = p ? 1.f : ease(.8f, dt);
    camX_ += (tx - camX_) * k;
    camY_ += (ty - camY_) * k;
    float fov = (cam->fov > 0 ? cam->fov : 1) * (game_.mobile && game_.settings.mobileZoom ? 1.6f : 1.f);
    fov_ += (fov - fov_) * ease(.8f, dt);
}

void GameRendering::drawEntity(Context &c, Entity &e, double now, float alpha, float grow) {
    if (e.drop) {
        // a card lying in the world: spins in as it appears and gently pulses
        float ap = e.drop->appear;
        if (ap <= .001f || e.drop->collected) return;
        c.save();
        c.globalAlpha(alpha * std::sqrt(ap));
        c.translate(e.x, e.y);
        c.scale(grow * ap * (1 + .03f * std::sin(float(now - e.born) * .01f)) * (5 / 7.f));
        float tilt = (int(e.id * 2654435761u >> 16 & 1023) / 1023.f - .5f) * .2f;
        c.rotate(ap * 12.566371f + tilt);
        float a0 = c.globalAlpha();
        c.globalAlpha(a0 * .2f);
        c.lineWidth(5.833f);
        c.strokeStyle(Color(0x000000));
        c.strokeRect(-36, -36, 72, 72);
        c.globalAlpha(a0);
        card(c, 0, 0, 70, e.drop->type, now, a0);
        c.restore();
        return;
    }
    Look l;
    l.hurt = e.hurt;
    l.id = e.id;
    l.time = now;
    if (e.mob) {
        l.anim = e.mob->animT;
        l.chase = e.mob->chaseK;
        l.walk = e.mob->walk;
    }
    c.save();
    c.globalAlpha(alpha);
    c.translate(e.x, e.y);
    c.rotate(e.angle);
    c.scale(grow);
    // in this order: a missile or a pea in flight, a flower, a petal, a mob
    if (e.has(C_MISSILE)) {
        c.scale(1.3f);
        drawPetal(c, P_MISSILE, l);
    } else if (e.pea) {
        drawPea(c, e.pea->poison(), e.pea->big(), l);
    } else if (e.flower) {
        const sim::FlowerData &f = *e.flower;
        l.dead = e.hp() <= 0;
        l.purple = f.purple;
        l.light = f.light;
        l.antennae = f.antennae();
        l.lumpy = f.lumpy == 1;
        c.rotate(-e.angle);
        drawFlower(c, e.r, l, f.lookX, f.lookY, f.mouth, f.angry);
    } else if (e.petal) {
        drawPetal(c, e.petal->type, l);
    } else if (e.mob) {
        drawMob(c, e.mob->type, e.r, l);
    } else if (e.size) {
        c.fillStyle(Color(0xff00ff));
        c.fillCircle(0, 0, e.r);
    }
    c.restore();
}

// diep's way out: each tick for five ticks it grows by a tenth and fades by a sixth
float GameRendering::deathAlpha(const Entity &e, double now) const {
    if (e.deadAt < 0) return 1;
    float t = float(now - e.deadAt) / 40.f;
    return std::max(0.f, t < 1 ? 1 - t * 2 / 6 : (5 - t) / 6);
}

// a tank, a boss or a shot
void GameRendering::drawDiep(Context &c, Entity &e, double now) {
    float alpha = deathAlpha(e, now);
    if (alpha <= 0) return;
    float grow = e.deadAt < 0 ? 1.f : std::pow(1.1f, std::min(5.f, float(now - e.deadAt) / 40.f));
    if (e.mob) {
        c.save();
        c.recolor(tankColor(COLOR_SQUARE, false), 1);
        drawEntity(c, e, now, alpha, grow);
        c.restore();
        return;
    }
    TankPaint p;
    std::array<float, 8> turrets{};
    p.now = now;
    p.ticks = now / 40;
    p.angle = e.angle;
    p.flash = std::clamp((120 - float(now - e.hurtAt)) / 120, 0.f, 1.f);
    uint32_t team = e.shot ? e.shot->team : e.tank->team;
    int color = e.shot ? e.shot->color : e.tank->color;
    p.fill = tankColor(color, team && team == sim_.cameraId);
    if (e.tank) {
        sim::TankData &t = *e.tank;
        if (!t.firedSeen) {
            t.seenFired = t.fired;
            t.firedSeen = true;
        }
        uint32_t flip = t.fired ^ t.seenFired;
        for (int i = 0; i < 24; i++)
            if (flip >> i & 1) t.kickAt[i] = now;
        t.seenFired = t.fired;
        p.kickAt = t.kickAt.data();
        for (size_t i = 0; i < turrets.size(); i++) turrets[i] = t.turretGlide[i].at(now);
        p.turrets = turrets.data();
        p.spin = t.spinGlide.at(now);
        p.blink = t.spawning() && (long((now - e.born) / 100) & 1);
        alpha *= t.alpha / 255.f;
        sim::CameraData *cam = sim_.camera();
        bool mine = cam && cam->player == e.id && !e.shot;
        if (mine && e.deadAt < 0 && game_.screen == Screen::Playing) p.angle = game_.tankAngle;
        if (mine) {
            Entity *ce = sim_.get(sim_.cameraId);
            if (ce && ce->driver) p.reload = 15 * std::pow(.914f, float(ce->driver->stats[tankdata::S_RELOAD]));
        } else if (t.layout >= tankdata::L_GUARDIAN) {
            p.reload = 15 * std::pow(.914f, 7.f);
        } else if (t.layout == tankdata::L_MINION || t.layout == tankdata::L_ROCKET) {
            p.reload = 1;
        }
    }
    if (alpha <= 0) return;
    const tankdata::TankDef *def = e.tank ? tankdata::layout(e.tank->layout) : nullptr;
    bool layer = alpha < 1 && def;
    c.save();
    c.translate(e.x, e.y);
    if (layer)
        c.beginLayer();
    else
        c.globalAlpha(alpha);
    if (def)
        drawTank(c, e.tank->layout, e.r * grow / def->baseSize, p);
    else
        drawShot(c, e.shot->look, e.shot->kind == tankdata::B_TRAP, e.r * grow, p);
    if (layer) c.endLayer(alpha);
    c.restore();
}

void GameRendering::drawWorld(Context &c, double now, float dt) {
    float W = float(c.width()), H = float(c.height());
    float s = worldScale();
    const sim::ArenaData *arena = sim_.arena();
    float mapW = arena && arena->width > 0 ? arena->width : 50000,
          mapH = arena && arena->height > 0 ? arena->height : 4000;
    auto sx = [&](float x) { return W * .5f + (x - camX_) * s; };
    auto sy = [&](float y) { return H * .5f + (y - camY_) * s; };
    // zones: easy, medium, hard and the end, by fractions of the map width
    const float edges[5] = {-.5f, -.5f + .3f, -.5f + .6f, -.5f + .9f, .5f};
    const uint32_t cols[4] = {0x1ea761, 0xdecf7c, 0xb06655, 0x4d5e56};
    for (int i = 0; i < 4; i++) {
        float x0 = i == 0 ? 0 : std::max(0.f, sx(edges[i] * mapW));
        float x1 = i == 3 ? W : std::min(W, sx(edges[i + 1] * mapW));
        if (x1 > x0) {
            c.fillStyle(Color(cols[i]));
            c.fillRect(x0, 0, x1 - x0, H);
        }
    }
    // darken outside the map; its edges grow out from the middle when it first shows, a fifth of
    // the way each frame
    if (arena) {
        bounds_[0] = (bounds_[0] * 4 - mapW * .5f) * .2f;
        bounds_[1] = (bounds_[1] * 4 + mapW * .5f) * .2f;
        bounds_[2] = (bounds_[2] * 4 - mapH * .5f) * .2f;
        bounds_[3] = (bounds_[3] * 4 + mapH * .5f) * .2f;
    }
    c.globalAlpha(.1f);
    c.fillStyle(Color(0x000000));
    float L = sx(bounds_[0]), R = sx(bounds_[1]), T = sy(bounds_[2]), B = sy(bounds_[3]);
    if (arena) {
        if (T > 0) c.fillRect(0, 0, W, T);
        if (B < H) c.fillRect(0, B, W, H - B);
        if (L > 0) c.fillRect(0, std::max(0.f, T), L, std::min(H, B) - std::max(0.f, T));
        if (R < W) c.fillRect(R, std::max(0.f, T), W - R, std::min(H, B) - std::max(0.f, T));
    }
    // and the zones the flower has outgrown
    Entity *me = sim_.get(game_.player);
    float edge = std::max(-mapW * .5f, me && me->flower ? zoneEdge(me->flower->level) : -INFINITY);
    if (!edgePlaced_) {
        edgeShown_ = -mapW * .5f;
        edgePlaced_ = true;
    }
    edgeShown_ += (edge - edgeShown_) * ease(.8f, dt);
    float zl = std::max(0.f, sx(-mapW * .5f)), zr = std::min(W, sx(edgeShown_)), zt = std::max(0.f, sy(-mapH * .5f)),
          zb = std::min(H, sy(mapH * .5f));
    if (zr > zl && zb > zt) c.fillRect(zl, zt, zr - zl, zb - zt);
    c.globalAlpha(1);
    // grid
    float g = 50 * s;
    if (g >= 2) {
        c.globalAlpha(std::min(1.f, .1f * s));  // darker when zoomed in
        c.strokeStyle(Color(0x000000));
        c.lineWidth(1);
        c.grid(0, 0, W, H, g, std::fmod(W * .5f - camX_ * s, g) + .5f, std::fmod(H * .5f - camY_ * s, g) + .5f);
        c.globalAlpha(1);
    }

    // drawn in passes: webs on the ground; flowers' health bars and names; drops; then everything
    // else, flowers on top, mobs that move above those that don't, petals above the rest, by slot
    std::vector<Entity *> list;
    list.reserve(sim_.entities.size());
    for (auto &kv : sim_.entities)
        if (kv.second.has(C_POSITION) && kv.second.placed) list.push_back(&kv.second);
    std::sort(list.begin(), list.end(), [](Entity *a, Entity *b) {
        auto key = [](const Entity *e) {
            return std::make_tuple(e->has(C_TANK) && !e->has(C_SHOT), e->has(C_SHOT), e->has(C_FLOWER), e->has(C_MOVES),
                                   e->has(C_PETAL), !e->has(C_DROP));
        };
        auto ka = key(a), kb = key(b);
        return ka != kb ? ka < kb : (a->id & 16383) < (b->id & 16383);
    });
    c.setTransform(s, 0, 0, s, W * .5f - camX_ * s, H * .5f - camY_ * s);
    cp6::Matrix base = c.matrix();
    auto reset = [&] { c.setTransform(base.a, base.b, base.c, base.d, base.e, base.f); };
    drawMovementHelper(c, dt);
    auto fade = [&](const Entity *e) { return e->deadAt < 0 ? 0.f : std::min(1.f, float(now - e->deadAt) / 150.f); };
    // skipped when more than 100 units off screen
    float vl = camX_ - W * .5f / s - 100, vr = camX_ + W * .5f / s + 100, vt = camY_ - H * .5f / s - 100,
          vb = camY_ + H * .5f / s + 100;
    auto onScreen = [&](const Entity *e) {
        return !e->has(C_SIZE) || (e->x + e->r >= vl && e->x - e->r <= vr && e->y + e->r >= vt && e->y - e->r <= vb);
    };
    for (Entity *e : list) {
        if (!e->web || !onScreen(e)) continue;
        float &grow = e->web->grow;
        // spreads out when new; one already there when it came into view shows at once
        if (!e->drawn && !e->fresh)
            grow = 1;
        else
            grow += (1 - grow) * ease(.7f, dt);
        e->drawn = true;
        float t = fade(e);
        if (t >= 1) continue;
        reset();
        c.save();
        c.globalAlpha((1 - t) * .5f * grow);
        c.translate(e->x, e->y);
        c.rotate(e->angle);
        c.scale((1 + t) * grow);
        drawWeb(c);
        c.restore();
    }
    // only flowers have a health bar: 70 units wide, 50 below the middle
    for (Entity *e : list) {
        if (!e->flower || e->flower->healthAlpha <= .001f) continue;
        const sim::FlowerData &f = *e->flower;
        float a = std::min(1.f, f.healthAlpha);
        auto bar = [&](float to, float width, uint32_t col, float alpha) {
            c.globalAlpha(alpha);
            c.lineWidth(width);
            c.strokeStyle(Color(col));
            c.beginPath();
            c.moveTo(-35, 50);
            c.lineTo(to * 70 - 35, 50);
            c.stroke();
        };
        reset();
        c.translate(e->x, e->y);
        c.lineCap(cp6::Cap::Round);
        bar(1, 10, 0x333333, a);
        if (f.healthTrail > 0) bar(f.healthTrail, 6, 0xdd3434, a * std::min(1.f, f.healthTrail * 30));
        if (f.healthBar > 0) bar(f.healthBar, 7, 0x75dd34, a * std::min(1.f, f.healthBar * 30));
    }
    // other flowers' names
    for (Entity *e : list) {
        if (!e->flower || e->id == game_.player || !e->name || e->name->name.empty() || e->flower->nameAlpha <= .001f)
            continue;
        reset();
        c.globalAlpha(e->flower->nameAlpha);
        c.translate(e->x, e->y);
        outlinedText(c, e->name->name, 0, -45, 12);
    }
    c.globalAlpha(1);
    for (Entity *e : list) {
        if (!e->drop) continue;
        reset();
        drawEntity(c, *e, now, 1, 1);
    }
    Entity *viewer = sim_.flower();
    for (Entity *e : list) {
        if (e->web || e->drop || !onScreen(e)) continue;
        e->drawn = true;
        // mobs count as seen once drawn within 900 units of a living flower
        if (e->mob && viewer && viewer->hp() > 0 && std::hypot(e->x - viewer->x, e->y - viewer->y) < 900)
            game_.ui.menus.discoverMob(e->mob->type);
        reset();
        if (e->tank || e->shot) {
            drawDiep(c, *e, now);
            continue;
        }
        float t = fade(e);
        if (t >= 1) continue;
        drawEntity(c, *e, now, 1 - t, 1 + t);
    }
    // tanks' names, then their health bars
    sim::CameraData *cam = sim_.camera();
    uint32_t mine = cam ? cam->player : 0;
    for (Entity *e : list) {
        if (!e->tank || e->shot || !e->name || e->name->name.empty() || e->id == mine) continue;
        sim::TankData &t = *e->tank;
        t.nameFade += ((onScreen(e) ? 1.f : 0.f) - t.nameFade) * ease(onScreen(e) ? .8f : .9f, dt);
        float a = t.alpha / 255.f * deathAlpha(*e, now) * t.nameFade;
        if (a < .01f) continue;
        const tankdata::TankDef *def = tankdata::layout(t.layout);
        reset();
        drawTankName(c, e->name->name, t.score, e->x, e->y, e->r, def ? def->sides : 1, a);
    }
    for (Entity *e : list) {
        if (!e->tank || e->shot || !e->health || !onScreen(e)) continue;
        sim::TankData &t = *e->tank;
        float a = (e->id == mine ? 1.f : t.alpha / 255.f * deathAlpha(*e, now)) * t.bar;
        if (a < .01f) continue;
        reset();
        drawTankBar(c, e->x, e->y, e->r, e->maxHp() > 0 ? e->hp() / e->maxHp() : 0, a);
    }
    c.globalAlpha(1);
    c.resetTransform();
}

// the camera's zoom (on phones x1.6 unless the setting is off, see updateCamera)
float GameRendering::worldScale() const {
    return fov_ * std::max(ctx_.width() / 1920.f, ctx_.height() / 1080.f);
}

// the left edge of the area a flower of this level may play in: zones open by level, and in a
// closing arena the playable area shrinks from the left over time
float GameRendering::zoneEdge(int level) const {
    const sim::ArenaData *arena = sim_.arena();
    if (!arena) return -INFINITY;
    float W = arena->width, edge;
    if (arena->closing())
        edge = -W * .5f + W * std::clamp(arena->time * 1e-6f * 1.1f - .1f, 0.f, 1.f);
    else if (level >= 15)
        edge = -W * .5f + W * (level < 30 ? .3f : level < 45 ? .6f : .9f);
    else
        return -INFINITY;
    return std::round(edge * .02f) * 50;
}

// a faint arrow from the flower to the cursor, showing where mouse movement is taking it
void GameRendering::drawMovementHelper(Context &c, float dt) {
    if (!game_.settings.movementHelper || game_.settings.keyboardControl || game_.touchMode()) return;
    if (!sim_.camera()) return;
    Entity *me = sim_.flower();
    if (me) {
        helperFromX_ = me->x;
        helperFromY_ = me->y;
    }
    bool alive = me && me->hp() > 0;
    helperAlpha_ += ((alive ? 1.f : 0.f) - helperAlpha_) * ease(.7f, dt);
    float W = float(c.width()), H = float(c.height());
    float s = worldScale();
    float wx = camX_ + (game_.in.mx - W * .5f) / s, wy = camY_ + (game_.in.my - H * .5f) / s;
    float k = ease(.6f, dt);
    helperX_ += (wx - helperX_) * k;
    helperY_ += (wy - helperY_) * k;
    // the tip is pulled toward the middle of the map while it fades
    float f = std::clamp(helperAlpha_, 0.f, 1.f);
    float tx = helperX_ * f, ty = helperY_ * f;
    float dx = tx - helperFromX_, dy = ty - helperFromY_;
    float a = std::atan2(dy, dx), dist = std::hypot(dx, dy);
    float alpha = std::min(1.f, .2f * helperAlpha_) * std::clamp(dist * .02f - 2, 0.f, 1.f);
    if (alpha <= 0) return;
    float wing = 50 * helperAlpha_;
    c.save();
    c.globalAlpha(alpha);
    c.lineWidth(20);
    c.lineCap(cp6::Cap::Round);
    c.strokeStyle(Color(0x333333));
    c.beginPath();
    c.moveTo(helperFromX_ + std::cos(a) * 50, helperFromY_ + std::sin(a) * 50);
    c.lineTo(tx, ty);
    c.lineTo(tx + std::cos(a + 2.513274f) * wing, ty + std::sin(a + 2.513274f) * wing);
    c.lineTo(tx, ty);
    c.lineTo(tx + std::cos(a - 2.513274f) * wing, ty + std::sin(a - 2.513274f) * wing);
    c.stroke();
    c.restore();
}

}  // namespace game
