// Diep's tanks in florr's world: players' tanks with their levels, stats and classes, everything
// they fire, their turrets, and the bosses. Their behaviour runs in whole diep ticks; florr's
// physics moves them in between.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "game/Components.h"
#include "game/System.h"

namespace game {

class Tanks : public System {
public:
    explicit Tanks(Game &game);

    void spawn(Client &c);
    void onStat(Client &c, int stat, int level);
    void onUpgrade(Client &c, int id);
    void onDev(Client &c, int what);
    void tick(double dt);
    // two things touching, at least one of them a tank or a shot
    void collide(world::Entity &a, world::Entity &b);
    // a tank or a shot has just died
    void onDeath(world::Entity &e, world::Entity *by);
    // a mob was killed
    void onKill(world::Entity &victim, world::Entity *by);
    // something about to be destroyed
    void forget(world::Entity &e);
    void giveScore(world::Entity *camera, double score);
    void notify(Client &c, const std::string &text, uint32_t color = 0, float ms = 10000);
    void notifyAll(const std::string &text, uint32_t color = 0, float ms = 10000);

    std::vector<uint32_t> all;    // tanks and bosses
    std::vector<uint32_t> shots;  // everything they fired

private:
    // tanks
    void setTank(world::Entity &t, int id);
    void setFov(world::Entity &cam, double field);
    void calcStats(world::Entity &t);
    void setLevel(world::Entity &cam, int level);
    void levelFromScore(world::Entity &cam);
    void tickTank(world::Entity &t, TankState &ts);
    Client *clientOf(world::Entity &t);
    world::Entity *cameraOf(world::Entity &e);
    world::Entity *driver(world::Entity &t);
    int stat(world::Entity &e, int which);
    static double size(const TankState &ts) { return ts.base * ts.scale; }

    // barrels and what they fire
    void buildGun(world::Entity &t, TankState &ts);
    void calcBarrel(world::Entity &owner, Barrel &b, double reload);
    void cycle(world::Entity &owner, Barrel &b, int index, bool trying, uint32_t &fired, int bit, double gunAngle,
               double gunX, double gunY, double scale);
    void shoot(world::Entity &owner, Barrel &b, int index, double gunAngle, double gunX, double gunY, double scale);
    world::Entity &makeShot(world::Entity &owner, Barrel &b, int index, double angle, double gunX, double gunY,
                            double scale);
    void killShot(world::Entity &e);
    void bulletTick(world::Entity &e, ShotState &ss);
    void tickShot(world::Entity &e, ShotState &ss);
    void tickDrone(world::Entity &e, ShotState &ss);
    void tickOwnBarrels(world::Entity &e, ShotState &ss);
    void tickTurrets(world::Entity &t, TankState &ts);
    void claim(world::Entity &t, world::Entity &victim);

    // targets
    void think(Aim &aim, double rootX, double rootY, double fromX, double fromY, uint32_t side,
               const std::function<bool(double, double)> &ok);
    world::Entity *findTarget(Aim &aim, double rootX, double rootY, uint32_t side,
                              const std::function<bool(double, double)> &ok);
    void aimAt(Aim &aim, world::Entity &target, double fromX, double fromY);

    // bosses
    double bossGap();
    void tickBosses();
    void spawnBoss(double x, double y);
    void tickBoss(world::Entity &t, TankState &ts);

    // meeting
    uint32_t team(world::Entity &e);
    uint32_t dieOwner(world::Entity &e);
    Contact &contactOf(world::Entity &e, Contact &spare);
    void knock(world::Entity &e, world::Entity &from, double amount, double s);
    double bodyKeep(world::Entity &e);

    double dt_ = 50;
    double clock_ = -1;  // diep ticks so far, by the clock
    long ticks_ = 0;     // diep ticks run
    int nextHash_ = 0;
    uint32_t serial_ = 0;  // barrels made
    int bosses_ = 0;
    double nextBoss_ = -1;  // ms
};

}  // namespace game
