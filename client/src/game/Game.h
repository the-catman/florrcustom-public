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

private:
    void handleEvents(double now);
    void trackPlayer(double now);
    void rememberLife(const sim::FlowerData &flower);
    void sendInput(double now);

    bool wasAlive_ = false;
    double lastInput_ = 0;
    float lastAngle_ = 0;
    float lastDist_ = 0;
    uint32_t lastFlags_ = 0;
    bool keyY_ = false;  // pressed this frame
    bool keyI_ = false;
};

}  // namespace game
