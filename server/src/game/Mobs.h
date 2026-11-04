// Mob spawning: the zone spawner, mob creation and each mob type's stats and AI settings, ant
// holes letting more ants out as they get hurt, and centipedes' segments following their heads.
#pragma once
#include <cstdint>
#include <vector>

#include "game/Components.h"
#include "game/System.h"

namespace game {

enum AntKind : int { ANT_BABY, ANT_WORKER, ANT_SOLDIER, ANT_QUEEN };

class Mobs : public System {
public:
    struct Hole {
        uint32_t id;
        int hatched;
    };

    explicit Mobs(Game &game);

    world::Entity *make(uint8_t type, double x, double y, int zone);
    void shootMissile(world::Entity &hornet);
    void detachSegments(world::Entity &e);
    void hatchFromHoles();
    void spawn();
    void moveSegments();

    int zoneCount[8] = {};
    std::vector<Hole> holes;  // ant holes, and how many of their ants are out

private:
    bool spawnInZone(int zone);
    bool spotTaken(float x, float y);
    bool nearFlower(double x, double y);
    void init(world::Entity &e, uint8_t type);
    void initRock(world::Entity &e, bool boulder);
    void initCactus(world::Entity &e);
    void initLadybug(world::Entity &e, int type);
    void initBee(world::Entity &e);
    void initAnt(world::Entity &e, int kind);
    void initAntHole(world::Entity &e);
    void initBeetle(world::Entity &e, bool massive);
    void initHornet(world::Entity &e);
    void initCentipede(world::Entity &e, bool evil, bool head);
    void initSquare(world::Entity &e);
    void initSpider(world::Entity &e);
    void hatchAnt(world::Entity &hole, int kind);
    void follow(world::Entity &seg);
};

}  // namespace game
