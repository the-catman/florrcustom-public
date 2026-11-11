// The game as a whole: owns the connection, the simulation and what draws them, routes input, and
// runs one frame at a time.
#pragma once
#include <string>
#include <vector>

#include "cp6/Context.h"
#include "game/Draw.h"
#include "game/GameRendering.h"
#include "game/GameUI.h"
#include "net/Conn.h"
#include "sim/Simulation.h"

namespace game {

struct Input {
    float mx = 0, my = 0;  // canvas pixels
    bool left = false, right = false, space = false, shift = false;
    bool up = false, down = false, leftKey = false, rightKey = false;
    bool keyO = false, keyL = false, keyJ = false;  // held; sent to the server as input bits
    bool keyY = false, keyU = false, keyM = false;  // tanks: the class tree, and building stats
    bool clicked = false;                           // left button went down this frame
    float clickX = 0, clickY = 0;
};

struct Settings {
    bool touchControls = false;
    bool mobileZoom = true;
    bool fixedJoystick = false;
    bool keyboardControl = false;
    bool movementHelper = false;
};

enum class Screen { Title, Playing, Dead };

// what the flower had when last seen alive, for the death screen
struct LastLife {
    int level = 1;
    std::vector<int> slots;
    std::vector<int> ring;
    int petals[5] = {};  // the first five slots, empty ones too
    std::string killer;
};

class Game {
public:
    Game();

    bool init();
    void frame(double now);
    void onKey(const char *key, const char *code, bool down);
    void onEnter();
    bool onButton(int button, bool down);  // true when the HUD handled it
    // phones and tablets: fingers instead of the mouse; phase 0 start, 1 move, 2 end
    // phones always use them; elsewhere they are a setting, offered once a touch has been seen
    bool touchMode() const { return mobile || (touchSeen && settings.touchControls); }
    // the server runs tanks: the camera comes with a driver
    bool tankServer() const;
    // you play a tank: on a tank server, unless your player, or the last one you had, is a flower
    bool tankMode() const { return tankServer() && tankPlayer_; }
    void noteTouch() { touchSeen = true; }
    void onTouch(int phase, int id, float x, float y);  // canvas pixels

    Input in;
    cp6::Context ctx;
    net::Conn connection;
    sim::Simulation simulation;
    GameRendering rendering;
    GameUI ui;

    Settings settings;
    bool mobile = false;
    bool tablet = false;
    bool touchSeen = false;
    std::string name;
    Screen screen = Screen::Title;
    double time = 0;  // this frame's time, ms
    double spawnedAt = 0;
    double deadAt = 0;
    uint32_t player = 0;  // your flower while it lives, and for as long as it lingers after
    LastLife lastLife;
    bool forceInput = false;  // send the input this frame even if it hasn't changed
    bool autoFire = false;    // E
    bool autoSpin = false;    // C
    float tankAngle = 0;      // where your tank is aiming
    bool playTank = true;     // what the title screen spawns on a tank server
    bool devKeys = false;     // the server allows test keys
    void choosePlay(bool tank);

private:
    void handleEvents(double now);
    void trackPlayer(double now);
    void rememberLife(const sim::FlowerData &flower);
    void sendSpawn();
    void sendInput(double now);
    void sendTankInput(double now);
    void onTankKey(const char *code);
    bool onFlowerDevKey(const char *code);
    void sendDev(int what, int type = -1);

    bool wasAlive_ = false;
    bool tankPlayer_ = true;
    double spawnSentAt_ = 0;
    double lastInput_ = 0;
    float lastAngle_ = 0;
    float lastDist_ = 0;
    uint32_t lastFlags_ = 0;
    double spinPhase_ = 0;
    bool keyY_ = false;  // pressed this frame
    bool keyI_ = false;
};

}  // namespace game
