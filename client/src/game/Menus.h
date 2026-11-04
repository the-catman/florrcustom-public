// Title screen menus: buttons along the bottom left, each opening a panel above it, the settings
// and galleries inside them, and what they remember between visits.
#pragma once
#include <cstdint>

#include "cp6/Context.h"
#include "game/Draw.h"

namespace game {

class Game;
struct Input;

class Menus {
public:
    explicit Menus(Game &game);

    void loadStorage();
    void discoverPetal(int type);
    void discoverMob(int type);
    int floaterPetal() const;  // weighted by rarity, from the petals seen so far

    bool onButton(int button, bool down);  // true when a menu handled it
    void setWheel(float notches) { wheel_ = notches; }
    void draw(cp6::Context &c, double now);
    void closeCursor();

    // a finger on the title screen: drags the open panel when it lands on one
    void onFingerDown(int id, float hudX, float hudY, float y);
    void onFingerMove(int id, float y);
    void onFingerUp(int id);

private:
    enum {
        MENU_SETTINGS,
        MENU_PETALS,
        MENU_MOBS,
        MENU_CHANGELOG,
        MENU_CLOSE,
        MENU_DISCORD,
        MENU_REDDIT,
        MENU_ITEM
    };  // MENU_ITEM + i: setting i
    enum { S_TOUCH, S_ZOOM, S_FIXED, S_KEYBOARD, S_HELPER };
    struct Box {
        float x, y, w, h;
    };
    // each gallery card has its own tooltip, fading in while hovered and out after
    struct GalleryTip {
        float k = 0, x = 0, top = 0;
        int type = 0;
        bool over = false;
    };

    int settingsList(int *out) const;  // the settings shown, at most 5
    bool &setting(int s);
    static const char *settingKey(int s);
    Box panelBox(int p, float Hu) const;
    int menuTarget(float x, float y) const;  // UI units; -1 over nothing
    void scrollPanel(const Box &b, float maxScroll, float dt, double now);
    void openLink(int target);
    void drawPanel(cp6::Context &c, double now, int panel, float x, float y, float w, float h);

    Game &game_;
    cp6::Context &ctx_;
    Input &in;

    int panel_ = -1;    // open panel, or -1
    int pressed_ = -1;  // menu target the left button went down on
    float scroll_ = 0;
    float scrollTo_ = 0;
    float contentH_ = 0;
    // A finger drags the open panel's content along, and on phones it keeps going when let go and
    // springs back from past the ends. The scrollbar can be dragged with the mouse. On phones the
    // bar only shows while scrolling and for half a second after.
    float wheel_ = 0;    // wheel notches this frame
    float scrollV_ = 0;  // units per ms
    int dragId_ = -1;    // the finger dragging the panel
    float dragY_ = 0;
    float dragLastY_ = 0;
    bool thumbDrag_ = false;
    float thumbY_ = 0;
    float thumb_[4] = {};  // where the scrollbar can be grabbed, when it shows
    double scrollAt_ = -1e9;
    float barK_ = 0;
    double menuLast_ = 0;
    int cursor_ = 0;
    float linkTipAlpha_[2] = {};  // Discord and Reddit button tooltips
    // panels slide up into place and fade in, and back down when closed; a closing panel keeps
    // the scroll it had
    float panelK_[4] = {};
    float panelScroll_[4] = {};
    GalleryTip galleryTips_[512];
    bool seenPetal_[P_COUNT] = {};
    bool seenMob_[M_COUNT] = {};
};

}  // namespace game
