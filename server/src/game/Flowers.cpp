#include "game/Flowers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>

#include "Math.h"
#include "game/Game.h"

namespace game {
using namespace world;

// Names lose a few invisible or oversized characters and surrounding whitespace, then are cut to
// at most 13 bytes of valid UTF-8.
static std::string cleanName(std::string n) {
    static const char *const kDropped[] = {
        "\xf0\x92\x88\x99", "\xf0\x92\x90\xab", "\xef\xb7\xbd", "\xeb\xb8\xbb",
        "\xea\xa7\x85",     "\xe2\x80\x8f",     "\xe3\x85\xa4",
    };
    for (size_t i = 0; i < n.size();) {
        size_t cut = 0;
        for (const char *d : kDropped) {
            size_t k = std::strlen(d);
            if (n.compare(i, k, d) == 0) {
                cut = k;
                break;
            }
        }
        if (cut)
            n.erase(i, cut);
        else
            i++;
    }
    auto blank = [](unsigned char ch) { return ch == 9 || ch == 10 || ch == 12 || ch == 13 || ch == 32; };
    size_t a = 0, b = n.size();
    while (a < b && blank(n[a])) a++;
    while (b > a && blank(n[b - 1])) b--;
    n = n.substr(a, b - a);
    auto validUtf8 = [](const std::string &s, size_t len) {
        for (size_t i = 0; i < len;) {
            unsigned char ch = s[i];
            size_t k = ch < 0x80 ? 1 : (ch >> 5) == 6 ? 2 : (ch >> 4) == 14 ? 3 : (ch >> 3) == 30 ? 4 : 0;
            if (!k || i + k > len) return false;
            for (size_t j = 1; j < k; j++)
                if ((static_cast<unsigned char>(s[i + j]) >> 6) != 2) return false;
            i += k;
        }
        return true;
    };
    size_t len = n.size();
    while (len > 13 || !validUtf8(n, len)) len--;
    return n.substr(0, len);
}

// 1.06^n, by squaring
static double growth(int a) {
    double c = 1.06, b = (a & 1) ? 1.06 : 1.0;
    if (a + 1 >= 3) {
        int d = a;
        do {
            c = c * c;
            d = d / 2;
            if (d & 1) b = c * b;
        } while (d + 1 > 2);
    }
    return a < 0 ? 1.0 / b : b;
}

static int rarityOf(uint8_t type) {
    return isPetal(type) ? kPetals[type].rarity : 0;
}

Flowers::Flowers(Game &game) : System(game) {}

// a spawn point too close to a player (800) or a mob (100)
bool Flowers::spawnTaken(float x, float y) {
    for (uint16_t s : world_.slots()) {
        const Entity &e = *world_.at(s);
        const Obj &o = entities_.objs[s];
        if (!o.hit || !game_.physics.tree.contains(s)) continue;
        float d;
        if (e.has(C_FLOWER))
            d = 800;
        else if (e.has(C_MOB))
            d = 100;
        else
            continue;
        float dx = float(o.body->x) - x, dy = float(o.body->y) - y, rr = d + o.hit->radius;
        if (dx * dx + dy * dy < rr * rr) return true;
    }
    return false;
}

// a new flower in the part of the map for the client's level, mostly towards its left end
// A new flower in the strip its last one earned, levelled up to where that one got to, with the
// petals it got to keep and Basic petals after them.
void Flowers::spawn(Client &c) {
    Entity *cam = world_.get(c.camera);
    if (!cam || world_.get(c.flower) || c.spawnWait > 0) return;
    c.spawnWait = 2500;
    int earned = cam->camera->respawnLevel;
    float x, y;
    if (!findSpawnSpot(earned, x, y)) return;
    Entity &f = entities_.create(bit(C_NAME) | bit(C_DEATH) | bit(C_STATUS) | bit(C_FLOWER) | bit(C_LOOT));
    FlowerState &fs = states[f.slot()];
    fs = FlowerState();
    fs.safe = true;  // mobs leave a new flower alone for ten seconds
    fs.safeUntil = game_.now + 10000;
    f.name->setName(cleanName(c.name));
    entities_.setMaxHealth(f, 100);
    entities_.hitbox(f).damage = 25;
    entities_.setRadius(f, 25);
    entities_.setPos(f, x, y);
    entities_.setOwner(f, c.camera);
    all.push_back(f.id);
    entities_.obj(f).loot = std::make_unique<Loot>();
    f.flower->setLevel(1);
    for (int i = 0; i < 8; i++) f.flower->setCooldown(i, 65535);
    c.flower = f.id;
    cam->camera->setPlayer(f.id);
    givePetals(f, fs, *cam->camera, earned);
    cam->camera->setRespawnLevel(0);
    cam->camera->setKeptCount(0);
    updateStats(f);
}

// somewhere free in the strip the level earned last time belongs to, favouring its left end
bool Flowers::findSpawnSpot(int earned, float &x, float &y) {
    float w = game_.arena.mapW;
    double left = double(w * -0.5f), span = double(w * 0.5f) - left, lo;
    float hi;
    if (earned <= 12) {
        lo = span * 0.0 + left;
        hi = float(span * double(0.3f) + left);
    } else if (earned <= 27) {
        lo = span * double(0.3f) + left;
        hi = float(span * double(0.6f) + left);
    } else if (earned <= 42) {
        lo = span * double(0.6f) + left;
        hi = float(span * double(0.9f) + left);
    } else {
        lo = span * double(0.9f) + left;
        hi = w * 0.5f;
    }
    double start = double(float(lo)), width = double(hi) - start;
    for (int tries = 0;;) {
        float u = rng_.unit();
        x = float(width * std::fmin(double(u * u) * 0.8, 1.0) + start);
        y = game_.arena.mapH * (rng_.unit() - 0.5f);
        if (!spawnTaken(x, y)) return true;
        if (++tries == 1000) {
            std::fprintf(stderr, "no room to spawn a player\n");
            return false;
        }
    }
}

// a new flower gets its level back, the petals it kept, and Basics for the rest
void Flowers::givePetals(Entity &f, FlowerState &fs, const CameraData &cam, int earned) {
    uint8_t list[16] = {};
    int kept = cam.keptCount;
    for (int i = 0; i < kept; i++) list[i] = uint8_t(cam.kept[i]);
    for (int lv = f.level(); earned > lv; lv = f.level()) giveXp(f, levelNeed(lv) - fs.levelXp);

    // one flower in 20000 starts with a mythic Basic, if nobody has one
    bool mythic = rng_.unit() < 0.00005;
    int level = f.level(), n = std::max(kept, slotCount(level));
    for (int i = kept; i < n; i++) {
        list[i] = P_BASIC;
        if (mythic && !game_.drops.mythicTaken(P_UNIQUE_BASIC, f)) {
            list[i] = P_UNIQUE_BASIC;
            mythic = false;
        }
    }
    n = std::min(n, slotsWithSpares(level));
    for (int i = 0; i < n; i++) {
        setSlot(f, i, list[i]);
        int copies = isPetal(list[i]) ? kPetals[list[i]].count : 0;
        for (int k = 0; k < copies; k++) game_.petals.make(f, i, k);
    }
}

// Each flower: the ring turns, missing petals reload and grow back, stats are worked out, the
// zone timer runs and health comes back.
void Flowers::tick(double dt) {
    for (size_t i = 0; i < all.size(); i++) {
        Entity *f = world_.get(all[i]);
        if (!f || !f->has(C_HEALTH) || !entities_.obj(*f).body) continue;
        FlowerState &fs = states[f->slot()];
        int level = f->level();
        bool locked = zoneLocked(*f);
        turnRing(*f, fs, level, locked, dt);
        if (f->hp() > 0.0f) reloadPetals(*f, fs, level, locked, dt);
        updateStats(*f);
        runZoneTimer(*f, fs, level, dt);
        recover(*f, fs, level, dt);
    }
}

void Flowers::turnRing(Entity &f, FlowerState &fs, int level, bool locked, double dt) {
    static const float kTurn[9] = {1, -1, 0, 1, -1, 0, 1, -1, 5};  // by yin yangs, mod 9
    int faster = locked ? 0 : countAlive(fs, level, P_FASTER);
    int yin = 0;
    if (!locked) {
        for (int s = 0; s < slotCount(level); s++) yin += fs.slots[s] == P_YIN_YANG;
        yin %= 9;
    }
    double spin = (double(uint32_t(faster)) * 0.8 + 2.5) * double(kTurn[yin]) * double(game_.effects.spinFactor(f));
    fs.rot = float(spin * dt * 0.001 + double(fs.rot));
}

void Flowers::reloadPetals(Entity &f, FlowerState &fs, int level, bool locked, double dt) {
    uint32_t shown[8];
    for (uint32_t &v : shown) v = 65535;
    float step = float(dt);
    for (int s = 0; s < 8; s++) {
        for (int k = 0; k < 3; k++) {
            if (uint32_t id = fs.petal[s][k]) {
                if (Entity *p = world_.get(id)) {
                    if (!p->has(C_HEALTH)) continue;
                    float hp = p->hp();
                    if (hp > 0.0f && !locked) continue;
                    if (hp > 0.0f) game_.combat.damage(*p, nullptr, hp, 1);  // out of its zone too long
                }
                fs.petal[s][k] = 0;
            }
            uint8_t type = fs.slots[s];
            if (!isPetal(type) || slotCount(level) <= s) continue;
            const PetalDef &def = kPetals[type];
            if (k >= def.count) continue;
            if (def.passive) {
                fs.reload[s][k] = 0;
                shown[s] = 65535;
                continue;
            }
            float need = def.reload;
            if (fs.swapped[s][k] && need < 2500.0f) need = 2500.0f;
            float t = fs.reload[s][k] + step;
            t = t > need ? need : t;
            fs.reload[s][k] = t;
            if (locked) {
                shown[s] = 0;
                continue;
            }
            if (t >= need) {
                game_.petals.make(f, s, k);
                continue;
            }
            float r = t * 65535.0f / need;
            shown[s] = std::min(shown[s], std::min(toU32(r), 65535u));
        }
    }
    for (int s = 0; s < 8; s++) f.flower->setCooldown(s, shown[s]);
}

// the zone timer fills while left of the level's zone and drains a tenth as fast after
void Flowers::runZoneTimer(Entity &f, FlowerState &fs, int level, double dt) {
    float limit = game_.arena.royale() ? 5000.0f : 45000.0f;
    double zt = double(fs.zoneTime);
    float t = double(entities_.body(f).x) < double(game_.arena.zoneEdge(level)) ? float(dt + zt) : float(zt - dt * 0.1);
    t = 0.0f > t ? 0.0f : (limit < t ? limit : t);
    fs.zoneTime = t;
    float z = t * 255.0f / limit;
    z = z < 0.0f ? 0.0f : std::fmin(z, 255.0f);
    f.flower->setZoneTimer(toU32(z) & 255);
}

void Flowers::recover(Entity &f, FlowerState &fs, int level, double dt) {
    float hp = f.hp();
    if (!(hp > 0.0f)) return;
    bool royale = game_.arena.royale();
    double max = double(f.maxHp());
    if (entities_.cameraOf(&f)) {
        if (!zoneLocked(f)) {
            double r = dt * max * (1.0 / 240000);
            int leaves = countAlive(fs, level, P_LEAF);
            game_.combat.heal(f, float(r + dt * double(uint32_t(leaves)) * 0.001));
            if (!royale) return;
        } else if (!royale) {
            return;
        }
        if (zoneLocked(f)) game_.combat.damage(f, nullptr, float(dt * max * (1.0 / 15000)), 1);
    } else {
        // nobody behind it any more: it wastes away
        game_.combat.damage(f, nullptr, level >= 8 ? float(dt * max * (1.0 / 60000)) : hp, 1);
        if (royale && !game_.arena.running()) game_.combat.damage(f, nullptr, f.hp(), 0);
    }
}

void Flowers::move(double dt) {
    for (auto &[id, c] : game_.clients.all) {
        Entity *f = world_.get(c.flower);
        if (!f || f->hp() <= 0) continue;
        Body &b = entities_.body(*f);
        float speed = float(dt * double(game_.effects.slowdown(*f) * 2.5f) * 0.001);
        float dist = c.input.dist;
        float k = dist < 200 ? speed * dist * 0.005f : speed;
        b.vx += double(std::cos(c.input.angle) * k);
        b.vy += double(std::sin(c.input.angle) * k);
    }
}

// out of its zone for too long: petals that need the zone stop counting
bool Flowers::zoneLocked(Entity &f) {
    const Body &b = entities_.body(f);
    if (!(double(b.x) < double(game_.arena.zoneEdge(f.level())))) return false;
    return states[f.slot()].zoneTime >= (game_.arena.royale() ? 5000.0f : 45000.0f);
}

// equipped petals of a type whose first copy wasn't just swapped in
int Flowers::countAlive(const FlowerState &fs, int level, uint8_t type) {
    int n = 0;
    for (int s = 0; s < slotCount(level); s++)
        if (!fs.swapped[s][0] && fs.slots[s] == type) n++;
    return n;
}

// xp a level takes to finish
float Flowers::levelNeed(int level) {
    return float(std::floor(growth(level - 1) * double(uint32_t(level))) + 3.0);
}

// score and levels; each level needs 6% more than the last
void Flowers::giveXp(Entity &f, float xp) {
    FlowerState &fs = states[f.slot()];
    float m = game_.arena.royale() ? 3.0f : 1.0f;
    float add = m * xp;
    fs.levelXp = fs.levelXp + add;
    float score = f.flower->score;
    f.flower->setScore(add + score);
    if (Loot *l = entities_.obj(f).loot.get()) l->xp = float(double(f.flower->score) * 0.5 / double(m));
    for (int lv = f.level(); lv <= 98; lv = f.level()) {
        float need = levelNeed(lv);
        float have = fs.levelXp;
        if (have < need) {
            float p = have * 65535.0f / need;
            if (p != float(f.flower->xp & 0xffff)) f.flower->setXp(toU32(p) & 0xffff);
            return;
        }
        fs.levelXp = have - need;
        f.flower->setLevel(lv + 1);
        updateStats(f);
    }
    if ((f.flower->xp & 0xffff) != 65535) f.flower->setXp(65535);
}

// Max health from level and petals, the third eye, and the camera's zoom and respawn level. Runs
// every tick.
void Flowers::updateStats(Entity &f) {
    FlowerState &fs = states[f.slot()];
    int level = f.level();
    float fov = 1.0f;
    if (f.hp() > 0.0f) {
        float o = float(double(level - 1) * (1.0 / 44));
        float p = std::fmin(o, 1.0f);
        bool locked = zoneLocked(f);
        int cactus = locked ? 0 : countAlive(fs, level, P_CACTUS);
        int big = locked ? 0 : countAlive(fs, level, P_CACTUS_TRIO);
        int mid = locked ? 0 : countAlive(fs, level, P_POISON_CACTUS);
        float max = (o < 0.0f ? 100.0f : p * 50.0f + 100.0f) + float(uint32_t(cactus * 20)) + float(uint32_t(big * 40));
        max = max + float(uint32_t(mid * 20));
        entities_.setMaxHealth(f, max);
        bool eye = false;
        for (int s = 0; s < slotCount(level); s++)
            if (fs.slots[s] == P_ANTENNAE) eye = true;
        int face = f.flower->face;
        f.flower->setFace(eye ? face | 4 : face & 251);
        fov = eye ? 0.63f : 0.9f;
    }
    if (!game_.arena.running()) fov = float(double(fov) * 0.7);
    Entity *cam = entities_.cameraOf(&f);
    if (!cam) return;
    cam->camera->setFov(fov);
    cam->camera->setRespawnLevel(level * 3 >> 2);
}

// Puts a petal type in a slot. Whatever was out from a main slot is killed, and the new petal
// reloads as if it had just been swapped in.
void Flowers::setSlot(Entity &f, int slot, uint8_t type) {
    if (slot > 15) return;
    FlowerState &fs = states[f.slot()];
    if (fs.slots[slot] == type) return;
    if (slot <= 7) {
        for (int k = 0; k < 3; k++) {
            Entity *p = world_.get(fs.petal[slot][k]);
            if (p && p->has(C_HEALTH)) game_.combat.damage(*p, nullptr, p->hp(), 1);
            fs.petal[slot][k] = 0;
        }
        for (int k = 0; k < 3; k++) {
            fs.swapped[slot][k] = true;
            fs.reload[slot][k] = 0;
        }
    }
    if (fs.slots[slot] != type) {
        fs.slots[slot] = type;
        f.flower->setSlot(slot, type);
    }
    game_.petals.place(f);
    updateStats(f);
}

// A dead flower's petals, rarest first and in a random order within each rarity: a tenth are
// lost, the first three of the rest drop where it died, and the next ten are shown on the death
// screen. Basic petals are passed over.
void Flowers::onDeath(Entity &f) {
    FlowerState &fs = states[f.slot()];
    int level = f.level();
    std::vector<uint8_t> byRarity[6];
    int n = slotsWithSpares(level);
    int filled = 0;
    for (int s = 0; s < n; s++)
        if (uint8_t t = fs.slots[s]) {
            byRarity[rarityOf(t)].push_back(t);
            filled++;
        }
    for (int i = std::max(0, fs.invCount - (n - filled)); i < fs.invCount; i++)
        byRarity[rarityOf(fs.inv[i])].push_back(fs.inv[i]);

    uint8_t drop[3], shown[10];
    int dropped = 0, listed = 0;
    for (int r = 5; r >= 0; r--) {
        std::vector<uint8_t> &b = byRarity[r];
        if (b.empty()) continue;
        if (!deathRngSeeded_) {
            deathRng_ = Rng(game_.entropy ? game_.entropy() : std::random_device()());
            deathRngSeeded_ = true;
        }
        // shuffled by hand so every build gets the same order
        if (b.size() > 1)
            for (uint32_t i = 0, d = uint32_t(b.size()) - 1; d > 0; i++, d--)
                if (uint32_t j = deathRng_.between(0, d)) std::swap(b[i], b[i + j]);
        for (uint8_t t : b) {
            if (t == P_BASIC) continue;
            if (rng_.unit() < 0.1) continue;
            if (dropped < 3)
                drop[dropped++] = t;
            else if (listed < 10)
                shown[listed++] = t;
        }
    }
    for (int i = 0; i < dropped; i++)
        if (rarityOf(drop[i]) >= 2 && rng_.unit() < 0.01) drop[i] = P_POLLEN;

    if (Entity *cam = entities_.cameraOf(&f)) {
        cam->camera->setKeptCount(listed);
        for (int i = 0; i < listed; i++) cam->camera->setKept(i, shown[i]);
    }
    entities_.obj(f).loot->petals.assign(drop, drop + dropped);
}

// The camera that was following the flower turns to whatever killed it, and the death screen
// gets the score, the level and the killer's name.
void Flowers::deathScreen(Entity &f, Entity *by) {
    Entity *cam = entities_.cameraOf(&f);
    if (!cam || cam->camera->player != f.id) return;
    Entity *k = by ? entities_.killerOf(by) : nullptr;
    entities_.obj(*cam).killer = k ? k->id : 0;
    cam->camera->setScore(f.flower->score);
    cam->camera->setDeathLevel(f.level());
    cam->camera->setKiller(k && k->has(C_NAME) ? k->name->name : std::string());
}

}  // namespace game
