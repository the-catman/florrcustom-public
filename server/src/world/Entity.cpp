#include "world/Entity.h"

namespace world {

void Entity::add(int c) {
    if (has(c)) return;
    comps |= 1ull << c;
    switch (c) {
        case C_DROP: drop.emplace(&changed); break;
        case C_MOVES: moves.emplace(&changed); break;
        case C_PEA: pea.emplace(&changed); break;
        case C_CAMERA: camera = std::make_unique<CameraData>(&changed); break;
        case C_PETAL: petal.emplace(&changed); break;
        case C_ANT: ant.emplace(&changed); break;
        case C_MOB: mob.emplace(&changed); break;
        case C_OWNED: owned.emplace(&changed); break;
        case C_ARENA: arena = std::make_unique<ArenaData>(&changed); break;
        case C_NAME: name.emplace(&changed); break;
        case C_HEALTH: health.emplace(&changed); break;
        case C_POSITION: position.emplace(&changed); break;
        case C_SEGMENT: segment.emplace(&changed); break;
        case C_DEATH: death.emplace(&changed); break;
        case C_SIZE: size.emplace(&changed); break;
        case C_STATUS: status.emplace(&changed); break;
        case C_FLOWER: flower = std::make_unique<FlowerData>(&changed); break;
        case C_TANK: tank.emplace(&changed); break;
        case C_SHOT: shot.emplace(&changed); break;
        case C_DRIVER: driver = std::make_unique<DriverData>(&changed); break;
        default: break;  // nothing but the fact that it has it
    }
}

void Entity::clean() {
    changed = 0;
    if (camera) camera->kept.changed = 0;
    if (arena) {
        arena->leaderFlowers.changed = 0;
        arena->leaderNames.changed = 0;
        arena->leaderScores.changed = 0;
    }
    if (flower) {
        flower->slots.changed = 0;
        flower->cooldowns.changed = 0;
    }
    if (tank) tank->turrets.changed = 0;
    if (driver) driver->stats.changed = 0;
    if (age < 2) age++;
}

}  // namespace world
