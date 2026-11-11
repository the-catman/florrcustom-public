// Barrels, what they fire, auto turrets, how targets are picked, and how diep things meet.
#include <algorithm>
#include <cmath>

#include "Math.h"
#include "game/Diep.h"
#include "game/Game.h"
#include "game/Tanks.h"

namespace game {
using namespace world;
using namespace tankdata;
using diep::kHealth;
using diep::kLength;

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kRest = 400;           // drones further than this from their tank go back
static constexpr double kFocus = 800.0 * 800;  // minions back off inside this
static constexpr double kSkimmerTurn = 0.1;
static constexpr double kPassiveTurn = 0.01;
static constexpr double kForever = 1e300;
static constexpr double kPlayerHits = 0.3;  // how hard players' tanks and what they fire hit, next to diep

double Tanks::bodyKeep(Entity &e) {
    return entities_.obj(e).body->diep ? diep::decay() : diep::kFlorrDecay;
}

// a push of `v` diep units a tick toward `angle`
static void addVelocity(Body &b, double angle, double v, double keep) {
    double k = diep::push(v, keep);
    b.vx += std::cos(angle) * k;
    b.vy += std::sin(angle) * k;
}

uint32_t Tanks::team(Entity &e) {
    return entities_.root(e.id);
}

uint32_t Tanks::dieOwner(Entity &e) {
    return entities_.obj(e).owner;
}

// what a mob is like to a diep thing: as heavy as florr makes it next to a level 1 tank, and as
// hurtful in each diep tick as it is in each of florr's
Contact &Tanks::contactOf(Entity &e, Contact &spare) {
    Obj &o = entities_.obj(e);
    if (o.contact) return *o.contact;
    const Hitbox &h = *o.hit;
    spare = Contact();
    spare.damage = h.damage * diep::kTickMs / 50;
    double mass = double(h.radius) * h.radius * h.density;
    spare.absorb = !h.pushable || h.still || mass <= 0 ? 0 : std::min(1.0, 25.0 * 25 / mass);
    spare.reduction = e.has(C_HEALTH) ? 1 : 0;
    return spare;
}

// diep's ObjectEntity::receiveKnockback
void Tanks::knock(Entity &e, Entity &from, double amount, double s) {
    Body &b = *entities_.obj(e).body, &f = *entities_.obj(from).body;
    double dx = double(b.x) - f.x, dy = double(b.y) - f.y;
    double a = dx == 0 && dy == 0 ? rng_.unit() * 2 * kPi : std::atan2(dy, dx);
    addVelocity(b, a, amount * s, bodyKeep(e));
}

// diep's isColliding, then ObjectEntity's and LivingEntity's handleCollision, over the diep ticks
// this florr tick spans
void Tanks::collide(Entity &a, Entity &b) {
    Contact sa, sb;
    Contact &ca = contactOf(a, sa), &cb = contactOf(b, sb);
    uint32_t ta = team(a), tb = team(b);
    if (ta == tb) {
        if (ca.noOwnTeam || cb.noOwnTeam) return;
        if (dieOwner(a) != dieOwner(b) && (ca.onlySameOwner || cb.onlySameOwner)) return;
    }
    double s = dt_ / diep::kTickMs;
    knock(a, b, ca.absorb * cb.push, s);
    knock(b, a, cb.absorb * ca.push, s);
    if (ta && ta == tb) return;
    float ha = a.hp(), hb = b.hp();
    if (!a.has(C_HEALTH) || !b.has(C_HEALTH) || ha <= 0 || hb <= 0) return;
    if (ca.reduction == 0 && cb.reduction == 0) return;
    if ((ca.damage == 0 && ca.push == 0) || (cb.damage == 0 && cb.push == 0)) return;
    double common = std::max(ca.minMul, cb.minMul) * std::min(ca.maxMul, cb.maxMul);
    auto hits = [&](Entity &e) { return entities_.obj(e).contact && entities_.cameraOf(&e) ? kPlayerHits : 1.0; };
    double fa = ca.damage * s * common * cb.reduction * hits(a), fb = cb.damage * s * common * ca.reduction * hits(b);
    double ratio = std::max(1 - ha / fb, 1 - hb / fa);
    double toB = fa * std::min(1.0, 1 - ratio), toA = fb * std::min(1.0, 1 - ratio);
    auto hurt = [&](Entity &e, Entity &by, Contact &c, double amount) {
        if (e.hp() <= 0.0001f) return;
        if (TankState *ts = entities_.obj(e).tank.get(); ts && !ts->boss && amount > 0 && c.reduction != 0) {
            if (ts->lastHit != game_.now && ts->opacity < 1) ts->opacity = std::min(1.0, ts->opacity + 0.2);
            e.tank->setAlpha(int(std::lround(ts->opacity * 255)));
        }
        float left = float(e.hp() - amount);
        game_.combat.damage(e, &by, left <= 0.0001f ? e.hp() : float(amount), c.quiet ? 1 : 0);
    };
    hurt(a, b, ca, toA);
    hurt(b, a, cb, toB);
}

// barrels and turrets for a tank's class, as diep's setTank adds them
void Tanks::buildGun(Entity &t, TankState &ts) {
    const TankDef &def = *ts.def;
    ts.barrels.clear();
    ts.turrets.clear();
    for (int i = 0; i < def.barrelCount; i++) {
        Barrel b;
        b.def = &def.barrels[i];
        b.reload = b.pos = ts.reload * b.def->reload;
        b.serial = ++serial_;
        ts.barrels.push_back(b);
    }
    auto turret = [&](const BarrelDef &bd, bool ownerAims) -> Turret & {
        Turret tu;
        tu.def = &bd;
        tu.barrel.def = &bd;
        tu.barrel.reload = tu.barrel.pos = ts.reload * bd.reload;
        tu.ownerAims = ownerAims;
        tu.aim.predict = true;
        tu.aim.hash = nextHash_++ % 16384;
        tu.aim.turn = rng_.unit() < 0.5 ? kPassiveTurn : -kPassiveTurn;
        ts.turrets.push_back(tu);
        return ts.turrets.back();
    };
    auto ring = [&](int n) {
        for (int i = 0; i < n; i++) {
            Turret &tu = turret(kAutoTurretMini, true);
            tu.ring = true;
            tu.slot = 2 * kPi * (double(i) / n);
            tu.aim.mouseX = std::cos(tu.slot) * 100;
            tu.aim.mouseY = std::sin(tu.slot) * 100;
            tu.aim.turn = kPassiveTurn;
            tu.x = ts.base * std::cos(tu.slot) * 0.8;
            tu.y = ts.base * std::sin(tu.slot) * 0.8;
        }
    };
    for (Addon a : {def.preAddon, def.postAddon}) {
        switch (a) {
            case A_AUTOTURRET: turret(kAutoTurret, false); break;
            case A_AUTOSMASHER: turret(kAutoTurret, true); break;
            case A_AUTO2: ring(2); break;
            case A_AUTO3: ring(3); break;
            case A_AUTO5: ring(5); break;
            case A_AUTO7: ring(7); break;
            default: break;
        }
    }
    for (int i = 0; i < 8; i++) t.tank->setTurret(i, 0);
}

// diep's Barrel::calculateStatData: the shot rhythm keeps its place when reload changes
void Tanks::calcBarrel(Entity &owner, Barrel &b, double reload) {
    double r = reload * b.def->reload;
    if (r != b.reload) {
        if (b.reload > 0) b.pos *= r / b.reload;
        b.reload = r;
    }
    b.accel = (20 + stat(owner, S_BULLET_SPEED) * 3) * b.def->bullet.speed;
}

// diep's ShootCycle::tick, one diep tick of it
void Tanks::cycle(Entity &owner, Barrel &b, int index, bool trying, uint32_t &fired, int bit, double gunAngle,
                  double gunX, double gunY, double scale) {
    const BarrelDef &d = *b.def;
    double r = b.reload;
    bool always = d.forceFire || d.bullet.kind == B_DRONE || d.bullet.kind == B_MINION;
    if (b.pos >= r) {
        if (!trying && !always) {
            b.pos = r;
            return;
        }
        if (d.droneCount >= 0 && b.drones >= d.droneCount) {
            b.pos = r;
            return;
        }
    }
    if (b.pos >= r * (1 + d.delay)) {
        shoot(owner, b, index, gunAngle, gunX, gunY, scale);
        fired ^= 1u << bit;
        b.pos = r * d.delay;
    }
    b.pos += 1;
}

// diep's Barrel::shoot: a little scatter, the kick back, and the shot
void Tanks::shoot(Entity &owner, Barrel &b, int index, double gunAngle, double gunX, double gunY, double scale) {
    const BarrelDef &d = *b.def;
    double scatter = (kPi / 180) * d.bullet.scatterRate * (rng_.unit() - 0.5) * 10;
    double angle = d.angle + scatter + gunAngle;
    addVelocity(*entities_.obj(owner).body, angle + kPi, d.recoil * 2, bodyKeep(owner));
    makeShot(owner, b, index, angle, gunX, gunY, scale);
}

// diep's Bullet constructor, then what each kind changes
Entity &Tanks::makeShot(Entity &owner, Barrel &b, int index, double angle, double gunX, double gunY, double scale) {
    const BarrelDef &d = *b.def;
    const BulletDef &bd = d.bullet;
    bool barrels = bd.kind == B_MINION || bd.kind == B_SKIMMER || bd.kind == B_ROCKET;
    Entity &e = entities_.create(bit(C_DEATH) | bit(C_SHOT) | (barrels ? bit(C_TANK) : 0));
    Obj &o = entities_.obj(e);
    entities_.setOwner(e, owner.id);
    Body &body = entities_.body(e);
    body.diep = true;
    body.unbounded = true;
    double dmg = stat(owner, S_BULLET_DAMAGE), pen = stat(owner, S_BULLET_PENETRATION);
    double radius = d.width * scale / 2 * bd.sizeRatio;
    o.contact = std::make_unique<Contact>();
    Contact &c = *o.contact;
    c.noOwnTeam = true;
    c.absorb = bd.absorbtionFactor;
    c.push = (7.0 / 3 + dmg) * bd.damage * bd.absorbtionFactor;
    c.damage = (7 + dmg * 3) * bd.damage * kHealth;
    c.minMul = 0.25;
    c.maxMul = 1;
    c.quiet = true;
    entities_.setMaxHealth(e, float((1.5 * pen + 2) * bd.health * kHealth));
    o.shot = std::make_unique<ShotState>();
    ShotState &ss = *o.shot;
    ss.kind = bd.kind;
    ss.tank = owner.id;
    ss.def = &d;
    ss.born = game_.now;
    ss.age = -1;
    ss.life = bd.lifeLength * 75;
    ss.accel = b.accel;
    ss.speed = b.accel + 30 - rng_.unit() * bd.scatterRate;
    ss.angle = angle;
    ss.scale = scale;
    ss.aim.hash = nextHash_++ % 16384;
    ss.aim.turn = rng_.unit() < 0.5 ? kPassiveTurn : -kPassiveTurn;
    double x = gunX + (std::cos(angle) * d.size * scale - std::sin(angle) * d.offset * scale) * kLength;
    double y = gunY + (std::sin(angle) * d.size * scale + std::cos(angle) * d.offset * scale) * kLength;
    entities_.setPos(e, x, y);
    entities_.setAngle(e, angle);

    // its colour and side come from the tank behind it
    Entity *root = &owner;
    while (root && root->has(C_SHOT)) root = world_.get(entities_.obj(*root).owner);
    int color = root && root->has(C_TANK) ? root->tank->color : COLOR_TEAM;
    int look = 1;
    int rootLayout = root && entities_.obj(*root).tank ? entities_.obj(*root).tank->layout : -1;
    uint32_t side = team(e);
    Entity *sideCam = world_.get(side);
    e.shot->setTeam(sideCam && sideCam->has(C_CAMERA) ? side : 0);
    e.shot->setKind(bd.kind);

    auto own = [&](int lay, double reload) {
        const TankDef &ld = *layout(lay);
        ss.reload = reload;
        for (int i = 0; i < ld.barrelCount; i++) {
            Barrel sb;
            sb.def = &ld.barrels[i];
            sb.reload = sb.pos = reload * sb.def->reload;
            calcBarrel(e, sb, reload);
            ss.barrels.push_back(sb);
        }
        e.tank->setLayout(lay);
        e.tank->setTeam(e.shot->team);
        e.tank->setColor(color);
    };

    switch (bd.kind) {
        case B_TRAP:
            ss.speed = b.accel / 2 + 30 - rng_.unit() * bd.scatterRate;
            ss.accel = 0;
            look = 3;
            c.noOwnTeam = false;
            c.onlySameOwner = true;
            c.quiet = false;
            ss.ownUntil = int(ss.life) >> 3;
            ss.life = int(600 * bd.lifeLength) >> 3;
            entities_.setAngle(e, rng_.unit() * 2 * kPi);
            break;
        case B_DRONE:
        case B_NECRODRONE:
        case B_SWARM:
        case B_MINION:
            ss.drone = true;
            ss.facing = true;
            ss.aim.view = 900;
            ss.aim.predict = true;
            ss.controlled = d.canControlDrones;
            look = 3;
            c.noOwnTeam = false;
            c.onlySameOwner = true;
            body.unbounded = false;
            c.quiet = false;
            ss.life = bd.lifeLength != -1 ? 88 * bd.lifeLength : kForever;
            c.push = 4;
            c.absorb = bd.absorbtionFactor;
            ss.speed /= 3;
            if (index >= 0) {
                b.drones++;
                ss.barrel = int(b.serial);
            }
            ss.aim.speed = ss.accel * 9;
            c.minMul = c.maxMul = 1;
            if (bd.kind == B_NECRODRONE) {
                ss.aim.view = 600;
                ss.reach = 1000;
                look = 4;
                color = COLOR_NECRO;
                c.minMul = 1;
                c.maxMul = 4;
                ss.speed = 0;
                ss.restDivisor = 18;
            } else if (bd.kind == B_SWARM) {
                ss.aim.view = 2000;
                ss.reachAll = true;
                body.unbounded = true;
                c.noOwnTeam = true;
                c.push = 1.2;
                ss.facesVelocity = false;
            } else if (bd.kind == B_MINION) {
                ss.facing = false;
                look = 1;
                radius *= 1.2;
                ss.scale = radius / 50;
                ss.aim.speed = ss.accel;
                own(L_MINION, 1);
            }
            break;
        case B_SKIMMER: {
            ss.scale = radius / 50;
            TankState *gs = entities_.obj(owner).tank.get();
            ss.spin = gs && (gs->flags & 2) ? -kSkimmerTurn : kSkimmerTurn;
            ss.firing = true;
            own(L_SKIMMER, 15);
            break;
        }
        case B_ROCKET:
            ss.scale = radius / 50;
            own(L_ROCKET, 1);
            break;
        default: break;
    }
    if (rootLayout == L_SUMMONER) {
        color = COLOR_NECRO;
        look = 4;
    } else if (rootLayout == L_DEFENDER) {
        color = COLOR_NEUTRAL;
    }
    entities_.setRadius(e, float(radius * kLength));
    e.shot->setLook(look);
    e.shot->setColor(color);
    shots.push_back(e.id);
    return e;
}

// a shot is done: diep's destroy(true), florr's death
void Tanks::killShot(Entity &e) {
    if (e.hp() > 0) game_.combat.damage(e, nullptr, e.hp(), 1);
}

// diep's Bullet::tick: launched on its first tick, then kept going
void Tanks::bulletTick(Entity &e, ShotState &ss) {
    Body &b = *entities_.obj(e).body;
    ss.age += 1;
    if (ss.age == 1)
        addVelocity(b, ss.angle, ss.speed, diep::decay());
    else
        addVelocity(b, ss.facing ? b.angle : ss.angle, ss.accel * 0.1, diep::decay());
    if (ss.age >= ss.life) killShot(e);
}

void Tanks::tickShot(Entity &e, ShotState &ss) {
    Entity *gun = world_.get(ss.tank);
    TankState *gs = gun ? entities_.obj(*gun).tank.get() : nullptr;
    ShotState *gss = gun ? entities_.obj(*gun).shot.get() : nullptr;
    double gunReload = gs ? gs->reload : gss ? gss->reload : ss.reload;
    switch (ss.kind) {
        case B_BULLET: bulletTick(e, ss); break;
        case B_TRAP:
            bulletTick(e, ss);
            if (ss.age == ss.ownUntil) {
                Contact &c = *entities_.obj(e).contact;
                c.onlySameOwner = false;
                c.noOwnTeam = true;
            }
            break;
        case B_SKIMMER: {
            if (gun) ss.reload = gunReload;
            Body &b = *entities_.obj(e).body;
            entities_.setAngle(e, b.angle + ss.spin);
            tickOwnBarrels(e, ss);
            bulletTick(e, ss);
            break;
        }
        case B_ROCKET:
            if (gun) ss.reload = gunReload;
            tickOwnBarrels(e, ss);
            bulletTick(e, ss);
            if (ss.age >= ss.reload) ss.firing = true;
            break;
        default: tickDrone(e, ss); break;
    }
}

// a shot's own barrels fire from it, at its tank's reload
void Tanks::tickOwnBarrels(Entity &e, ShotState &ss) {
    if (e.hp() <= 0) return;
    Body &b = *entities_.obj(e).body;
    for (Barrel &br : ss.barrels) {
        br.reload = ss.reload * br.def->reload;
        cycle(e, br, -1, ss.firing, ss.fired, int(&br - ss.barrels.data()), b.angle, b.x, b.y, ss.scale);
    }
    e.tank->setFired(ss.fired);
}

// diep's Drone::tick (and Minion's and Swarm's): with a target or its tank's mouse it heads there;
// with neither it rests near its tank, or a swarm wanders
void Tanks::tickDrone(Entity &e, ShotState &ss) {
    Body &b = *entities_.obj(e).body;
    Entity *gun = world_.get(ss.tank);
    TankState *gs = gun && gun->hp() > 0 ? entities_.obj(*gun).tank.get() : nullptr;
    bool barrelGone = true;
    if (gs)
        for (Barrel &br : gs->barrels)
            if (int(br.serial) == ss.barrel) barrelGone = false;
    if (ss.barrel < 0) barrelGone = !gun;
    uint32_t flags = gs ? gs->flags : 0;
    bool usingAI = !ss.controlled || !gs || (!(flags & 1) && !(flags & 2));

    double tx = gun ? entities_.obj(*gun).body->x : b.x, ty = gun ? entities_.obj(*gun).body->y : b.y;
    double reach = ss.reach * kLength;
    bool all = ss.reachAll;
    think(ss.aim, b.x, b.y, b.x, b.y, team(e), [&](double x, double y) {
        if (all) return true;
        if (!gun) return false;
        double dx = x - tx, dy = y - ty;
        return dx * dx + dy * dy <= reach * reach;
    });

    auto mixin = [&] {
        if (ss.kind != B_MINION) {
            bulletTick(e, ss);
            return;
        }
        // diep's Minion::tickMixin
        if (!gs) {
            bulletTick(e, ss);
            return;
        }
        ss.reload = gs->reload;
        bool ai = !ss.controlled || (!(flags & 1) && !(flags & 2));
        ss.firing = !ss.aim.idle;
        if (ai && ss.aim.idle) {
            ss.angle = b.angle;
        } else {
            ss.firing = true;
            double mx = ai ? ss.aim.mouseX : gs->mouseX, my = ai ? ss.aim.mouseY : gs->mouseY;
            double dx = (mx - b.x) / kLength, dy = (my - b.y) / kLength, d = dx * dx + dy * dy;
            ss.angle = d < kFocus / 7 ? b.angle + kPi : d < kFocus ? b.angle + kPi / 2 : b.angle;
        }
        tickOwnBarrels(e, ss);
        bulletTick(e, ss);
    };

    if (usingAI && ss.aim.idle && gs) {
        double base = ss.accel;
        if (ss.kind == B_SWARM) {
            // diep's Swarm::restingBehavior: straight runs of 20 to 60 ticks, then a sweeping turn
            if (!ss.wandering || b.angle != ss.heading) {
                ss.wandering = true;
                ss.heading = ss.headingTo = b.angle;
                ss.hold = 20 + int(rng_.unit() * 40);
            }
            double diff = std::remainder(ss.headingTo - ss.heading, 2 * kPi);
            if (diff != 0) {
                ss.heading += std::clamp(diff, -0.25, 0.25);
            } else if (--ss.hold <= 0) {
                ss.headingTo = rng_.unit() * 2 * kPi;
                ss.hold = 20 + int(rng_.unit() * 40);
            }
            entities_.setAngle(e, ss.heading);
        } else {
            // diep's Drone::restingBehavior
            double dx = (b.x - tx) / kLength, dy = (b.y - ty) / kLength, d = dx * dx + dy * dy;
            double back = size(*gs) * 1.2;
            if (ss.returning) {
                if (d < back * back) ss.returning = false;
            } else if (d > kRest * kRest) {
                ss.returning = true;
            }
            if (ss.returning) {
                entities_.setAngle(e, std::atan2(-dy, -dx));
            } else {
                ss.accel /= ss.restDivisor;
                entities_.setAngle(e, std::atan2(dy, dx) + kPi / 2);
            }
        }
        if (barrelGone) killShot(e);
        mixin();
        double f = (1 - std::pow(diep::decay(), diep::kTickMs)) / (1 - diep::decay()) / kLength;
        double vx = b.vx * f, vy = b.vy * f;
        if (ss.facesVelocity && vx * vx + vy * vy > 0.01) entities_.setAngle(e, std::atan2(b.vy, b.vx));
        ss.accel = base;
        return;
    }
    double mx = usingAI || !gs ? ss.aim.mouseX : gs->mouseX, my = usingAI || !gs ? ss.aim.mouseY : gs->mouseY;
    entities_.setAngle(e, std::atan2(my - b.y, mx - b.x));
    ss.returning = false;
    if (ss.controlled && !usingAI && (flags & 2)) entities_.setAngle(e, b.angle + kPi);
    if (barrelGone) killShot(e);
    mixin();
}

// a Necromancer's kill comes back as one of its drones, looking as it did and as strong as a square makes one
void Tanks::claim(Entity &t, Entity &victim) {
    TankState &ts = *entities_.obj(t).tank;
    int most = 11 + stat(t, S_RELOAD);
    std::vector<int> free;
    for (size_t i = 0; i < ts.barrels.size(); i++)
        if (ts.barrels[i].def->bullet.kind == B_NECRODRONE && ts.barrels[i].drones < most) free.push_back(int(i));
    if (free.empty()) return;
    int i = free[std::min<size_t>(size_t(rng_.unit() * free.size()), free.size() - 1)];
    Body &vb = *entities_.obj(victim).body;
    Body &tb = *entities_.obj(t).body;
    Entity &d = makeShot(t, ts.barrels[i], i, vb.angle, tb.x, tb.y, ts.scale);
    entities_.setPos(d, vb.x, vb.y);
    entities_.setAngle(d, vb.angle);
    entities_.setRadius(d, victim.size->radius);
    d.shot->setMob(victim.mob->type + 1);
    constexpr double kSquareDamage = 8;
    entities_.obj(d).contact->damage *= kSquareDamage / 2;
    entities_.setMaxHealth(d, float(d.maxHp() * kSquareDamage / 2));
    d.health->setHealth(d.maxHp());
    entities_.queueRemoval(victim);
}

// diep's AutoTurret::tick, with the ring that turns under ring turrets
void Tanks::tickTurrets(Entity &t, TankState &ts) {
    if (ts.turrets.empty()) return;
    Body &b = *entities_.obj(t).body;
    ts.ring += kPassiveTurn;
    double frame = ts.boss ? b.angle : 0;
    for (size_t i = 0; i < ts.turrets.size(); i++) {
        Turret &tu = ts.turrets[i];
        double fa = tu.ring ? ts.ring : frame;
        double ox = (tu.x * std::cos(fa) - tu.y * std::sin(fa)) * ts.scale * kLength;
        double oy = (tu.x * std::sin(fa) + tu.y * std::cos(fa)) * ts.scale * kLength;
        double wx = b.x + ox, wy = b.y + oy;
        auto ok = [&](double x, double y) {
            if (!tu.ring) return true;
            double a = std::atan2(y - wy, x - wx) - (tu.slot + ts.ring);
            a = std::fmod(std::fmod(a, 2 * kPi) + 2 * kPi, 2 * kPi);
            return a < kPi / 2 || a > 2 * kPi - kPi / 2;
        };
        think(tu.aim, b.x, b.y, wx, wy, team(t), ok);
        if (!tu.aim.idle) tu.aim.turn = rng_.unit() < 0.5 ? kPassiveTurn : -kPassiveTurn;
        tu.aim.speed = tu.barrel.accel;
        calcBarrel(t, tu.barrel, ts.reload);
        bool useAI = !(tu.ownerAims && (ts.flags & 3));
        bool trying = false;
        if (!useAI) {
            double flip = ts.flags & 2 ? -1 : 1;
            double dx = (ts.mouseX - wx) * flip, dy = (ts.mouseY - wy) * flip;
            if (!ok(wx + dx, wy + dy)) {
                useAI = true;
            } else {
                trying = true;
                tu.angle = std::atan2(dy, dx);
                tu.aim.idle = false;
            }
        }
        if (useAI) {
            if (tu.aim.idle) {
                tu.angle = tu.ring ? tu.slot + ts.ring + tu.aim.turn : tu.angle + tu.aim.turn;
            } else {
                tu.angle = std::atan2(tu.aim.mouseY - wy, tu.aim.mouseX - wx);
                trying = true;
            }
        }
        cycle(t, tu.barrel, -1, trying, ts.fired, 16 + int(i), tu.angle, wx, wy, ts.scale);
        if (i < 8) t.tank->setTurret(int(i), tu.angle);
    }
    t.tank->setFired(ts.fired);
    t.tank->setSpin(ts.ring);
}

// diep's AI::tick: aims at a target if it has one, and otherwise turns idly
void Tanks::think(Aim &aim, double rootX, double rootY, double fromX, double fromY, uint32_t side,
                  const std::function<bool(double, double)> &ok) {
    Entity *t = findTarget(aim, rootX, rootY, side, ok);
    if (!t) {
        aim.idle = true;
        aim.target = 0;
        double a = std::atan2(aim.mouseY, aim.mouseX) + aim.turn;
        aim.mouseX = std::cos(a) * 100;
        aim.mouseY = std::sin(a) * 100;
        return;
    }
    aim.idle = false;
    aimAt(aim, *t, fromX, fromY);
}

// diep's AI::findTarget: the old target while it stays fair game, or the nearest tank in view,
// or the nearest anything else
Entity *Tanks::findTarget(Aim &aim, double rootX, double rootY, uint32_t side,
                          const std::function<bool(double, double)> &ok) {
    auto alive = [&](Entity *t) { return t && t->has(C_HEALTH) && t->hp() > 0; };
    auto seen = [&](Entity &t) {
        TankState *ts = entities_.obj(t).tank.get();
        return !ts || ts->opacity > 0;
    };
    Entity *old = world_.get(aim.target);
    if (aim.interval != 0 && (ticks_ + aim.hash) % aim.interval != 1) {
        if (alive(old)) return old;
        aim.target = 0;
        return nullptr;
    }
    double view = aim.view * kLength;
    if (alive(old) && team(*old) != side && seen(*old)) {
        const Body &ob = *entities_.obj(*old).body;
        double dx = ob.x - rootX, dy = ob.y - rootY;
        if (ok(ob.x, ob.y) && dx * dx + dy * dy < view * view * 2) return old;
    }
    Entity *player = nullptr, *other = nullptr;
    double bestPlayer = view * view, bestOther = view * view;
    game_.physics.tree.visit(float(rootX - view), float(rootY - view), float(view * 2), float(view * 2),
                             [&](uint16_t s) {
                                 Entity &d = *world_.at(s);
                                 const Obj &o = entities_.objs[s];
                                 if (!o.body || !o.hit || o.hit->ghost || d.has(C_SHOT)) return false;
                                 if (!alive(&d) || team(d) == side || !seen(d)) return false;
                                 if (!ok(o.body->x, o.body->y)) return false;
                                 double dx = o.body->x - rootX, dy = o.body->y - rootY, n = dx * dx + dy * dy;
                                 if (d.isPlayer()) {
                                     if (n < bestPlayer) {
                                         player = &d;
                                         bestPlayer = n;
                                     }
                                 } else if (n < bestOther) {
                                     other = &d;
                                     bestOther = n;
                                 }
                                 return false;
                             });
    Entity *t = player ? player : other;
    aim.target = t ? t->id : 0;
    return t;
}

// diep's AI::aimAtTarget: leads a moving target when it predicts
void Tanks::aimAt(Aim &aim, Entity &target, double fromX, double fromY) {
    const Body &tb = *entities_.obj(target).body;
    double speed = aim.speed * 1.6 * kLength;
    if (speed <= 0.001 * kLength || !aim.predict) {
        aim.mouseX = tb.x;
        aim.mouseY = tb.y;
        return;
    }
    double dx = tb.x - fromX, dy = tb.y - fromY, dist = std::sqrt(dx * dx + dy * dy);
    if (dist == 0) dist = 1;
    double px = dy / dist, py = -dx / dist;
    double keep = tb.diep ? diep::decay() : diep::kFlorrDecay;
    double f = (1 - std::pow(keep, diep::kTickMs)) / (1 - keep);
    double vx = tb.vx * f + tb.ax * diep::kTickMs, vy = tb.vy * f + tb.ay * diep::kTickMs;
    double side = std::clamp(px * vx + py * vy, speed * -0.9, speed * 0.9);
    double direct = std::sqrt(speed * speed - side * side);
    double off = side / direct * dist / 2;
    aim.mouseX = tb.x + off * px;
    aim.mouseY = tb.y + off * py;
}

}  // namespace game
