#include "game/Game.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace protocol;

extern "C" {
void ui_storage_set(const char *key, const char *val);
int ui_device();
double ui_wheel();
}

namespace game {
using sim::Entity;

Game::Game() : rendering(*this), ui(*this) {}

bool Game::tankServer() const {
    const Entity *cam = simulation.get(simulation.cameraId);
    return cam && cam->driver;
}

// a tank's keys: E and C toggle firing and spinning, the number keys put a point into a stat
// (1 is the last one), and with test keys on, K levels up, backslash goes back a tank, O dies and
// B brings a boss
void Game::onTankKey(const char *code) {
    if (!simulation.flower()) return;
    if (!std::strcmp(code, "KeyE")) {
        autoFire = !autoFire;
        ui.notify(autoFire ? "Auto Fire: ON" : "Auto Fire: OFF", 0x0000ff, 5000, "auto_fire");
    } else if (!std::strcmp(code, "KeyC")) {
        autoSpin = !autoSpin;
        if (autoSpin) spinPhase_ = tankAngle - time / 1000;
        ui.notify(autoSpin ? "Auto Spin: ON" : "Auto Spin: OFF", 0x0000ff, 5000, "auto_spin");
    } else if (!std::strncmp(code, "Digit", 5) && code[5] >= '1' && code[5] <= '8' && !code[6]) {
        ui.spendStat(8 - (code[5] - '0'));
    } else if (devKeys) {
        static const char *kDev[3] = {"KeyK", "Backslash", "KeyO"};
        for (int i = 0; i < 3; i++)
            if (!std::strcmp(code, kDev[i])) sendDev(i);
    }
}

bool Game::onFlowerDevKey(const char *code) {
    if (!devKeys) return false;
    if (!std::strcmp(code, "KeyP")) {
        ui.togglePetalPicker();
    } else if (!std::strcmp(code, "Escape") && ui.petalPickerOpen()) {
        ui.togglePetalPicker();
    } else if (!std::strcmp(code, "KeyK")) {
        sendDev(0);
    } else if (!std::strcmp(code, "KeyO")) {
        sendDev(2);
    } else {
        return false;
    }
    return true;
}

void Game::sendDev(int what, int type) {
    net::Writer w;
    w.u8(uint8_t(what));
    if (type >= 0) w.u8(uint8_t(type));
    connection.send(MSG_DEV, w.b);
}

bool Game::init() {
    if (!ctx.init()) return false;
    int device = ui_device();
    mobile = device > 0;
    tablet = device == 2;
    connection.open();
    ui.menus.loadStorage();
    return true;
}

void Game::onKey(const char *key, const char *code, bool down) {
    bool k = down;
    if (!std::strcmp(code, "KeyW") || !std::strcmp(code, "ArrowUp"))
        in.up = k;
    else if (!std::strcmp(code, "KeyS") || !std::strcmp(code, "ArrowDown"))
        in.down = k;
    else if (!std::strcmp(code, "KeyA") || !std::strcmp(code, "ArrowLeft"))
        in.leftKey = k;
    else if (!std::strcmp(code, "KeyD") || !std::strcmp(code, "ArrowRight"))
        in.rightKey = k;
    else if (!std::strcmp(code, "Space"))
        in.space = k;
    else if (!std::strcmp(code, "ShiftLeft") || !std::strcmp(code, "ShiftRight"))
        in.shift = k;
    else if (!std::strcmp(code, "KeyO"))
        in.keyO = k;
    else if (!std::strcmp(code, "KeyL"))
        in.keyL = k;
    else if (!std::strcmp(code, "KeyJ"))
        in.keyJ = k;
    if (!std::strcmp(code, "KeyY")) in.keyY = k;
    if (!std::strcmp(code, "KeyU")) in.keyU = k;
    if (!std::strcmp(code, "KeyM")) in.keyM = k;
    if (!down) return;
    if (!std::strcmp(code, "KeyY"))
        keyY_ = true;
    else if (!std::strcmp(code, "KeyI"))
        keyI_ = true;
    else if (!std::strcmp(code, "Semicolon"))
        ui.toggleStats();
    if (screen == Screen::Title) {
        if (!std::strcmp(key, "Enter")) {
            onEnter();
            return;
        }
        if (!std::strcmp(key, "Backspace")) {
            while (!name.empty() && (name.back() & 0xc0) == 0x80) name.pop_back();
            if (!name.empty()) name.pop_back();
            ui_storage_set("florrio_nickname", name.c_str());
            return;
        }
        size_t n = std::strlen(key);
        bool printable = n == 1 ? (unsigned char)key[0] >= 32 : (n > 1 && (unsigned char)key[0] >= 0xc0 && n <= 4);
        // typed names hold up to 13 bytes
        if (printable && name.size() + n <= 13) {
            name += key;
            ui_storage_set("florrio_nickname", name.c_str());
        }
    } else if (screen == Screen::Dead) {
        // back to the title, once the flower has been dead for half a second
        bool go = !std::strcmp(key, "Enter") || !std::strcmp(code, "Space") || !std::strcmp(code, "NumpadEnter");
        if (go && time - deadAt > 500) {
            connection.send(MSG_CONTINUE);
            screen = Screen::Title;
        }
    } else if (screen == Screen::Playing) {
        if (tankMode())
            onTankKey(code);
        else if (!onFlowerDevKey(code))
            ui.inventory.onKey(code);
    }
}

void Game::onEnter() {
    if (screen == Screen::Playing) return;
    connection.send(MSG_CONTINUE);
    ui_storage_set("florrio_nickname", name.c_str());
    sendSpawn();
    screen = Screen::Playing;
    ui.showDeathHud(false);
    spawnedAt = time;
    wasAlive_ = false;
}

void Game::sendSpawn() {
    if (tankServer()) {
        net::Writer k;
        k.u8(playTank ? 0 : 1);
        connection.send(MSG_KIND, k.b);
        tankPlayer_ = playTank;
    }
    net::Writer w;
    w.str(name);
    connection.send(MSG_SPAWN, w.b);
    spawnSentAt_ = time;
}

// Sent whenever it changes, at most every 30 ms.
void Game::sendInput(double now) {
    if (tankMode()) {
        sendTankInput(now);
        return;
    }
    uint32_t flags = 0;
    if (in.left || in.space) flags |= 1;
    if (in.right || in.shift) flags |= 2;
    float angle = 0, dist = 0;
    if (touchMode()) {
        flags = (ui.touch.attacking() ? 1 : 0) | (ui.touch.defending() ? 2 : 0);
        angle = ui.touch.joystickAngle();
        dist = ui.touch.joystickStrength() * 200;
    } else if (settings.keyboardControl) {
        float dx = float(in.rightKey) - float(in.leftKey), dy = float(in.down) - float(in.up);
        if (dx || dy) {
            angle = std::atan2(dy, dx);
            dist = 1000;
        }
    } else {
        float s = rendering.worldScale();
        float wx = rendering.cameraX() + (in.mx - ctx.width() * .5f) / s,
              wy = rendering.cameraY() + (in.my - ctx.height() * .5f) / s;
        Entity *p = simulation.get(player);
        float px = p ? p->x : rendering.cameraX(), py = p ? p->y : rendering.cameraY();
        angle = std::atan2(wy - py, wx - px);
        dist = std::hypot(wx - px, wy - py);
    }
    if (!simulation.flower()) angle = dist = 0;  // no flower to steer
    if (ui.inventory.stripOpen()) {
        flags &= ~3u;
        dist = 0;
    }  // the mouse is over the inventory
    // O asks for the next battle royale round; L, J and Y only count where the server has an event
    // entity, and Y goes out at once
    if (in.keyO) flags |= 1u << 31;
    bool eventKeys = false;
    for (const auto &kv : simulation.entities)
        if (kv.second.has(C_EVENT)) {
            eventKeys = true;
            break;
        }
    if (eventKeys) {
        if (in.keyL) flags |= 1u << 30;
        if (in.keyJ) flags |= 1u << 29;
        if (keyY_) flags |= 1u << 28;
    }
    bool force = forceInput || (eventKeys && keyY_);
    forceInput = false;
    if (!force) {
        if (flags == lastFlags_ && angle == lastAngle_ && dist == lastDist_) return;
        if (lastInput_ > 0 && now - lastInput_ <= 30) return;
    }
    lastInput_ = now;
    lastFlags_ = flags;
    lastAngle_ = angle;
    lastDist_ = dist;
    net::Writer w;
    w.varint(flags);
    w.f32(angle);
    w.f32(dist);
    connection.send(MSG_INPUT, w.b);
}

// a tank: WASD or the arrows move it, it aims at the mouse and fires with the left button or
// space; the right button or shift is its second ability
void Game::sendTankInput(double now) {
    uint32_t flags = 0;
    if (in.left || in.space || autoFire) flags |= 1;
    if (in.right || in.shift) flags |= 2;
    if (in.up) flags |= kInputUp;
    if (in.down) flags |= kInputDown;
    if (in.leftKey) flags |= kInputLeft;
    if (in.rightKey) flags |= kInputRight;
    float angle = 0, dist = 0;
    Entity *p = simulation.get(player);
    if (touchMode()) {
        flags = ui.touch.attacking() ? 1 : 0;
        if (ui.touch.defending()) flags |= 2;
        float s = ui.touch.joystickStrength();
        if (s > .2f) {
            float a = ui.touch.joystickAngle(), x = std::cos(a), y = std::sin(a);
            if (y < -.38f) flags |= kInputUp;
            if (y > .38f) flags |= kInputDown;
            if (x < -.38f) flags |= kInputLeft;
            if (x > .38f) flags |= kInputRight;
            angle = a;
            dist = 200;
        }
    } else if (autoSpin) {
        angle = float(std::remainder(now / 1000 + spinPhase_, 6.283185307179586));  // a radian a second
        dist = 350;
    } else {
        float s = rendering.worldScale();
        float wx = rendering.cameraX() + (in.mx - ctx.width() * .5f) / s,
              wy = rendering.cameraY() + (in.my - ctx.height() * .5f) / s;
        float px = p ? p->x : rendering.cameraX(), py = p ? p->y : rendering.cameraY();
        angle = std::atan2(wy - py, wx - px);
        dist = std::hypot(wx - px, wy - py);
    }
    if (!simulation.flower()) angle = dist = 0;
    if (dist > 0) tankAngle = angle;
    if (ui.overTankHud(in.mx, in.my)) flags &= ~3u;
    bool force = forceInput;
    forceInput = false;
    if (!force) {
        if (flags == lastFlags_ && angle == lastAngle_ && dist == lastDist_) return;
        if (lastInput_ > 0 && now - lastInput_ <= 30) return;
    }
    lastInput_ = now;
    lastFlags_ = flags;
    lastAngle_ = angle;
    lastDist_ = dist;
    net::Writer w;
    w.varint(flags);
    w.f32(angle);
    w.f32(dist);
    connection.send(MSG_INPUT, w.b);
}

void Game::handleEvents(double now) {
    for (const net::Event &event : connection.events) {
        if (event.type == EVENT_PLAYER_ID) {
            ui_storage_set("cp6_player_id", event.text.c_str());
            continue;
        }
        if (event.type == EVENT_DEV) {
            devKeys = true;
            continue;
        }
        if (event.type == EVENT_NOTIFY) {
            ui.notify(event.text, event.color, event.ms);
            continue;
        }
        Entity *e = simulation.get(event.id);
        if (!e) continue;
        if (event.type == EVENT_HIT) {
            e->hurtAt = now;
            if (simulation.flower() == e) rendering.noteHit(now);
        } else if (event.type == EVENT_PICKUP && event.slot >= 0 && event.slot < 16) {
            // the card starts where the entity is on screen and grows into its slot
            float W = float(ctx.width()), H = float(ctx.height());
            float s = rendering.worldScale(), u = ui.hudScale();
            float x = (W * .5f + (e->x - rendering.cameraX()) * s) / u;
            float y = (H * .5f + (e->y - rendering.cameraY()) * s) / u;
            ui.inventory.notePickup(event.slot, x, y, now);
            if (e->drop) e->drop->collected = true;
        }
    }
}

// keeps what the death screen will want while your flower lives, and notices when it dies
void Game::trackPlayer(double now) {
    sim::CameraData *cam = simulation.camera();
    uint32_t id = cam ? cam->player : 0;
    Entity *flower = simulation.get(id);
    bool alive = flower && flower->deadAt < 0;  // the server keeps a dead flower around for a moment
    if (alive) {
        if (!wasAlive_ && devKeys)
            ui.notify(flower->tank ? "Dev keys: K to level up, \\ for the previous tank, O to die"
                                   : "Dev keys: K to level up, P to pick petals, O to die",
                      0x000000, 8000, "dev_keys");
        player = id;
        wasAlive_ = true;
        tankPlayer_ = bool(flower->tank);
        if (flower->flower) rememberLife(*flower->flower);
    }
    if (cam && !alive && wasAlive_) lastLife.killer = cam->killer;
    if (screen == Screen::Playing && wasAlive_ && !alive && now - spawnedAt > 500) {
        screen = Screen::Dead;
        deadAt = now;
        ui.showDeathHud(true);
    }
    if (screen == Screen::Playing && !wasAlive_ && now - spawnSentAt_ > 1000) sendSpawn();
}

void Game::rememberLife(const sim::FlowerData &flower) {
    lastLife.level = std::max(1, flower.level);
    lastLife.slots.clear();
    lastLife.ring.clear();
    const std::vector<int> &slots = flower.slots;
    int ringSlots = Inventory::primarySlots(lastLife.level);
    for (int i = 0; i < 5; i++) lastLife.petals[i] = i < int(slots.size()) && slots[i] > 0 ? slots[i] : 0;
    for (int i = 0; i < int(slots.size()); i++) {
        if (slots[i] <= 0) continue;
        lastLife.slots.push_back(slots[i]);
        if (i < ringSlots) lastLife.ring.push_back(slots[i]);
        ui.menus.discoverPetal(slots[i]);
    }
}

void Game::frame(double now) {
    float frameMs = time ? float(now - time) : 16.f;
    float dt = std::min(frameMs, 250.f);
    time = now;
    ui.menus.setWheel(float(ui_wheel()));
    connection.frame(simulation, now);
    if (connection.dropped()) {
        screen = Screen::Title;
        devKeys = false;
        wasAlive_ = false;
        player = 0;
    }
    handleEvents(now);
    for (auto &kv : simulation.entities) {
        Entity &e = kv.second;
        if (e.deadAt < 0 && e.death && e.death->dead)
            e.deadAt = e.drawn ? now : now - 150;  // dead before it was seen: gone
        rendering.smooth(e, dt, now);
    }
    trackPlayer(now);
    sim::CameraData *cam = simulation.camera();
    rendering.updateCamera(cam, simulation.get(cam ? cam->player : 0), dt, now);
    ui.inventory.update(now);
    ui.touch.update(now);
    // the dead flower still gets input until the server removes it
    if (cam && screen != Screen::Title) {
        sendInput(now);
        if (keyI_) connection.send(MSG_ACTION);  // the server decides what I does
    }
    keyY_ = keyI_ = false;
    if (screen != Screen::Title) ui.menus.closeCursor();

    ui.render(ctx, now, dt, frameMs);
    in.clicked = false;
}

void Game::choosePlay(bool tank) {
    playTank = tank;
    ui_storage_set("play_as", tank ? "tank" : "flower");
}

bool Game::onButton(int button, bool down) {
    if (screen == Screen::Title) {
        float u = ui.hudScale();
        int kind = button == 0 && down && tankServer() ? ui.playKindAt(in.mx / u, in.my / u) : -1;
        if (kind >= 0) {
            choosePlay(kind == 0);
            return true;
        }
        return ui.menus.onButton(button, down);
    }
    if (screen != Screen::Playing || button != 0) return false;
    if (tankMode()) return ui.onTankButton(down);
    if (ui.petalPickerOpen()) {
        float u = ui.hudScale();
        int type = ui.pickerAt(in.mx / u, in.my / u);
        if (type >= 0) {
            if (down && type > 0) sendDev(3, type);
            return true;
        }
    }
    return ui.inventory.onButton(down);
}

void Game::onTouch(int phase, int id, float x, float y) {
    ui.touch.onTouch(phase, id, x, y);
}

}  // namespace game
