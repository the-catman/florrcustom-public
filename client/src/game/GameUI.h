// Everything drawn over the world: the title screen, the HUD, the leaderboard and the death screen.
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "cp6/Context.h"
#include "game/Inventory.h"
#include "game/Menus.h"
#include "game/TouchControls.h"
#include "net/Conn.h"
#include "sim/Simulation.h"

namespace game {

class Game;

class GameUI {
public:
    explicit GameUI(Game &game);

    void render(cp6::Context &c, double now, float dt, float frameMs);

    float hudScale() const;
    float hudW() const;  // screen size in HUD units, rounded up
    float hudH() const;
    bool nameBoxAt(float x, float y) const;  // HUD units
    bool playButtonAt(float x, float y) const;
    void askName();
    void toggleStats() { stats_ = !stats_; }
    void showDeathHud(bool on) { deadHud_ = on; }

    Inventory inventory;
    Menus menus;
    TouchControls touch;

private:
    void drawTitle(cp6::Context &c, double now);
    void drawHud(cp6::Context &c, double now);
    void drawLeaderboard(cp6::Context &c, float Wu);
    void drawDeath(cp6::Context &c, double now);
    void drawZoneWarning(cp6::Context &c, double now);
    void drawWin(cp6::Context &c, float dt, sim::Entity *me);
    void drawRoyale(cp6::Context &c, float dt);
    void drawStats(cp6::Context &c, float dt);
    void drawRotateHint(cp6::Context &c, double now);
    float boxAway() const { return std::max(connK_, leaveK_); }  // the name box is gone at 1

    Game &game_;
    sim::Simulation &sim_;
    cp6::Context &ctx_;
    net::Conn &conn_;
    Input &in;

    float lbShown_[10] = {};  // leaderboard scores as shown
    double lbLast_ = 0;
    float levelShown_ = 0;  // level bar progress as shown
    double levelLast_ = 0;
    float zoneAlpha_ = 0;
    float zoneTimer_ = 0;
    double zoneLast_ = 0;
    // the HUD slides off screen, up to 400 units, while your flower is dead
    float hudSlide_ = 0;

    // the title screen
    struct Floater {  // petals drifting in from the left, in HUD units
        float x, y, scale, phase;
        int type;
        uint32_t id;
    };
    std::vector<Floater> floaters_;
    float floaterTimer_ = 0;
    double titleLast_ = 0;
    float connK_ = 0;         // 1 while waiting for the server
    float leaveK_ = 0;        // 1 once spawning: the name box slides away
    double inGameAt_ = -1e9;  // last frame away from the title
    // what the title shows under the box, settled while on the title: the respawn level and kept
    // cards from the last life, or How to play when there are none
    int titleLevel_ = 0;
    int titleKept_ = 0;
    std::vector<int> titleKeptTypes_;
    float iris_ = 0;        // 0 on the title, 1 in game; the title is drawn outside a circle of this size
    bool deadHud_ = false;  // the death screen stays inside the circle while it closes

    // battle royale: "YOU WON" over the screen, and a box while waiting for the round to start
    float winK_ = 0;
    float royaleK_ = 0;
    double royaleLast_ = 0;
    sim::Lerp royaleTime_;
    float deathTipK_[16] = {};  // death screen card tooltips
    double deathLast_ = 0;

    // the ; key shows the canvas size, frame times and frame rate in the bottom right
    bool stats_ = false;
    std::vector<float> frameMs_;
    float fpsShown_ = 0;
    int statsFrame_ = 0;
    std::string statsText_;  // the text is redone every 10 frames
};

}  // namespace game
