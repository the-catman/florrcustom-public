// Diep's bosses. One comes every one to two minutes, somewhere in the middle of the map, and
// keeps to a patch around that spot.
#include <algorithm>
#include <cmath>

#include "game/Diep.h"
#include "game/Game.h"
#include "game/Tanks.h"

namespace game {
using namespace world;
using namespace tankdata;
using diep::kHealth;
using diep::kLength;

static constexpr double kPi = 3.14159265358979323846;
static constexpr int kMostBosses = 3;
static constexpr double kPatch = 2000;  // diep units each way from its spot

double Tanks::bossGap() {
    return 60000 + rng_.unit() * 60000.0;
}

void Tanks::tickBosses() {
    if (nextBoss_ < 0) nextBoss_ = game_.now + bossGap();
    if (game_.now < nextBoss_) return;
    nextBoss_ = game_.now + bossGap();
    if (bosses_ < kMostBosses)
        spawnBoss(game_.arena.mapW * (rng_.unit() - 0.5) * 0.2, game_.arena.mapH * (rng_.unit() - 0.5) * 0.6);
}

// diep's AbstractBoss and the constructor of each boss
void Tanks::spawnBoss(double hx, double hy) {
    static const int kKinds[] = {L_GUARDIAN, L_SUMMONER, L_DEFENDER, L_FALLEN_OVERLORD, L_FALLEN_BOOSTER};
    int lay = kKinds[std::min(int(rng_.unit() * 5), 4)];
    const TankDef &def = *layout(lay);
    Entity &t = entities_.create(bit(C_NAME) | bit(C_DEATH) | bit(C_TANK));
    Obj &o = entities_.obj(t);
    t.name->setName(def.name);
    entities_.body(t).diep = true;
    entities_.setPos(t, hx, hy);
    entities_.setOwner(t, game_.arena.id);
    o.contact = std::make_unique<Contact>();
    o.contact->absorb = 0.05;
    o.contact->damage = 10 * kHealth;
    o.tank = std::make_unique<TankState>();
    TankState &ts = *o.tank;
    ts.boss = true;
    ts.def = &def;
    ts.layout = lay;
    ts.base = def.baseSize;
    ts.born = game_.now;
    ts.shield = false;
    ts.reload = 15.0 * std::pow(0.914, 7.0);
    ts.scoreReward = 30000;
    ts.homeX = hx;
    ts.homeY = hy;
    ts.aim.view = 2000;
    ts.aim.interval = 0;
    ts.aim.hash = nextHash_++ % 16384;
    ts.aim.turn = rng_.unit() < 0.5 ? 0.01 : -0.01;
    int color = COLOR_FALLEN;
    switch (lay) {
        case L_GUARDIAN: color = COLOR_CRASHER; break;
        case L_SUMMONER: color = COLOR_SQUARE; break;
        case L_DEFENDER:
            color = COLOR_TRIANGLE;
            ts.bossSpeed = 0.2;
            ts.aim.view = 0;
            ts.aim.turn *= 2;
            break;
        case L_FALLEN_BOOSTER: ts.bossSpeed = 1.0; break;
        default: break;
    }
    if (lay == L_FALLEN_OVERLORD || lay == L_FALLEN_BOOSTER) ts.scale = std::pow(1.01, 75 - 1);
    t.tank->setLayout(lay);
    t.tank->setColor(color);
    t.tank->setTeam(0);
    entities_.setMaxHealth(t, float(3000 * kHealth));
    entities_.setRadius(t, float(size(ts) * kLength));
    buildGun(t, ts);
    if (lay == L_DEFENDER) {
        for (int i = 0; i < 3; i++) {
            Turret tu;
            tu.def = &kMountedTurret;
            tu.barrel.def = &kMountedTurret;
            tu.barrel.reload = tu.barrel.pos = ts.reload * kMountedTurret.reload;
            tu.ownerAims = true;
            tu.aim.predict = true;
            tu.aim.hash = nextHash_++ % 16384;
            tu.aim.turn = rng_.unit() < 0.5 ? 0.01 : -0.01;
            double a = 2 * kPi * (i / 3.0);
            tu.aim.mouseX = std::cos(a) * 100;
            tu.aim.mouseY = std::sin(a) * 100;
            tu.x = 60 * std::cos(a);
            tu.y = 60 * std::sin(a);
            ts.turrets.push_back(tu);
        }
    }
    for (Barrel &b : ts.barrels) calcBarrel(t, b, ts.reload);
    for (Turret &tu : ts.turrets) calcBarrel(t, tu.barrel, ts.reload);
    all.push_back(t.id);
    bosses_++;
    notifyAll(std::string("The ") + def.name + " has spawned!");
}

// diep's AbstractBoss::tick and each boss's own: it looks for a target, walks its patch, turns,
// fires and heals
void Tanks::tickBoss(Entity &t, TankState &ts) {
    Body &b = *entities_.obj(t).body;
    think(ts.aim, b.x, b.y, b.x, b.y, team(t), [](double, double) { return true; });
    ts.flags = ts.aim.idle ? 0 : 1;
    ts.mouseX = ts.aim.mouseX;
    ts.mouseY = ts.aim.mouseY;

    // diep's BossMovementControl::moveBoss, round the corners of its patch instead of the map's
    double mx = 0, my = 0;
    auto patrol = [&] {
        double x = (b.x - ts.homeX) / kLength, y = (b.y - ts.homeY) / kLength;
        if (ts.corner < 0) ts.corner = x >= 0 && y >= 0 ? 0 : x <= 0 && y >= 0 ? 3 : x <= 0 && y <= 0 ? 2 : 1;
        double c = kPatch * 3 / 4;
        double tx = ts.corner == 2 || ts.corner == 3 ? -c : c;
        double ty = ts.corner == 1 || ts.corner == 2 ? -c : c;
        double dx = tx - x, dy = ty - y;
        if (dx * dx + dy * dy < 90000.0) {
            ts.corner = (ts.corner + 1) % 4;
        } else {
            double a = std::atan2(dy, dx);
            mx = std::cos(a);
            my = std::sin(a);
        }
    };
    switch (ts.layout) {
        case L_GUARDIAN:
            patrol();
            if (mx || my) entities_.setAngle(t, std::atan2(my, mx));
            break;
        case L_FALLEN_BOOSTER:
            if (ts.aim.idle) {
                patrol();
                if (mx || my) entities_.setAngle(t, std::atan2(my, mx));
            } else {
                entities_.setAngle(t, std::atan2(ts.aim.mouseY - b.y, ts.aim.mouseX - b.x));
            }
            break;
        default:
            patrol();
            entities_.setAngle(t, b.angle + ts.aim.turn);
            break;
    }
    double v = diep::push(ts.bossSpeed, diep::decay());
    b.vx += mx * v;
    b.vy += my * v;

    for (size_t i = 0; i < ts.barrels.size(); i++)
        cycle(t, ts.barrels[i], int(i), !ts.aim.idle, ts.fired, int(i), b.angle, b.x, b.y, ts.scale);
    t.tank->setFired(ts.fired);

    float hp = t.hp(), max = t.maxHp();
    if (hp < max) {
        double h = hp + max / 25000.0;
        if (game_.now - ts.lastHit >= 750 * diep::kTickMs) h += max / 250.0;
        t.health->setHealth(float(std::min<double>(h, max)));
    }
}

}  // namespace game
