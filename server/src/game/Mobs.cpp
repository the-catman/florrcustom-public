#include "game/Mobs.h"

#include <cmath>

#include "Math.h"
#include "game/Game.h"

namespace game {
using namespace world;

// how many mobs each zone holds
static const int kZoneMobs[8] = {120, 120, 119, 120, 119, 119, 40, 40};

// another ant comes out each time the hole's health drops past the next mark
struct Hatch {
    float below;
    uint8_t kind;
};
static const Hatch kHatch[33] = {
    {0.97f, ANT_BABY},    {0.95f, ANT_WORKER},  {0.93f, ANT_BABY},    {0.9f, ANT_WORKER},   {0.87f, ANT_WORKER},
    {0.83f, ANT_WORKER},  {0.8f, ANT_SOLDIER},  {0.77f, ANT_SOLDIER}, {0.73f, ANT_SOLDIER}, {0.67f, ANT_SOLDIER},
    {0.63f, ANT_WORKER},  {0.6f, ANT_SOLDIER},  {0.57f, ANT_SOLDIER}, {0.53f, ANT_SOLDIER}, {0.47f, ANT_SOLDIER},
    {0.44f, ANT_WORKER},  {0.41f, ANT_SOLDIER}, {0.38f, ANT_SOLDIER}, {0.36f, ANT_QUEEN},   {0.32f, ANT_SOLDIER},
    {0.3f, ANT_SOLDIER},  {0.27f, ANT_SOLDIER}, {0.24f, ANT_SOLDIER}, {0.2f, ANT_SOLDIER},  {0.26f, ANT_SOLDIER},
    {0.16f, ANT_SOLDIER}, {0.1f, ANT_SOLDIER},  {0.07f, ANT_SOLDIER}, {0.04f, ANT_SOLDIER}, {0.02f, ANT_SOLDIER},
    {0.0f, ANT_SOLDIER},  {0.0f, ANT_SOLDIER},  {0.0f, ANT_SOLDIER},
};

// the settings most moving mobs start from
static AiParams walker() {
    AiParams p;
    p.faceMove = true;
    p.spin = 0.2f;
    p.speed = 1.0f;
    p.pulse = true;
    p.pulseRate = 0.002f;
    p.pulsePow = 2.0f;
    p.leadSpeed = 1.0f;
    p.chaseSpeed = 2.4f;
    p.sight = 1000;
    p.giveUp = 1.5f;
    p.leash = 500;
    p.guard = 1000;
    p.attackPeriod = 1000;
    return p;
}

Mobs::Mobs(Game &game) : System(game) {}

// a dead centipede segment lets go of the ones behind it, which carry on as heads
void Mobs::detachSegments(Entity &e) {
    Obj &o = entities_.obj(e);
    if (Entity *next = world_.get(o.segNext)) entities_.obj(*next).segPrev = 0;
    o.segNext = 0;
    if (Entity *prev = world_.get(o.segPrev)) {
        Obj &p = entities_.obj(*prev);
        if (p.segNext == e.id) p.segNext = 0;
    }
    o.segPrev = 0;
}

// tops every zone back up to its share of the map's area
void Mobs::spawn() {
    for (int z = 0; z < 8; z++) {
        if (kZoneMobs[z] <= zoneCount[z]) continue;
        while (spawnInZone(z) && zoneCount[z] < kZoneMobs[z]) {
        }
    }
}

// one mob from the zone's table, at a random free spot in the zone's strip of the map
bool Mobs::spawnInZone(int z) {
    const SpawnTable &t = kSpawns[z];
    float k = t.total * rng_.unit();
    if (!t.count) return false;
    const SpawnEntry *pick = nullptr;
    for (int i = 0; i < t.count; i++) {
        if (k < t.entries[i].weight) {
            pick = &t.entries[i];
            break;
        }
        k = k - t.entries[i].weight;
    }
    if (!pick) return false;
    float lo = 0, hi = 0.15f;
    if (z > 0) {
        lo = kZoneLo[z - 1];
        hi = kZoneHi[z - 1];
    }
    for (int blocked = 0;;) {
        float x = game_.arena.mapW * (rng_.real(lo, hi) - 0.5f);
        float y = game_.arena.mapH * (rng_.unit() - 0.5f);
        if (!spotTaken(x, y)) return make(pick->mob, x, y, z) != nullptr;
        if (++blocked == 100) return false;
    }
}

// a player within 600 or a mob within 100 of the spot
bool Mobs::spotTaken(float x, float y) {
    for (uint16_t s : world_.slots()) {
        const Entity &e = *world_.at(s);
        const Obj &o = entities_.objs[s];
        if (!o.hit || !game_.physics.tree.contains(s)) continue;
        float d;
        if (e.has(C_FLOWER))
            d = 600;
        else if (e.has(C_MOB))
            d = 100;
        else
            continue;
        float dx = float(o.body->x) - x, dy = float(o.body->y) - y, rr = d + o.hit->radius;
        if (dx * dx + dy * dy < rr * rr) return true;
    }
    return false;
}

// a player close to where a centipede segment would go
bool Mobs::nearFlower(double x, double y) {
    float bx = float(x - 35), by = float(y - 35);
    return game_.physics.tree.visit(bx, by, 70, 70, [&](uint16_t s) {
        const Obj &o = entities_.objs[s];
        if (!world_.at(s)->has(C_FLOWER)) return false;
        float dx = float(o.body->x) - float(x), dy = float(o.body->y) - float(y), rr = o.hit->radius + 45.0f;
        return dx * dx + dy * dy < rr * rr;
    });
}

Entity *Mobs::make(uint8_t type, double x, double y, int zone) {
    const MobDef &def = kMobs[type];
    Entity &e =
        entities_.create(bit(C_POSITION) | bit(C_DEATH) | bit(C_STATUS) | bit(C_MOB) | bit(C_NAME) | bit(C_LOOT));
    entities_.setOwner(e, game_.arena.id);
    if (std::isfinite(x) && std::isfinite(y)) entities_.setPos(e, x, y);
    Obj &o = entities_.obj(e);
    o.mob = std::make_unique<Mob>();
    o.mob->type = type;
    o.mob->zone = int8_t(zone);
    e.mob->setType(type);
    e.name->setName(def.name);
    o.loot = std::make_unique<Loot>();
    o.loot->xp = def.xp;
    float boost = 1.0f;  // three times the drops in the other game mode
    for (int i = 0; i < def.dropCount; i++) {
        if (o.loot->petals.size() > 5) break;
        if (rng_.unit() < boost * def.drops[i].chance) o.loot->petals.push_back(def.drops[i].petal);
    }
    init(e, type);
    if (zone >= 0 && zone <= 7) zoneCount[zone]++;
    game_.physics.treeAdd(e);
    return &e;
}

void Mobs::init(Entity &e, uint8_t type) {
    switch (type) {
        case M_ROCK: initRock(e, false); break;
        case M_BOULDER: initRock(e, true); break;
        case M_CACTUS: initCactus(e); break;
        case M_LADYBUG: initLadybug(e, M_LADYBUG); break;
        case M_MASSIVE_LADYBUG: initLadybug(e, M_MASSIVE_LADYBUG); break;
        case M_BEE: initBee(e); break;
        case M_BABY_ANT: initAnt(e, ANT_BABY); break;
        case M_WORKER_ANT: initAnt(e, ANT_WORKER); break;
        case M_SOLDIER_ANT: initAnt(e, ANT_SOLDIER); break;
        case M_QUEEN_ANT: initAnt(e, ANT_QUEEN); break;
        case M_ANT_HOLE: initAntHole(e); break;
        case M_BEETLE: initBeetle(e, false); break;
        case M_MASSIVE_BEETLE: initBeetle(e, true); break;
        case M_HORNET: initHornet(e); break;
        case M_CENTIPEDE: initCentipede(e, false, true); break;
        case M_CENTIPEDE_BODY: initCentipede(e, false, false); break;
        case M_EVIL_CENTIPEDE: initCentipede(e, true, true); break;
        case M_EVIL_CENTIPEDE_BODY: initCentipede(e, true, false); break;
        case M_SQUARE: initSquare(e); break;
        case M_DARK_LADYBUG: initLadybug(e, M_DARK_LADYBUG); break;
        case M_SHINY_LADYBUG: initLadybug(e, M_SHINY_LADYBUG); break;
        case M_SPIDER: initSpider(e); break;
        default: break;
    }
}

void Mobs::initRock(Entity &e, bool boulder) {
    e.add(C_ROCK);
    float r = boulder ? rng_.real(60.0f, 100.0f) : rng_.real(20.0f, 40.0f);
    entities_.setMaxHealth(e, float((boulder ? 2.0 : 1.0) * double(r) * 0.4));
    Hitbox &h = entities_.hitbox(e);
    h.damage = 10;
    entities_.setRadius(e, r);
    h.pushable = false;
}

void Mobs::initCactus(Entity &e) {
    e.add(C_CACTUS);
    double r = double(rng_.real(40.0f, 60.0f));
    entities_.setMaxHealth(e, float(r * 0.7));
    Hitbox &h = entities_.hitbox(e);
    h.damage = 30;
    entities_.setRadius(e, float(r));
    h.pushable = false;
}

// kind -1 is the massive one; 1 and 2 are the rarer colours
void Mobs::initLadybug(Entity &e, int type) {
    e.add(C_LADYBUG);
    entities_.hitbox(e).damage = 10;
    AiParams p = walker();
    if (type == M_MASSIVE_LADYBUG) {
        entities_.setMaxHealth(e, 1000);
        entities_.setRadius(e, 120);
        p.pulseRate = p.pulseRate * 0.5f;
        p.speed = float(double(p.speed) * 0.2);
    } else {
        if (type == M_DARK_LADYBUG || type == M_SHINY_LADYBUG) {
            p.fightsBack = true;
            entities_.setMaxHealth(e, 35);
        } else
            entities_.setMaxHealth(e, 25);
        entities_.setRadius(e, 30);
    }
    p.headingPeriod = float(TAU / double(p.pulseRate));
    game_.mobAi.init(e, p);
}

void Mobs::initBee(Entity &e) {
    e.add(C_BEE);
    entities_.setMaxHealth(e, 15);
    entities_.hitbox(e).damage = 50;
    entities_.setRadius(e, 25);
    AiParams p = walker();
    p.wobble = true;
    p.wobbleRate = 0.002f;
    p.wobbleSize = 0.5f;
    p.pulseRate = 0.0028723683f;
    p.pulsePow = 0.5f;
    p.headingPeriod = rng_.real(5000.0f, 10000.0f);
    game_.mobAi.init(e, p);
}

void Mobs::initAnt(Entity &e, int kind) {
    e.add(C_ANT);
    e.ant->setKind(kind);
    AiParams p = walker();
    p.pulseRate = float(double(rng_.real(0.9f, 1.1f)) * 0.002);
    p.headingPeriod = float(TAU / double(p.pulseRate));
    switch (kind) {
        case ANT_BABY:
            entities_.setMaxHealth(e, 10);
            entities_.setRadius(e, 14);
            break;
        case ANT_WORKER:
            entities_.setMaxHealth(e, 25);
            entities_.setRadius(e, 20);
            p.fightsBack = true;
            break;
        case ANT_SOLDIER:
            entities_.setMaxHealth(e, 40);
            entities_.setRadius(e, 20);
            p.sight = 300;
            p.hunts = true;
            p.minZone = 0.3f;
            break;
        case ANT_QUEEN:
            entities_.setMaxHealth(e, 200);
            entities_.setRadius(e, 40);
            p.sight = 400;
            p.hunts = true;
            p.minZone = 0.3f;
            break;
        default: break;
    }
    entities_.hitbox(e).damage = 10;
    game_.mobAi.init(e, p);
}

void Mobs::initAntHole(Entity &e) {
    e.add(C_ANT_HOLE);
    holes.push_back({e.id, 0});
    entities_.setMaxHealth(e, 300);
    Hitbox &h = entities_.hitbox(e);
    entities_.setRadius(e, 40);
    h.pushable = false;
    h.hitsTeam = false;
    h.damage = 15;
    hatchAnt(e, ANT_BABY);
    hatchAnt(e, ANT_BABY);
    hatchAnt(e, ANT_BABY);
    hatchAnt(e, ANT_WORKER);
    hatchAnt(e, ANT_WORKER);
    hatchAnt(e, ANT_SOLDIER);
}

// an ant crawling out of its hole with a small push in a random direction
void Mobs::hatchAnt(Entity &hole, int kind) {
    const Body &hb = entities_.body(hole);
    Entity *ant = make(uint8_t(M_BABY_ANT + kind), hb.x, hb.y, -1);
    float a = rng_.angle();
    if (!ant) return;
    entities_.setOwner(*ant, hole.id);
    Body &b = entities_.body(*ant);
    b.vx += double(std::cos(a) * 0.25f);
    b.vy += double(std::sin(a) * 0.25f);
    entities_.obj(*ant).ai->leader = hole.id;
}

void Mobs::initBeetle(Entity &e, bool massive) {
    e.add(C_BEETLE);
    entities_.setMaxHealth(e, massive ? 300 : 40);
    entities_.hitbox(e).damage = massive ? 20 : 30;
    entities_.setRadius(e, massive ? 80 : 40);
    AiParams p = walker();
    p.attackPeriod = 1000;
    p.guard = 1000;
    p.leash = 500;
    p.speed = massive ? 1.1f : 1.0f;
    p.sight = 600;
    p.hunts = true;
    p.minZone = 0.3f;
    p.headingPeriod = 3141.5925f;
    game_.mobAi.init(e, p);
}

void Mobs::initHornet(Entity &e) {
    e.add(C_HORNET);
    entities_.setMaxHealth(e, 40);
    entities_.hitbox(e).damage = 50;
    entities_.setRadius(e, 25);
    AiParams p = walker();
    p.headingPeriod = rng_.real(5000.0f, 10000.0f);
    p.leads = rng_.unit() < 0.2;
    p.leadSpeed = rng_.real(0.5f, 0.7f);
    p.hunts = true;
    p.spin = 0.2f;
    p.speed = 0.7f;
    p.wobble = true;
    p.wobbleRate = 0.002f;
    p.wobbleSize = 0.5f;
    p.pulseRate = 0.0028723683f;
    p.pulsePow = 0.5f;
    p.minZone = 0.6f;
    p.orbitDist = 500;
    p.sight = 600;
    bool left = rng_.between(0, 1) == 1;
    p.attackPeriod = 2000;
    p.shoots = true;
    p.orbitSpeed = left ? 0.4f : -0.4f;
    game_.mobAi.init(e, p);
}

// A hornet's missile leaves from behind it and flies the way it faces, pushing it back a little.
void Mobs::shootMissile(Entity &h) {
    Body &hb = entities_.body(h);
    float a = float(hb.angle);
    Entity &m = entities_.create(bit(C_MISSILE) | bit(C_DEATH));
    entities_.setOwner(m, h.id);
    Body &mb = entities_.body(m);
    mb.unbounded = true;
    Hitbox &mh = entities_.hitbox(m);
    entities_.life(m);
    float c = std::cos(a), s = std::sin(a);
    hb.vx += double(c * -0.4f);
    hb.vy += double(s * -0.4f);
    double y = double(hb.y) - double(s * 20.0f), x = double(hb.x) - double(c * 20.0f);
    if (std::isfinite(x) && std::isfinite(y)) entities_.setPos(m, x, y);
    entities_.setRadius(m, 11);
    mh.hitsTeam = false;
    mh.damage = 10;
    entities_.setMaxHealth(m, 10);
    entities_.life(m).fuse = 2000;
    entities_.setAngle(m, std::fmod(double(a), TAU));
    mb.ay = double(s * 0.6f);
    mb.ax = double(c * 0.6f);
    mb.vy = double(s * 1.4f);
    mb.vx = double(c * 1.4f);
}

// a head lays its body out behind it: 9 segments, or 49 one time in a hundred
void Mobs::initCentipede(Entity &e, bool evil, bool head) {
    bool longOne = false;
    if (head) longOne = rng_.unit() < 0.01;
    e.add(C_SEGMENT);
    entities_.setMaxHealth(e, 50);
    entities_.hitbox(e).damage = 10;
    entities_.setRadius(e, 35);
    AiParams p;
    p.turn = 0.006f;
    p.faceMove = true;
    p.spin = 0.2f;
    p.speed = 0.2f;
    p.pulseRate = 0;
    p.pulsePow = 1.0f;
    p.wobble = true;
    p.wobbleRate = 0.0001f;
    p.wobbleSize = 0.6f;
    p.leadSpeed = 1.0f;
    p.chaseSpeed = 2.4f;
    p.sight = 1000;
    p.giveUp = 1.5f;
    p.leash = 500;
    p.guard = 1000;
    p.attackPeriod = 1000;
    if (evil) {
        p.sight = 600;
        p.minZone = 0.3f;
        p.hunts = true;
        p.speed = 0.16f;
    }
    game_.mobAi.init(e, p);
    e.segment->setFlags((evil ? 1 : 0) | (head ? 2 : 0));
    if (!head) return;
    float a = rng_.angle();
    uint8_t bodyType = evil ? M_EVIL_CENTIPEDE_BODY : M_CENTIPEDE_BODY;
    double step = longOne ? TAU / 100 : TAU / 20;
    int count = longOne ? 49 : 9;
    Entity *prev = &e;
    for (int i = 0; i < count; i++) {
        const Body &pb = entities_.body(*prev);
        double r = double(a);
        double y = std::sin(r) * 70.0 + pb.y, x = pb.x + std::cos(r) * 70.0;
        float hw = game_.arena.mapW * 0.5f, hh = game_.arena.mapH * 0.5f;
        if (x < double(35.0f - hw) || y < double(35.0f - hh) || x > double(hw - 35.0f) || y > double(hh - 35.0f))
            return;
        if (nearFlower(x, y)) return;
        Entity *seg = make(bodyType, x, y, -1);
        if (!seg) return;
        entities_.setOwner(*seg, prev->id);
        entities_.obj(*seg).segPrev = prev->id;
        entities_.obj(*prev).segNext = seg->id;
        a = float(step + r);
        prev = seg;
    }
}

void Mobs::initSquare(Entity &e) {
    e.name->setName("Square");
    entities_.setMaxHealth(e, 15);
    entities_.hitbox(e).damage = 10;
    entities_.setRadius(e, 30);
    AiParams p;
    p.turn = 0.123207994f;
    p.spin = 0.2f;
    p.speed = 0.2f;
    p.pulseRate = 0;
    p.pulsePow = 1.0f;
    p.leadSpeed = 1.0f;
    p.chaseSpeed = 2.4f;
    p.sight = 1000;
    p.giveUp = 1.5f;
    p.leash = 500;
    p.guard = 1000;
    p.attackPeriod = 1000;
    game_.mobAi.init(e, p);
}

void Mobs::initSpider(Entity &e) {
    e.add(C_SPIDER);
    entities_.setMaxHealth(e, 20);
    entities_.hitbox(e).damage = 25;
    entities_.setRadius(e, 25);
    AiParams p = walker();
    p.chaseSpeed = 2.88f;
    p.sight = 600;
    p.minZone = 0.45f;
    p.hunts = true;
    p.headingPeriod = 3141.5925f;
    game_.mobAi.init(e, p);
}

// Centipedes: from each head down, every segment is pulled to just touch the one in front and
// turned to face it.
void Mobs::moveSegments() {
    for (uint16_t s : world_.slots()) {
        Obj &o = entities_.objs[s];
        if (!o.body || world_.get(o.segPrev)) continue;
        if (Entity *next = world_.get(o.segNext)) follow(*next);
    }
}

void Mobs::follow(Entity &seg) {
    Obj &o = entities_.obj(seg);
    if (Entity *lead = world_.get(o.segPrev)) {
        float gap = o.hit ? o.hit->radius + 0.0001f : 0.0001f;
        Obj &l = entities_.obj(*lead);
        if (l.hit) gap = gap + l.hit->radius;
        Body &b = *o.body;
        const Body &lb = *l.body;
        double dy = double(lb.y) - double(b.y), dx = double(lb.x) - double(b.x);
        float a = float(std::atan2(dy, dx));
        entities_.setAngle(seg, std::fmod(double(a), TAU));
        double d2 = dx * dx + dy * dy, g = double(gap);
        if (d2 > g * g) {
            double d = std::sqrt(d2), ux, uy;
            if (d < EPSILON) {
                ux = 1.0;
                uy = 0.0;
            } else {
                ux = dx / d;
                uy = dy / d;
            }
            double y = double(lb.y) - uy * g, x = double(lb.x) - ux * g;
            if (std::isfinite(x) && std::isfinite(y)) entities_.setPos(seg, x, y);
        }
    }
    if (Entity *next = world_.get(o.segNext)) follow(*next);
}

void Mobs::hatchFromHoles() {
    for (size_t i = 0; i < holes.size(); i++) {
        Hole &h = holes[i];
        Entity *e = world_.get(h.id);
        if (!e || h.hatched > 32) continue;
        float left = e->hp() / e->maxHp();
        while (!(left > kHatch[h.hatched].below)) {
            int kind = kHatch[h.hatched].kind;
            hatchAnt(*e, kind);
            if (++h.hatched >= 33) break;
        }
    }
}

}  // namespace game
