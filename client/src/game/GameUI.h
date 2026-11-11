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
    int playKindAt(float x, float y) const;  // HUD units: 0 the tank button, 1 the flower one, -1 neither
    void askName();
    void toggleStats() { stats_ = !stats_; }
    void showDeathHud(bool on) { deadHud_ = on; }

    // tanks
    void notify(const std::string &text, uint32_t color, float ms, const std::string &id = "");
    void spendStat(int stat);                  // a number key: a point now, or into the build while U or M is held
    bool overTankHud(float x, float y) const;  // a point is on one of its buttons
    // every petal, to pick one with the test keys
    void togglePetalPicker() { picker_ = !picker_; }
    bool petalPickerOpen() const { return picker_; }
    int pickerAt(float x, float y) const;  // HUD units: a petal, 0 elsewhere on the panel, -1 off it
    bool onTankButton(bool down);

    Inventory inventory;
    Menus menus;
    TouchControls touch;

private:
    void drawTitle(cp6::Context &c, double now);
    void drawPicker(cp6::Context &c, double now);
    void playKindBox(int kind, float &x, float &y, float &w, float &h) const;
    void drawPlayKind(cp6::Context &c, double now);
    void drawHud(cp6::Context &c, double now);
    void drawLeaderboard(cp6::Context &c, float Wu);
    void drawDeath(cp6::Context &c, double now);
    void drawZoneWarning(cp6::Context &c, double now);
    void drawWin(cp6::Context &c, float dt, sim::Entity *me);
    void drawRoyale(cp6::Context &c, float dt);
    void drawStats(cp6::Context &c, float dt);
    void drawRotateHint(cp6::Context &c, double now);
    float boxAway() const { return std::max(connK_, leaveK_); }  // the name box is gone at 1

    // tanks, in TankHud.cpp
    struct Hit {
        float x, y, w, h;
        int action, arg;
    };
    struct Note {
        std::string text, id;
        uint32_t color;
        double created, expiry;  // ms; expiry 0 stays
    };
    struct Slot {
        int tank = -1;
        float fade = 0;
        int col = -1, row = -1;
        uint32_t color = 0;
    };
    struct TreeNode {
        int tank = 0, level = 0, sibling = 0;
        float start = 0, end = 0;
        std::vector<int> children;
    };
    struct TreeSector {
        int node;  // -1 for a connector
        float start, end;
        int inner, outer;
        int index;
    };
    struct TreeRing {
        int inner = 0;
        std::vector<TreeSector> sectors;
    };
    float tankUnit() const;
    const sim::DriverData *driver() const;
    bool hasTank() const;
    void applyStat(int stat, int target);
    void queueStat(int stat, bool toMax);
    const Hit *hitAt(float x, float y) const;
    bool pressed(int action, int arg) const;
    void drawTankHud(cp6::Context &c, double now, float dt);
    void drawTankBars(cp6::Context &c, float dt);
    void drawTankStats(cp6::Context &c, float dt);
    void drawNotes(cp6::Context &c, double now);
    void drawUpgrades(cp6::Context &c, double now, float dt);
    int treeNode(int tank, int sibling, int depth);
    void drawTree(cp6::Context &c, double now);
    void drawTankDeath(cp6::Context &c, float dt);

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
    bool picker_ = false;

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

    std::vector<Hit> hits_;  // the tank HUD's buttons this frame, in pixels; the last on top
    bool pressing_ = false;
    int pressAction_ = 0, pressArg_ = 0;
    std::vector<Note> notes_;
    double tankLast_ = 0;
    float levelCreep_ = 0, levelPerc_ = 0, scorePerc_ = 0, leaderShown_ = 0;
    bool levelFill_ = false, levelDrain_ = false;
    int levelSeen_ = -1;
    std::string levelText_;
    float statsOpen_ = 0;
    bool statsLatch_ = false, hadTank_ = false, uWas_ = false, queuedSinceU_ = false;
    std::string statBuild_;  // U and M: stats to spend into, as '8' - stat
    std::vector<Slot> slots_;
    bool ignored_ = false;
    int ignoreUntil_ = 0;
    float slotsFade_ = 0, ignoreW_ = 0, ignoreB_ = 0;
    float treeFade_ = 0;
    std::vector<TreeNode> tree_;
    std::vector<TreeRing> rings_;
    float dimK_ = 0, deathK_ = 0;
};

}  // namespace game
