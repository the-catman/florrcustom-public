// Touch controls: a joystick wherever a finger lands, attack (A) and defend (B) buttons bottom
// right, the leaderboard folded into a button top right.
#pragma once
#include <vector>

#include "cp6/Context.h"

namespace game {

class Game;

class TouchControls {
public:
    explicit TouchControls(Game &game);

    void onTouch(int phase, int id, float x, float y);  // canvas pixels
    void update(double now);
    void draw(cp6::Context &c, float Wu, float ia);

    bool attacking() const { return aDown_; }
    bool defending() const { return bDown_; }
    float joystickAngle() const { return joyAngle_; }
    float joystickStrength() const { return joyStrength_; }
    bool boardOpen() const { return boardOpen_; }
    float boardK() const { return boardK_; }
    float boardSquareY() const { return 20 - 80 * boardK_; }

private:
    enum Role { R_NONE, R_JOY, R_BUTTON, R_BOARD, R_SLOT, R_UI };
    struct Touch {
        int id;
        float x, y, x0, y0;
        Role role;
    };

    Game &game_;
    std::vector<Touch> touches_;
    int joyId_ = -1;
    float joyX_ = 0;
    float joyY_ = 0;
    float joyAlpha_ = 0;
    float joyStrength_ = 0;
    float joyAngle_ = 0;
    bool aDown_ = false;
    bool bDown_ = false;
    float touchAlpha_ = 0;  // the controls fade in while alive
    bool boardOpen_ = false;
    float boardK_ = 0;
    double touchLast_ = 0;
};

}  // namespace game
