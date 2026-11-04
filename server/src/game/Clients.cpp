#include "game/Clients.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Cipher.h"
#include "Math.h"
#include "game/Game.h"

namespace game {
using namespace world;

// zero, subnormal, infinite and NaN input numbers all count as zero
static float sane(float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return ((b + 0x800000u) & 0x7f000000u) ? f : 0.0f;
}

Clients::Clients(Game &game) : System(game) {}

int Clients::connect() {
    int id = nextClient_++;
    Client &c = all[id];
    packets[id];
    Entity &cam = entities_.create(bit(C_CAMERA));
    cam.camera->setFov(1);
    c.camera = cam.id;
    return id;
}

void Clients::disconnect(int client) {
    auto it = all.find(client);
    if (it == all.end()) return;
    Client &c = it->second;
    if (Entity *f = world_.get(c.flower)) {
        FlowerState fs = game_.flowers.states[f->slot()];
        for (auto &copies : fs.petal)
            for (uint32_t p : copies) entities_.destroy(p);
        entities_.destroy(f->id);
    }
    entities_.destroy(c.camera);
    all.erase(it);
    packets.erase(client);
}

void Clients::receive(int client, const uint8_t *b, size_t n, double now) {
    game_.now = now;
    auto it = all.find(client);
    if (it == all.end()) return;
    Client &c = it->second;
    std::vector<uint8_t> buf(b, b + n);
    net::cipher(buf.data(), buf.size());
    net::Reader r(buf.data(), buf.size());
    // [varint seq][varint type][payload], back to back; ones already handled are resent until acked
    while (!r.atEnd() && !r.bad()) {
        uint32_t seq = r.varint();
        uint32_t type = r.varint();
        bool fresh = seq > c.lastSeq;
        if (fresh) c.lastSeq = seq;
        handle(c, type, r, fresh);  // an old one is still read, to get past it
    }
}

void Clients::handle(Client &c, uint32_t type, net::Reader &r, bool apply) {
    switch (type) {
        case MSG_HELLO:
            r.u8();
            r.str();
            r.u8();
            r.str();
            break;
        case 2:
            r.str();
            r.str();
            break;
        case MSG_CONTINUE: break;
        case 5: r.str(); break;
        case MSG_SPAWN: {
            std::string name = r.str();
            if (!apply) break;
            c.name = name;
            if (c.camera) game_.flowers.spawn(c);
            break;
        }
        case 3:
        case 7: r.varint(); break;
        case MSG_TRASH: onTrash(c, r, apply); break;
        case MSG_SWAP: onSwap(c, r, apply); break;
        case MSG_INPUT: onInput(c, r, apply); break;
        default: break;
    }
}

// give up a petal: it goes to the inventory, and to xp unless it's a Basic
void Clients::onTrash(Client &c, net::Reader &r, bool apply) {
    static const float kXp[6] = {2, 10, 50, 200, 1000, 2500};
    int slot = r.u8();
    uint8_t type = r.u8();
    Entity *f = apply ? world_.get(c.flower) : nullptr;
    if (!f) return;
    FlowerState &fs = game_.flowers.states[f->slot()];
    if (slot > 15 || slot >= slotsWithSpares(f->level())) return;
    if (fs.slots[slot] != type || !isPetal(type)) return;
    uint8_t kept = 1;
    if (type != P_BASIC) {
        game_.flowers.giveXp(*f, kXp[std::min<int>(kPetals[type].rarity, 5)]);
        kept = fs.slots[slot];
        if (!kept) return;
    }
    if (fs.invCount >= 16) {
        std::memmove(fs.inv, fs.inv + 1, 15);
        fs.invCount--;
    }
    fs.inv[fs.invCount++] = kept;
    game_.flowers.setSlot(*f, slot, P_NONE);
}

// two slots trade petals
void Clients::onSwap(Client &c, net::Reader &r, bool apply) {
    int a = r.u8();
    uint8_t ta = r.u8();
    int b = r.u8();
    uint8_t tb = r.u8();
    Entity *f = apply ? world_.get(c.flower) : nullptr;
    if (!f) return;
    FlowerState &fs = game_.flowers.states[f->slot()];
    int n = slotsWithSpares(f->level());
    if (a >= n || b >= n || ta != fs.slots[a] || tb != fs.slots[b]) return;
    game_.flowers.setSlot(*f, b, ta);
    game_.flowers.setSlot(*f, a, tb);
}

void Clients::onInput(Client &c, net::Reader &r, bool apply) {
    uint32_t flags = r.varint();
    float angle = sane(r.f32()), dist = sane(r.f32());
    if (!apply) return;
    angle = float(std::fmod(double(angle), TAU));
    c.input = {flags, angle, dist < 0.0f || dist != dist ? 0.0f : dist};
    Entity *f = world_.get(c.flower);
    if (!f) return;
    // where the eyes look, in 256ths of a turn
    double q = std::floor(std::fmod(std::fmod(double(angle), TAU) + TAU, TAU) * (255.0 / TAU));
    f->flower->setEyes(toU32(q) & 255);
    int face = f->flower->face;
    face = flags & 1 ? face | 1 : face & 254;  // attacking
    face = flags & 2 ? face | 2 : face & 253;  // defending
    f->flower->setFace(face);
}

// a camera looks at its flower, and once that's gone at whatever killed it
void Clients::moveCameras() {
    for (auto &[id, c] : all) {
        Entity *cam = world_.get(c.camera);
        if (!cam) continue;
        Entity *t = world_.get(cam->camera->player);
        if (!t) t = world_.get(entities_.obj(*cam).killer);
        if (!t || !entities_.obj(*t).body) continue;
        const Body &b = *entities_.obj(*t).body;
        cam->camera->setX(b.x);
        cam->camera->setY(b.y);
    }
}

std::vector<uint32_t> Clients::visibleTo(Client &c) {
    std::vector<uint32_t> ids;
    Entity *cam = world_.get(c.camera);
    if (!cam) return ids;
    ids.push_back(game_.arena.id);
    ids.push_back(c.camera);
    if (Entity *f = world_.get(cam->camera->player)) {
        ids.push_back(f->id);
        c.looked = true;
    }
    // once a client has followed a flower it sees around its camera for good
    if (c.looked) {
        float fov = cam->camera->fov;
        float w = 1920.f / fov + 200.f, h = 1080.f / fov + 200.f;
        float x0 = w * -0.5f + cam->camera->x, y0 = h * -0.5f + cam->camera->y;
        game_.physics.tree.visit(x0, y0, w, h, [&](uint16_t s) {
            ids.push_back(world_.at(s)->id);
            return false;
        });
    }
    std::sort(ids.begin(), ids.end(), [](uint32_t a, uint32_t b) { return (a & 16383) < (b & 16383); });
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

void Clients::buildPacket(Client &c, std::vector<uint8_t> &out) {
    net::Writer body;
    body.varint(c.lastSeq);
    net::Writer sec;
    auto pad = [&] { return uint8_t(rng_.between(0, 255)); };
    auto yours = [&](const world::Entity &e) {
        Entity *cam = entities_.cameraOf(world_.get(e.id));
        return cam && cam->id == c.camera;
    };
    c.view.write(sec, world_, visibleTo(c), pad, yours);
    body.varint(uint32_t(sec.size()));
    body.bytes(sec.b.data(), sec.size());
    body.u8(pad());
    for (auto &ev : c.events) {
        body.varint(ev.reliable ? c.packetSeq + 2 : 1);
        body.bytes(ev.bytes.data(), ev.bytes.size());
    }
    c.events.clear();
    body.varint(0);
    body.u8(pad());
    body.varint(0);
    net::cipher(body.b.data(), body.size());
    net::Writer pkt;
    pkt.u8(kPacketUpdate);
    pkt.varint(++c.packetSeq);
    pkt.bytes(body.b.data(), body.size());
    out = std::move(pkt.b);
}

void Clients::waitToSpawn(double dt) {
    for (auto &[id, c] : all) c.spawnWait = std::max(float(double(c.spawnWait) - dt), 0.0f);
}

void Clients::buildPackets() {
    for (auto &[id, c] : all) buildPacket(c, packets[id]);
}

}  // namespace game
