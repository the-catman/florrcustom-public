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
        ui.inventory.onKey(code);
    }
}

void Game::onEnter() {
    if (screen == Screen::Playing) return;
    connection.send(MSG_CONTINUE);
    ui_storage_set("florrio_nickname", name.c_str());
    net::Writer w;
    w.str(name);
    connection.send(MSG_SPAWN, w.b);
    screen = Screen::Playing;
    ui.showDeathHud(false);
    spawnedAt = time;
    wasAlive_ = false;
}

// Sent whenever it changes, at most every 30 ms.
void Game::sendInput(double now) {
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

void Game::handleEvents(double now) {
    for (const net::Event &event : connection.events) {
        if (event.type == EVENT_PLAYER_ID) {
            ui_storage_set("cp6_player_id", event.text.c_str());
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
        player = id;
        wasAlive_ = true;
        if (flower->flower) rememberLife(*flower->flower);
    }
    if (cam && !alive && wasAlive_) lastLife.killer = cam->killer;
    if (screen == Screen::Playing && wasAlive_ && !alive && now - spawnedAt > 500) {
        screen = Screen::Dead;
        deadAt = now;
        ui.showDeathHud(true);
    }
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

bool Game::onButton(int button, bool down) {
    if (screen == Screen::Title) return ui.menus.onButton(button, down);
    if (screen != Screen::Playing || button != 0) return false;
    return ui.inventory.onButton(down);
}

void Game::onTouch(int phase, int id, float x, float y) {
    ui.touch.onTouch(phase, id, x, y);
}

}  // namespace game
