// The world on screen: entities gliding toward what the server says, the camera, and drawing them.
#pragma once
#include <cstdint>

#include "cp6/Context.h"
#include "sim/Simulation.h"

namespace game {

class Game;

class GameRendering {
public:
    explicit GameRendering(Game &game);

    void smooth(sim::Entity &e, float dt, double now);
    void updateCamera(sim::CameraData *cam, sim::Entity *p, float dt, double now);
    void drawWorld(cp6::Context &c, double now, float dt);

    float cameraX() const { return camX_; }
    float cameraY() const { return camY_; }
    float worldScale() const;         // canvas pixels per world unit
    float zoneEdge(int level) const;  // -INFINITY when every zone is open
    void noteHit(double now) { hurtAt_ = now; }

private:
    void drawEntity(cp6::Context &c, sim::Entity &e, double now, float alpha, float grow);
    void drawMovementHelper(cp6::Context &c, float dt);

    Game &game_;
    sim::Simulation &sim_;
    cp6::Context &ctx_;

    float camX_ = 0;
    float camY_ = 0;
    float fov_ = 1;
    // the view follows the camera's position, plus the lean of your own flower and a shake when
    // it is hit; it jumps along while there is a flower to follow and drifts after it otherwise
    sim::Lerp camLx_, camLy_;
    uint32_t camId_ = 0;
    float leanX_ = 0;  // your flower leans up to 5 units toward where it is steering
    float leanY_ = 0;
    float shakeK_ = 0;
    double hurtAt_ = -1e9;

    float edgeShown_ = 0;
    bool edgePlaced_ = false;
    float bounds_[4] = {};  // map edges as drawn: left, right, top, bottom

    // the movement helper arrow
    float helperAlpha_ = 0;
    float helperX_ = 0;
    float helperY_ = 0;
    float helperFromX_ = 0;
    float helperFromY_ = 0;
};

}  // namespace game
