#include "game/Tanks.h"

#include <algorithm>
#include <cmath>

#include "Math.h"
#include "game/Diep.h"
#include "game/Game.h"
#include "TankRules.h"

namespace game {
using namespace world;
using namespace tankdata;
using diep::kHealth;
using diep::kLength;

Tanks::Tanks(Game &game) : System(game) {}

Entity *Tanks::cameraOf(Entity &e) {
    return entities_.cameraOf(&e);
}

// the camera of a player's tank, which keeps its level, score and stats
Entity *Tanks::driver(Entity &t) {
    Entity *cam = cameraOf(t);
    return cam && cam->has(C_DRIVER) ? cam : nullptr;
}

Client *Tanks::clientOf(Entity &t) {
    for (auto &[id, c] : game_.clients.all)
        if (c.flower == t.id) return &c;
    return nullptr;
}

// a stat of the player behind anything of theirs; nothing for bosses
int Tanks::stat(Entity &e, int which) {
    Entity *cam = cameraOf(e);
    return cam && cam->has(C_DRIVER) ? cam->driver->stats[which] : 0;
}

void Tanks::notify(Client &c, const std::string &text, uint32_t color, float ms) {
    net::Writer w;
    w.varint(EVENT_NOTIFY);
    w.str(text);
    w.u32(color);
    w.f32(ms);
    c.events.push_back({false, std::move(w.b)});
}

void Tanks::notifyAll(const std::string &text, uint32_t color, float ms) {
    for (auto &[id, c] : game_.clients.all) notify(c, text, color, ms);
}

// A new tank: a Basic at level 1 with no stats, which then gets the level the last one earned.
void Tanks::spawn(Client &c) {
    Entity *cam = world_.get(c.camera);
    if (!cam || !cam->has(C_DRIVER) || world_.get(c.flower) || c.spawnWait > 0) return;
    c.spawnWait = 2500;
    int level = std::max(1, cam->camera->respawnLevel);
    float x, y;
    if (!game_.flowers.findSpawnSpot(level, x, y)) return;
    Entity &t = entities_.create(bit(C_NAME) | bit(C_DEATH) | bit(C_STATUS) | bit(C_TANK));
    Obj &o = entities_.obj(t);
    t.name->setName(cleanName(c.name));
    entities_.body(t).diep = true;
    entities_.setPos(t, x, y);
    entities_.setOwner(t, c.camera);
    o.contact = std::make_unique<Contact>();
    o.contact->reduction = 0;  // until the spawn shield is gone
    o.contact->maxMul = 6;
    o.tank = std::make_unique<TankState>();
    TankState &ts = *o.tank;
    ts.born = game_.now;
    t.tank->setTeam(c.camera);
    t.tank->setColor(COLOR_TEAM);
    t.tank->setFlags(1);
    all.push_back(t.id);
    c.flower = t.id;
    cam->camera->setPlayer(t.id);
    entities_.obj(*cam).killer = 0;
    DriverData &d = *cam->driver;
    d.setLevel(1);
    d.setScore(0);
    d.setPoints(0);
    for (int i = 0; i < S_COUNT; i++) d.setStat(i, 0);
    setTank(t, 0);
    setLevel(*cam, level);
    cam->camera->setRespawnLevel(0);
    cam->camera->setKeptCount(0);
}

// diep's TankBody::setTank: new barrels and addons, stats over the new caps given back as points
void Tanks::setTank(Entity &t, int id) {
    TankState &ts = *entities_.obj(t).tank;
    const TankDef *def = layout(id);
    if (!def) return;
    ts.def = def;
    ts.layout = id;
    t.tank->setLayout(id);
    if (Entity *cam = driver(t)) {
        DriverData &d = *cam->driver;
        for (int i = 0; i < S_COUNT; i++) {
            int max = def->stats[i].max;
            if (d.stats[i] > max) {
                d.setPoints(d.points + d.stats[i] - max);
                d.setStat(i, max);
            }
        }
        d.setTank(id);
    }
    ts.base = def->baseSize;
    ts.opacity = 1;
    t.tank->setAlpha(255);
    entities_.obj(t).contact->absorb = def->absorbtionFactor;
    entities_.setRadius(t, float(size(ts) * kLength));
    if (def->upgradeMessage[0])
        if (Client *c = clientOf(t)) notify(*c, def->upgradeMessage, 0, 10000);
    buildGun(t, ts);
    if (Entity *cam = driver(t)) setFov(*cam, def->fieldFactor);
    calcStats(t);
}

// diep's CameraEntity::setFieldFactor; florr's camera sees half as far for the same number
void Tanks::setFov(Entity &cam, double field) {
    cam.camera->setFov(float(2 * 0.55 * field / std::pow(1.01, (cam.driver->level - 1) / 2.0)));
}

// diep's TankBody::calculateStatData
void Tanks::calcStats(Entity &t) {
    Obj &o = entities_.obj(t);
    TankState &ts = *o.tank;
    if (ts.boss) return;
    Entity *cam = driver(t);
    int level = cam ? cam->driver->level : 1;
    const TankDef &def = *ts.def;
    o.contact->damage = (stat(t, S_BODY_DAMAGE) + 5 + def.bodyDamage) * kHealth;
    double max = def.maxHealth + 2 * (level - 1) + stat(t, S_MAX_HEALTH) * 20;
    entities_.setMaxHealth(t, float(max * kHealth));
    double maxHp = t.maxHp();
    ts.regen = (maxHp * 4 * stat(t, S_HEALTH_REGEN) + maxHp) / 25000.0;
    ts.reload = 15.0 * std::pow(0.914, stat(t, S_RELOAD));
    ts.speed = def.speed * 2.55 * std::pow(1.07, stat(t, S_MOVEMENT_SPEED)) / std::pow(1.015, level - 1);
    for (Barrel &b : ts.barrels) calcBarrel(t, b, ts.reload);
    for (Turret &tu : ts.turrets) calcBarrel(t, tu.barrel, ts.reload);
}

// diep's CameraEntity::setLevel: the tank grows, gets the level's score and its new stat points
void Tanks::setLevel(Entity &cam, int level) {
    DriverData &d = *cam.driver;
    int prev = d.level;
    d.setLevel(level);
    double score = levelScore(level);
    if (level <= kMaxLevel) d.setScore(float(score));
    Entity *t = world_.get(cam.camera->player);
    if (t && entities_.obj(*t).tank) {
        TankState &ts = *entities_.obj(*t).tank;
        ts.scale *= std::pow(1.01, level - prev);
        entities_.setRadius(*t, float(size(ts) * kLength));
        calcStats(*t);
        if (level <= kMaxLevel) ts.scoreReward = score;
    }
    d.setPoints(d.points + statPoints(level) - statPoints(prev));
    setFov(cam, t && entities_.obj(*t).tank ? entities_.obj(*t).tank->def->fieldFactor : 1);
    levelFromScore(cam);
}

// diep's CameraEntity::calculateLevelData
void Tanks::levelFromScore(Entity &cam) {
    DriverData &d = *cam.driver;
    double score = d.score;
    int level = d.level;
    while (level < kMaxLevel && score - levelScore(level + 1) >= 0) level++;
    if (level != d.level) {
        setLevel(cam, level);
        d.setScore(float(score));
    }
}

void Tanks::giveScore(Entity *cam, double score) {
    if (!cam || !cam->has(C_DRIVER)) return;
    Entity *t = world_.get(cam->camera->player);
    if (!t || !entities_.obj(*t).tank || t->hp() <= 0) return;
    cam->driver->setScore(float(cam->driver->score + score));
    levelFromScore(*cam);
}

// a point on a stat, if there's one to spend and the stat isn't full; `level` is where the client
// wants the stat to end up, so a message that arrives twice does no harm
void Tanks::onStat(Client &c, int which, int level) {
    Entity *cam = world_.get(c.camera);
    Entity *t = world_.get(c.flower);
    if (!cam || !cam->has(C_DRIVER) || !t || !entities_.obj(*t).tank || t->hp() <= 0) return;
    if (which < 0 || which >= S_COUNT) return;
    DriverData &d = *cam->driver;
    const TankDef &def = *entities_.obj(*t).tank->def;
    while (d.points > 0 && d.stats[which] < level && d.stats[which] < def.stats[which].max) {
        d.setStat(which, d.stats[which] + 1);
        d.setPoints(d.points - 1);
    }
    calcStats(*t);
}

void Tanks::onUpgrade(Client &c, int id) {
    Entity *cam = world_.get(c.camera);
    Entity *t = world_.get(c.flower);
    if (!cam || !cam->has(C_DRIVER) || !t || !entities_.obj(*t).tank || t->hp() <= 0) return;
    const TankDef &def = *entities_.obj(*t).tank->def;
    const TankDef *to = tank(id);
    bool listed = false;
    for (int i = 0; i < def.upgradeCount; i++)
        if (def.upgrades[i] == id) listed = true;
    if (!listed || !to || to->levelRequirement > cam->driver->level) return;
    setTank(*t, id);
}

// test keys, for servers started with them on
void Tanks::onDev(Client &c, int what) {
    if (!game_.devKeys) return;
    Entity *cam = world_.get(c.camera);
    Entity *t = world_.get(c.flower);
    if (!cam || !cam->has(C_DRIVER) || !t || !entities_.obj(*t).tank || t->hp() <= 0) return;
    TankState &ts = *entities_.obj(*t).tank;
    if (what == 0 && cam->driver->level < kMaxLevel) {
        setLevel(*cam, cam->driver->level + 1);
    } else if (what == 1) {
        int id = ts.layout;
        do id = (id + kTankCount - 1) % kTankCount;
        while (!tank(id) || tank(id)->devOnly);
        setTank(*t, id);
    } else if (what == 2) {
        game_.combat.damage(*t, t, t->hp(), 1);
    }
}

void Tanks::tick(double dt) {
    dt_ = dt;
    double now = std::floor(game_.now / diep::kTickMs);
    if (clock_ < 0) clock_ = now - 1;
    int n = int(std::min(3.0, now - clock_));
    clock_ = now;
    tickBosses();
    for (int k = 0; k < n; k++) {
        ticks_++;
        for (size_t i = 0; i < all.size(); i++) {
            Entity *t = world_.get(all[i]);
            if (!t) continue;
            Obj &o = entities_.obj(*t);
            if (t->hp() <= 0) {
                o.body->vx *= 0.5;
                o.body->vy *= 0.5;
                continue;
            }
            if (o.tank->boss)
                tickBoss(*t, *o.tank);
            else
                tickTank(*t, *o.tank);
            tickTurrets(*t, *o.tank);
        }
        for (size_t i = 0, end = shots.size(); i < end && i < shots.size(); i++) {
            Entity *e = world_.get(shots[i]);
            if (!e) continue;
            Obj &o = entities_.obj(*e);
            if (e->hp() <= 0) {
                o.body->vx *= 0.5;
                o.body->vy *= 0.5;
                continue;
            }
            tickShot(*e, *o.shot);
        }
    }
}

// diep's TankBody::tick: aim, the zoom, invisibility, the spawn shield, moving, firing, and
// LivingEntity's regen
void Tanks::tickTank(Entity &t, TankState &ts) {
    Client *c = clientOf(t);
    Body &b = *entities_.obj(t).body;
    Contact &ct = *entities_.obj(t).contact;
    const TankDef &def = *ts.def;
    if (Entity *cam = cameraOf(t))
        if (cam->driver) t.tank->setScore(cam->driver->score);
    uint32_t flags = c ? c->input.flags : 0;
    if (c && c->input.dist > 0) {
        ts.mouseX = b.x + std::cos(c->input.angle) * c->input.dist;
        ts.mouseY = b.y + std::sin(c->input.angle) * c->input.dist;
        entities_.setAngle(t, std::atan2(ts.mouseY - b.y, ts.mouseX - b.x));
    } else {
        ts.mouseX = b.x + std::cos(b.angle) * 100;
        ts.mouseY = b.y + std::sin(b.angle) * 100;
    }
    ts.flags = flags;
    bool shooting = flags & 1, repel = flags & 2;
    double mx = 0, my = 0;
    if (flags & kInputUp) my -= 1;
    if (flags & kInputDown) my += 1;
    if (flags & kInputRight) mx += 1;
    if (flags & kInputLeft) mx -= 1;
    bool moving = mx || my;
    if (moving) {
        double a = std::atan2(my, mx);
        mx = std::cos(a);
        my = std::sin(a);
    }

    if (def.zoomAbility && repel) {
        if (!ts.zoomed) {
            double a = std::atan2(ts.mouseY - b.y, ts.mouseX - b.x);
            ts.zoomX = std::cos(a) * 1500 * kLength + b.x;
            ts.zoomY = std::sin(a) * 1500 * kLength + b.y;
            ts.zoomed = true;
        }
    } else {
        ts.zoomed = false;
    }

    if (def.invisibility) {
        if (shooting) ts.opacity += def.visibilityRateShooting;
        if (moving) ts.opacity += def.visibilityRateMoving;
        ts.opacity -= def.invisibilityRate;
        ts.opacity = std::clamp(ts.opacity, 0.0, 1.0);
        t.tank->setAlpha(int(std::lround(ts.opacity * 255)));
    }

    if (ts.shield && (game_.now >= ts.born + 374 * diep::kTickMs || shooting || moving)) {
        ts.shield = false;
        ct.reduction = 1;
        t.tank->setFlags(t.tank->flags & ~1);
    }

    double slow = t.has(C_STATUS) ? game_.effects.slowdown(t) : 1;
    double v = diep::push(ts.speed * slow, diep::decay());
    b.vx += mx * v;
    b.vy += my * v;

    for (size_t i = 0; i < ts.barrels.size(); i++)
        cycle(t, ts.barrels[i], int(i), shooting, ts.fired, int(i), b.angle, b.x, b.y, ts.scale);
    t.tank->setFired(ts.fired);

    float hp = t.hp(), max = t.maxHp();
    if (hp < max) {
        double h = hp + ts.regen;
        if (game_.now - ts.lastHit >= 750 * diep::kTickMs) h += max / 250.0;
        t.health->setHealth(float(std::min<double>(h, max)));
    }
}

// The dead tank's camera gets the death screen and the level to come back at, and whoever
// killed it gets its score.
void Tanks::onDeath(Entity &e, Entity *by) {
    Obj &o = entities_.obj(e);
    if (o.life) o.life->removeAt = game_.now + 6 * diep::kTickMs;
    if (!o.tank || o.shot) return;
    TankState &ts = *o.tank;
    Entity *k = by ? entities_.killerOf(by) : nullptr;
    std::string killer;
    if (k && k->has(C_NAME))
        killer = k->name->name;
    else if (k && entities_.obj(*k).mob)
        killer = kMobs[entities_.obj(*k).mob->type].name;
    if (ts.boss) {
        if (killer.empty() || (k && !k->has(C_TANK))) killer = "an unnamed tank";
        notifyAll(std::string("The ") + ts.def->name + " has been defeated by " + killer + "!");
        if (Entity *kc = by ? cameraOf(*by) : nullptr) giveScore(kc, ts.scoreReward);
        return;
    }
    Entity *cam = driver(e);
    if (cam && cam->camera->player == e.id) {
        DriverData &d = *cam->driver;
        d.setAlive(float((game_.now - ts.born) / 1000));
        d.setDeathTank(ts.layout);
        cam->camera->setScore(d.score);
        cam->camera->setDeathLevel(d.level);
        cam->camera->setKiller(killer);
        entities_.obj(*cam).killer = k && k != &e ? k->id : 0;
        double lvl = d.level;
        cam->camera->setRespawnLevel(int(std::min(std::max(lvl - 1, 1.0), std::floor(std::sqrt(lvl) * 3.2796))));
    }
    Entity *kc = by ? cameraOf(*by) : nullptr;
    if (kc && kc != cam) {
        giveScore(kc, ts.scoreReward);
        for (auto &[id, c] : game_.clients.all)
            if (c.camera == kc->id)
                notify(c, "You've killed " + (e.name->name.empty() ? std::string("an unnamed tank") : e.name->name), 0,
                       10000);
    }
}

// a mob killed by a Necromancer's side becomes one of its drones
void Tanks::onKill(Entity &victim, Entity *by) {
    if (!by || !entities_.obj(victim).mob) return;
    Entity *t = by;
    while (t && !(t->has(C_TANK) && !t->has(C_SHOT)))
        t = t->has(C_OWNED) ? world_.get(entities_.obj(*t).owner) : nullptr;
    if (!t || !entities_.obj(*t).tank || t->hp() <= 0) return;
    if (entities_.obj(*t).tank->def->canClaimSquares) claim(*t, victim);
}

// before something of a tank's is destroyed: it leaves the lists, and a drone frees its place
void Tanks::forget(Entity &e) {
    Obj &o = entities_.obj(e);
    if (o.shot) {
        auto it = std::find(shots.begin(), shots.end(), e.id);
        if (it != shots.end()) {
            *it = shots.back();
            shots.pop_back();
        }
        ShotState &ss = *o.shot;
        if (ss.barrel >= 0)
            if (Entity *g = world_.get(ss.tank))
                if (TankState *gs = entities_.obj(*g).tank.get())
                    for (Barrel &br : gs->barrels)
                        if (int(br.serial) == ss.barrel) br.drones--;
    } else if (o.tank) {
        auto it = std::find(all.begin(), all.end(), e.id);
        if (it != all.end()) {
            *it = all.back();
            all.pop_back();
        }
        if (o.tank->boss) bosses_--;
    }
}

}  // namespace game
