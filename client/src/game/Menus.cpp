#include "game/Menus.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "game/Data.h"
#include "game/Ease.h"
#include "game/Game.h"
#include "game/Ui.h"

extern "C" {
int ui_storage_get(const char *key, char *buf, int cap);
void ui_storage_set(const char *key, const char *val);
void ui_cursor(int kind);
void ui_open(const char *url);
}

namespace game {
using cp6::Align;
using cp6::Color;
using cp6::Context;

namespace {

constexpr float PI = 3.14159265f;
constexpr uint32_t BLUE = 0x5a9fdb, BLUE_DARK = 0x4981b1;

struct Button {
    float x0, x1, textX;
    const char *label;
};
const Button kButtons[4] = {
    {10, 110, 60, "Settings"}, {120, 220, 160, "Petals"}, {230, 330, 280, "Mobs"}, {340, 460, 400, "Changelog"}};
constexpr float BUTTON_TOP = 45, BUTTON_H = 35;  // top edge measured up from the bottom

// community links in the top right corner, 40 units square, centred this far in from the right edge
struct Link {
    float fromRight;
    uint32_t fill, stroke;
    const char *tip, *url;
};
const Link kLinks[2] = {{40, 0x7289da, 0x5c6fb1, "Join our Discord community!", "https://discord.gg/TRMtDSE"},
                        {90, 0xff4500, 0xcf3800, "Join our Reddit community!", "https://reddit.com/r/florrio/"}};

// the icons, as path data: the Discord logo in a 240 unit box (eyes, then the face cut out of the
// bubble) and the Reddit alien in a 24 unit box
const char *const kDiscordEyes =
    "M104.4,103.9 C98.7,103.9,94.2,108.9,94.2,115 C94.2,121.1,98.8,126.1,104.4,126.1 "
    "C110.1,126.1,114.6,121.1,114.6,115 "
    "C114.7,108.9,110.1,103.9,104.4,103.9 Z M140.9,103.9 C135.2,103.9,130.7,108.9,130.7,115 "
    "C130.7,121.1,135.3,126.1,140.9,126.1 "
    "C146.6,126.1,151.1,121.1,151.1,115 C151.1,108.9,146.6,103.9,140.9,103.9 Z";
const char *const kDiscordBubble =
    "M189.5,20 L55.5,20 C44.2,20,35,29.2,35,40.6 L35,175.8 C35,187.2,44.2,196.4,55.5,196.4 L168.9,196.4 L163.6,177.9 "
    "L176.4,189.8 L188.5,201 L210,220 L210,40.6 C210,29.2,200.8,20,189.5,20 Z M150.9,150.6 "
    "C150.9,150.6,147.3,146.3,144.3,142.5 "
    "C157.4,138.8,162.4,130.6,162.4,130.6 C158.3,133.3,154.4,135.2,150.9,136.5 C145.9,138.6,141.1,140,136.4,140.8 "
    "C126.8,142.6,118,142.1,110.5,140.7 C104.8,139.6,99.9,138,95.8,136.4 C93.5,135.5,91,134.4,88.5,133 "
    "C88.2,132.8,87.9,132.7,87.6,132.5 "
    "C87.4,132.4,87.3,132.3,87.2,132.2 C85.4,131.2,84.4,130.5,84.4,130.5 C84.4,130.5,89.2,138.5,101.9,142.3 "
    "C98.9,146.1,95.2,150.6,95.2,150.6 C73.1,149.9,64.7,135.4,64.7,135.4 C64.7,103.2,79.1,77.1,79.1,77.1 "
    "C93.5,66.3,107.2,66.6,107.2,66.6 L108.2,67.8 C90.2,73,81.9,80.9,81.9,80.9 C81.9,80.9,84.1,79.7,87.8,78 "
    "C98.5,73.3,107,72,110.5,71.7 C111.1,71.6,111.6,71.5,112.2,71.5 C118.3,70.7,125.2,70.5,132.4,71.3 "
    "C141.9,72.4,152.1,75.2,162.5,80.9 C162.5,80.9,154.6,73.4,137.6,68.2 L139,66.6 C139,66.6,152.7,66.3,167.1,77.1 "
    "C167.1,77.1,181.5,103.2,181.5,135.4 C181.5,135.4,173,149.9,150.9,150.6 Z";
const char *const kReddit =
    "M24,11.78 C24,10.32,22.81,9.13,21.34,9.13 C20.63,9.13,19.98,9.42,19.5,9.88 C17.69,8.69,15.24,7.93,12.53,7.83 "
    "L14.02,3.17 L18.03,4.11 L18.02,4.16 C18.02,5.36,19,6.33,20.2,6.33 C21.4,6.33,22.37,5.36,22.37,4.16 "
    "C22.37,2.97,21.4,2,20.2,2 C19.28,2,18.49,2.57,18.18,3.38 L13.85,2.36 C13.66,2.32,13.47,2.43,13.41,2.61 "
    "L11.75,7.82 C8.92,7.85,6.35,8.62,4.45,9.85 C3.98,9.41,3.35,9.13,2.66,9.13 C1.19,9.13,0,10.32,0,11.78 "
    "C0,12.75,0.53,13.59,1.32,14.05 C1.26,14.33,1.23,14.62,1.23,14.91 C1.23,18.82,6.04,22,11.95,22 "
    "C17.86,22,22.67,18.82,22.67,14.91 C22.67,14.63,22.64,14.36,22.59,14.1 C23.43,13.65,24,12.79,24,11.78 Z "
    "M6.78,13.6 C6.78,12.73,7.49,12.02,8.36,12.02 C9.23,12.02,9.94,12.73,9.94,13.6 C9.94,14.46,9.23,15.17,8.36,15.17 "
    "C7.49,15.17,6.78,14.46,6.78,13.6 Z M15.84,18.26 C15.04,19.06,13.79,19.44,12.01,19.44 L12,19.44 L11.99,19.44 "
    "C10.21,19.44,8.96,19.06,8.16,18.26 C8.02,18.12,8.02,17.88,8.16,17.74 C8.31,17.6,8.54,17.6,8.69,17.74 "
    "C9.34,18.39,10.42,18.7,11.99,18.7 L12,18.7 L12.01,18.7 C13.58,18.7,14.66,18.39,15.31,17.74 "
    "C15.46,17.59,15.69,17.6,15.84,17.74 C15.98,17.88,15.98,18.12,15.84,18.26 Z M15.65,15.17 "
    "C14.78,15.17,14.07,14.46,14.07,13.59 C14.07,12.73,14.78,12.02,15.65,12.02 C16.52,12.02,17.23,12.73,17.23,13.59 "
    "C17.23,14.46,16.52,15.17,15.65,15.17 Z";

// path data with absolute M, L, C and Z commands only
cp6::Path2D parsePath(const char *d) {
    cp6::Path2D p;
    char cmd = 0;
    float v[6];
    while (*d) {
        while (*d == ' ' || *d == ',') d++;
        if (!*d) break;
        if (std::strchr("MLCZ", *d)) {
            cmd = *d++;
            if (cmd == 'Z') {
                p.closePath();
                continue;
            }
        }
        int n = cmd == 'C' ? 6 : 2;
        for (int i = 0; i < n; i++) {
            while (*d == ' ' || *d == ',') d++;
            char *end;
            v[i] = std::strtof(d, &end);
            d = end;
        }
        if (cmd == 'M')
            p.moveTo(v[0], v[1]);
        else if (cmd == 'L')
            p.lineTo(v[0], v[1]);
        else
            p.bezierCurveTo(v[0], v[1], v[2], v[3], v[4], v[5]);
    }
    return p;
}

struct PanelDef {
    const char *title;
    float w, h, headW;
};
const PanelDef kPanels[4] = {{"Settings", 239.6f, 163.6f, 220},
                             {"Petal Gallery", 336, 370, 336},
                             {"Mob Gallery", 351, 370, 351},
                             {"Changelog", 500, 400, 500}};

// scrolling content inside a panel
struct Clip {
    float x, y, w, h;
};
Clip contentRect(float x, float y, float w, float h) {
    return {x + 10, y + 63.6f, w - 20, h - 73.6f};
}

const char *const kSettingLabel[5] = {"Touch controls", "Mobile zoom", "Fixed joystick", "Keyboard movement",
                                      "Movement helper"};
const char *const kSettingKey[5] = {"florrio_touch_controls", "florrio_mobile_zoom", "florrio_fixed_joystick",
                                    "florrio_keyboard_control", "florrio_movement_helper"};

uint32_t mix(uint32_t a, uint32_t b, float k) {
    uint32_t out = 0;
    for (int s = 0; s <= 16; s += 8) {
        float ca = float(a >> s & 255), cb = float(b >> s & 255);
        out |= uint32_t(ca + (cb - ca) * k) << s;
    }
    return out;
}

// gallery entries are saved as a short code derived from the id rather than a plain flag
std::string galleryCode(int id, bool mob) {
    static const char kChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()[]{}";
    uint64_t s = uint32_t(id ^ (mob ? 29203 : 10280)) % 2147483647u;
    if (!s) s = 1;
    std::string t;
    for (int i = 0; i < 8; i++) {
        s = s * 48271 % 2147483647;
        t += kChars[s % 75];
    }
    return t;
}

std::string storageGet(const std::string &key) {
    char buf[256];
    int n = ui_storage_get(key.c_str(), buf, sizeof buf);
    return n < 0 ? std::string() : std::string(buf, size_t(n));
}

// text placed by its box: (x, y) is the top left of a box 1.4 * size tall
void textBox(Context &c, const std::string &s, float x, float y, float size, uint32_t col = 0xffffff) {
    outlinedText(c, s, x + size * .2f, y + size * .7f, size, Color(col), Align::Left);
}

std::vector<std::string> wrapText(Context &c, const std::string &text, float size, float maxW) {
    c.font(size);
    std::vector<std::string> out;
    std::string line;
    size_t i = 0;
    while (i <= text.size()) {
        size_t j = text.find(' ', i);
        if (j == std::string::npos) j = text.size();
        std::string word = text.substr(i, j - i);
        std::string next = line.empty() ? word : line + " " + word;
        if (!line.empty() && c.measureText(next) > maxW) {
            out.push_back(line);
            line = word;
        } else
            line = next;
        i = j + 1;
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

// drop chances: whole percents, or one significant digit below 1% ("44%", ".5%", ".06%")
std::string percent(float chance) {
    float p = chance * 100;
    char b[24];
    if (p > 0 && p < 1) {
        int d = std::max(1, int(std::ceil(-std::log10(p) - 1e-4f)));
        std::snprintf(b, sizeof b, "%.*f%%", d, p);
        if (b[0] == '0' && b[1] == '.') return b + 1;
    }
    std::snprintf(b, sizeof b, "%.0f%%", p);
    return b;
}

// galleries list by rarity, then name
const std::vector<int> &petalOrder() {
    static std::vector<int> v;
    if (v.empty()) {
        for (int t = 1; t < P_COUNT; t++) v.push_back(t);
        std::stable_sort(v.begin(), v.end(), [](int a, int b) {
            int ra = petalRarity(a), rb = petalRarity(b);
            return ra != rb ? ra < rb : std::strcmp(petalName(a), petalName(b)) < 0;
        });
    }
    return v;
}

const std::vector<int> &mobOrder() {
    static std::vector<int> v;
    if (v.empty()) {
        for (int t = 1; t < M_COUNT; t++)
            if (t != M_CENTIPEDE_BODY && t != M_EVIL_CENTIPEDE_BODY) v.push_back(t);
        std::stable_sort(v.begin(), v.end(), [](int a, int b) {
            int ra = mobRarity(a), rb = mobRarity(b);
            return ra != rb ? ra < rb : std::strcmp(mobName(a), mobName(b)) < 0;
        });
    }
    return v;
}

// how a mob sits in its gallery picture: overall scale, the radius it is drawn at before that, and
// how far it is moved back along its length
struct Icon {
    float scale, r, back = 0;
};
Icon mobIcon(int t) {
    switch (t) {
        case M_ROCK: return {.6f, 30};
        case M_BOULDER: return {.8f, 30};
        case M_CACTUS: return {.4f, 50};
        case M_LADYBUG:
        case M_DARK_LADYBUG:
        case M_SHINY_LADYBUG: return {.5f, 30};
        case M_MASSIVE_LADYBUG: return {.7f, 30};
        case M_BEE: return {.5f, 25};
        case M_HORNET: return {.5f, 25, 10};
        case M_QUEEN_ANT: return {.5f, 25};
        case M_ANT_HOLE: return {.5f, 45};
        case M_BEETLE: return {.4f, 40, 15};
        case M_MASSIVE_BEETLE: return {.5f, 40, 15};
        case M_CENTIPEDE:
        case M_EVIL_CENTIPEDE: return {.4f, 35};
        case M_SPIDER: return {.5f, 15};
        case M_SQUARE: return {.5f, 35};
        default: return {.5f, 14};  // ants
    }
}

// an unknown entry: a dark square with a faded question mark
void unknownCard(Context &c, float cx, float cy, float size) {
    float a = c.globalAlpha(), ts = size * 32 / 60.f;
    c.globalAlpha(a * .25f);
    c.fillStyle(Color(0x000000));
    roundRect(c, cx - size * .5f, cy - size * .5f, size, size, size / 24.f);
    c.fill();
    c.font(ts);
    c.textAlign(Align::Center);
    c.textBaseline(cp6::Baseline::Middle);
    c.globalAlpha(a * .75f);
    c.fillStyle(Color(0xffffff));
    c.fillText("?", cx, cy);
    c.globalAlpha(a);
}

// layout of one mob gallery row; lines of the description wrap at 222 units
constexpr float ROW_W = 315, DESC_W = 222;
float mobTextBottom(int lines) {
    return 10 + 25.2f + 19.6f + 5 + 19.6f * float(lines);
}

}  // namespace

Menus::Menus(Game &game) : game_(game), ctx_(game.ctx), in(game.in) {}

void Menus::closeCursor() {
    pressed_ = -1;
    if (cursor_) {
        ui_cursor(0);
        cursor_ = 0;
    }
}

void Menus::onFingerDown(int id, float hudX, float hudY, float y) {
    if (panel_ < 0) return;
    Box b = panelBox(panel_, game_.ui.hudH());
    if (hudX >= b.x && hudX <= b.x + b.w && hudY >= b.y && hudY <= b.y + b.h) {
        dragId_ = id;
        dragY_ = dragLastY_ = y;
    }
}

void Menus::onFingerMove(int id, float y) {
    if (id == dragId_) dragY_ = y;
}

void Menus::onFingerUp(int id) {
    if (id == dragId_) dragId_ = -1;
}

// ---------------------------------------------------------------- remembered state

void Menus::loadStorage() {
    game_.settings.keyboardControl = storageGet("florrio_keyboard_control") == "Y";
    game_.settings.movementHelper = storageGet("florrio_movement_helper") == "Y";
    std::string zoom = storageGet("florrio_mobile_zoom"), touch = storageGet("florrio_touch_controls");
    game_.settings.mobileZoom = zoom.empty() || zoom == "Y";
    game_.settings.touchControls = touch.empty() ? game_.mobile : touch == "Y";
    game_.settings.fixedJoystick = storageGet("florrio_fixed_joystick") == "Y";
    game_.name = storageGet("florrio_nickname");
    game_.playTank = storageGet("play_as") != "flower";
    for (int t = 1; t < P_COUNT; t++)
        seenPetal_[t] = storageGet("gallery_" + std::to_string(t)) == galleryCode(t, false);
    for (int t = 1; t < M_COUNT; t++) seenMob_[t] = storageGet("mob_" + std::to_string(t)) == galleryCode(t, true);
}

void Menus::discoverPetal(int t) {
    if (t <= 0 || t >= P_COUNT || seenPetal_[t]) return;
    seenPetal_[t] = true;
    ui_storage_set(("gallery_" + std::to_string(t)).c_str(), galleryCode(t, false).c_str());
}

void Menus::discoverMob(int t) {
    if (t == M_CENTIPEDE_BODY) t = M_CENTIPEDE;
    if (t == M_EVIL_CENTIPEDE_BODY) t = M_EVIL_CENTIPEDE;
    if (t <= 0 || t >= M_COUNT || seenMob_[t]) return;
    seenMob_[t] = true;
    ui_storage_set(("mob_" + std::to_string(t)).c_str(), galleryCode(t, true).c_str());
}

int Menus::floaterPetal() const {
    static const float kWeight[6] = {100, 20, 5, 1, .1f, .02f};
    float total = 0;
    for (int t = 1; t < P_COUNT; t++)
        if ((t == P_BASIC || seenPetal_[t]) && t != P_ANTENNAE) total += kWeight[petalRarity(t)];
    float r = float(std::rand()) / (float(RAND_MAX) + 1) * total;
    for (int t = 1; t < P_COUNT; t++) {
        if ((t != P_BASIC && !seenPetal_[t]) || t == P_ANTENNAE) continue;  // Antennae has nothing to show on its own
        if ((r -= kWeight[petalRarity(t)]) < 0) return t;
    }
    return 1;
}

// ---------------------------------------------------------------- input

bool Menus::onButton(int button, bool down) {
    if (button != 0) return false;
    float u = game_.ui.hudScale();
    int t = menuTarget(in.mx / u, in.my / u);
    if (!down) thumbDrag_ = false;
    if (down && t < 0 && panel_ >= 0 && !game_.mobile && thumb_[2] > thumb_[0]) {
        float x = in.mx / u, y = in.my / u;
        if (x >= thumb_[0] && x <= thumb_[2] && y >= thumb_[1] && y <= thumb_[3]) {
            thumbDrag_ = true;
            thumbY_ = y;
            return true;
        }
    }
    if (down) {
        pressed_ = t;
        if (t < 0 && panel_ >= 0) {  // pressing anywhere else outside the open panel closes it
            Box b = panelBox(panel_, game_.ui.hudH());
            float x = in.mx / u, y = in.my / u;
            if (x < b.x || x > b.x + b.w || y < b.y || y > b.y + b.h) panel_ = -1;
        }
        if (t >= 0 && t <= MENU_CHANGELOG) {  // the panel buttons work as soon as they are pressed
            panel_ = panel_ == t ? -1 : t;
            scroll_ = scrollTo_ = scrollV_ = 0;
            dragId_ = -1;
            thumbDrag_ = false;
            barK_ = 0;
            scrollAt_ = -1e9;
            pressed_ = -1;
        }
        return t >= 0;
    }
    int was = pressed_;
    pressed_ = -1;
    if (t < 0 || t != was) return false;
    if (t == MENU_CLOSE) {
        panel_ = -1;
    } else if (t == MENU_DISCORD || t == MENU_REDDIT) {
        openLink(t);
    } else if (t >= MENU_ITEM) {
        int list[5], n = settingsList(list);
        if (t - MENU_ITEM < n) {
            bool &v = setting(list[t - MENU_ITEM]);
            v = !v;
            ui_storage_set(settingKey(list[t - MENU_ITEM]), v ? "Y" : "N");
        }
    }
    return true;
}

// touch settings with touch controls, the mouse and keyboard ones without; on a computer, touch
// controls are offered once a finger has touched the screen
int Menus::settingsList(int *out) const {
    int n = 0;
    if (!game_.mobile && game_.touchSeen) out[n++] = S_TOUCH;
    if (game_.mobile) out[n++] = S_ZOOM;
    if (game_.touchMode())
        out[n++] = S_FIXED;
    else {
        out[n++] = S_KEYBOARD;
        out[n++] = S_HELPER;
    }
    return n;
}

const char *Menus::settingKey(int s) {
    return kSettingKey[s];
}

bool &Menus::setting(int s) {
    bool *v[5] = {&game_.settings.touchControls, &game_.settings.mobileZoom, &game_.settings.fixedJoystick,
                  &game_.settings.keyboardControl, &game_.settings.movementHelper};
    return *v[s];
}

// a panel sits above the buttons, centred on its own where there is room; the settings panel fits
// its rows, and is no narrower than its title bar
Menus::Box Menus::panelBox(int p, float Hu) const {
    float w = kPanels[p].w, h = kPanels[p].h;
    if (p == MENU_SETTINGS) {
        int list[5], n = settingsList(list);
        Context &c = ctx_;
        c.font(16);
        float widest = 0;
        for (int i = 0; i < n; i++) widest = std::max(widest, c.measureText(kSettingLabel[list[i]]));
        w = std::max(219.6f, 239.6f + widest - c.measureText(kSettingLabel[S_KEYBOARD]));
        h = 73.6f + 45.f * float(n);
    }
    return {std::max(10.f, (kButtons[p].x0 + kButtons[p].x1) * .5f - w * .5f), Hu - 60 - h, w, h};
}

int Menus::menuTarget(float x, float y) const {
    float Hu = game_.ui.hudH(), la = game_.ui.hudW();
    for (int i = 0; i < 2; i++)
        if (std::fabs(x - (la - kLinks[i].fromRight)) <= 20 && std::fabs(y - 40) <= 20) return MENU_DISCORD + i;
    for (int i = 0; i < 4; i++)
        if (x >= kButtons[i].x0 && x <= kButtons[i].x1 && y >= Hu - BUTTON_TOP && y <= Hu - BUTTON_TOP + BUTTON_H)
            return i;
    if (panel_ < 0) return -1;
    Box b = panelBox(panel_, Hu);
    float cx = b.x + kPanels[panel_].headW - 35, cy = b.y + 14.3f;
    if (x >= cx && x <= cx + 25 && y >= cy && y <= cy + 25) return MENU_CLOSE;
    if (panel_ == MENU_SETTINGS) {
        Clip cl = contentRect(b.x, b.y, b.w, b.h);
        int list[5], n = settingsList(list);
        for (int i = 0; i < n; i++) {
            float by = cl.y + 18 + 45 * i;
            if (x >= cl.x + 5 && x <= cl.x + cl.w && std::fabs(y - by) <= 15) return MENU_ITEM + i;
        }
    }
    return -1;
}

// ---------------------------------------------------------------- drawing

// The open panel's scroll: the wheel, a finger dragging it and, on phones, the fling after and the
// spring back from past the ends. What shows eases toward where it is going, faster while held.
void Menus::scrollPanel(const Box &b, float maxScroll, float dt, double now) {
    float u = game_.ui.hudScale(), x = in.mx / u, y = in.my / u;
    bool held = dragId_ >= 0 || thumbDrag_;
    if (dragId_ >= 0) {
        float d = (dragLastY_ - dragY_) / u;
        dragLastY_ = dragY_;
        scrollTo_ += d;
        if (game_.mobile && dt > 0) scrollV_ += (d / dt - scrollV_) * ease(.8f, dt);
        scrollAt_ = now;
    } else if (game_.mobile) {
        scrollTo_ += scrollV_ * dt;
        scrollV_ -= scrollV_ * ease(.9f, dt);
    }
    if (thumbDrag_) {
        float track = contentRect(b.x, b.y, b.w, b.h).h - 8;
        if (track > 0 && contentH_ > 0) scrollTo_ += (y - thumbY_) * contentH_ / track;
        thumbY_ = y;
        scrollAt_ = now;
    }
    if (!held && wheel_ != 0 && x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) {
        scrollTo_ += wheel_ * 75;
        scrollAt_ = now;
    }
    float to = scrollTo_;
    if (game_.mobile) {
        if (dragId_ < 0) {
            if (scrollTo_ > maxScroll) scrollTo_ += (maxScroll - scrollTo_) * ease(.8f, dt);
            if (scrollTo_ < 0) scrollTo_ -= scrollTo_ * ease(.8f, dt);
        }
        to = scrollTo_;
        if (to > maxScroll) to = maxScroll + std::pow(to - maxScroll, .8f);
        if (to < 0) to = -std::pow(-to, .8f);
    } else {
        scrollTo_ = to = std::clamp(scrollTo_, 0.f, maxScroll);
        scroll_ = std::clamp(scroll_, 0.f, maxScroll);
    }
    scroll_ += (to - scroll_) * ease(held ? .5f : .8f, dt);
    panelScroll_[panel_] = scroll_;
}

void Menus::draw(Context &c, double now) {
    float u = game_.ui.hudScale(), Wu = float(c.width()) / u, Hu = game_.ui.hudH();
    float mx = in.mx / u, my = in.my / u;
    float dt = frameTime(menuLast_, now, 100);
    int hover = menuTarget(mx, my);
    int cur = hover >= 0 ? 1 : 0;
    if (cur != cursor_) {
        ui_cursor(cur);
        cursor_ = cur;
    }

    c.setTransform(u, 0, 0, u, 0, 0);
    c.globalAlpha(1);
    for (auto &t : galleryTips_) t.over = false;
    float k7 = ease(.7f, dt);
    for (int p = 0; p < 4; p++) {
        float &k = panelK_[p];
        k += ((panel_ == p ? 1.f : 0.f) - k) * k7;
        if (k < .001f) continue;
        Box b = panelBox(p, Hu);
        float px = b.x, py = Hu + 50 + k * (b.y - Hu - 50);
        const PanelDef &d = kPanels[p];
        bool open = p == panel_;
        c.globalAlpha(std::min(1.f, k));
        c.fillStyle(Color(BLUE));
        c.fillRect(px, py, b.w, b.h);
        c.lineWidth(7);
        c.strokeStyle(Color(BLUE_DARK));
        c.strokeRect(px, py, b.w, b.h);
        outlinedText(c, d.title, px + d.headW * .5f, py + 26.8f, 24);
        // close button
        float cx = px + d.headW - 35, cy = py + 14.3f;
        uint32_t red = open && hover == MENU_CLOSE ? mix(0xbb5555, 0xffffff, .1f) : 0xbb5555;
        roundRect(c, cx, cy, 25, 25, 4);
        c.fillStyle(Color(red));
        c.fill();
        c.lineWidth(4);
        c.strokeStyle(Color(0x974545));
        c.stroke();
        c.lineWidth(3.75f);
        c.strokeStyle(Color(0xcccccc));
        c.lineCap(cp6::Cap::Round);
        c.beginPath();
        c.moveTo(cx + 6.25f, cy + 6.25f);
        c.lineTo(cx + 18.75f, cy + 18.75f);
        c.stroke();
        c.beginPath();
        c.moveTo(cx + 18.75f, cy + 6.25f);
        c.lineTo(cx + 6.25f, cy + 18.75f);
        c.stroke();
        c.lineCap(cp6::Cap::Butt);

        Clip cl = contentRect(px, py, b.w, b.h);
        float maxScroll = std::max(0.f, contentH_ - cl.h);
        if (open) scrollPanel(b, maxScroll, dt, now);
        float scroll = panelScroll_[p], contentH = contentH_;
        c.save();
        c.beginPath();
        c.rect(cl.x, cl.y, cl.w, cl.h);
        c.clip();
        drawPanel(c, now, p, cl.x, cl.y - scroll, cl.w, cl.h);
        c.restore();
        if (!open)
            contentH_ = contentH;  // only the open panel's height counts for scrolling
        else
            contentH = contentH_;
        c.setTransform(u, 0, 0, u, 0, 0);
        if (open) {
            // fades in fast and out slowly
            bool show = contentH > cl.h + .001f && (!game_.mobile || now - scrollAt_ <= 500);
            barK_ += ((show ? 1.f : 0.f) - barK_) *
                     std::min(1.f, show ? 1 - std::exp(-.06f * dt) : 1 - std::pow(.9f, dt * .06f));
            thumb_[0] = thumb_[2] = 0;
            if (barK_ > .0001f && contentH > 0) {
                float track = cl.h - 8, start = std::clamp(scroll / contentH, 0.f, 1.f);
                float len = std::min(1 - start, cl.h / contentH);
                float x = cl.x + cl.w - 4, top = cl.y + 4 + track * start, bottom = top + track * len;
                c.globalAlpha(.2f * std::min(1.f, k) * barK_);
                c.lineWidth(8);
                c.strokeStyle(Color(0x000000));
                c.lineCap(cp6::Cap::Round);
                c.beginPath();
                c.moveTo(x, top);
                c.lineTo(x, bottom);
                c.stroke();
                c.lineCap(cp6::Cap::Butt);
                thumb_[0] = x - 4;
                thumb_[1] = top;
                thumb_[2] = x + 12;
                thumb_[3] = bottom;
            }
        }
    }
    c.globalAlpha(1);

    // the buttons
    for (int i = 0; i < 4; i++) {
        const Button &b = kButtons[i];
        uint32_t fill = panel_ == i ? mix(BLUE, 0, .1f) : hover == i ? mix(BLUE, 0xffffff, .1f) : BLUE;
        roundRect(c, b.x0, Hu - BUTTON_TOP, b.x1 - b.x0, BUTTON_H, 5);
        c.fillStyle(Color(fill));
        c.fill();
        c.lineWidth(5);
        c.strokeStyle(Color(BLUE_DARK));
        c.stroke();
        // button labels are set a little narrow
        c.save();
        c.translate(b.textX, Hu - BUTTON_TOP + BUTTON_H * .5f);
        c.scale(.826f, 1);
        outlinedText(c, b.label, 0, 0, 16);
        c.restore();
    }
    // how much of the petal gallery is filled in
    int seen = 0;
    for (int t = 1; t < P_COUNT; t++) seen += seenPetal_[t];
    float pcx = 203, pcy = Hu - 27;
    c.fillStyle(Color(0x2d4f6d));
    c.beginPath();
    c.arc(pcx, pcy, 10, 0, 2 * PI);
    c.fill();
    c.fillStyle(Color(0xaccfed));
    if (seen >= P_COUNT - 1) {
        c.beginPath();
        c.arc(pcx, pcy, 8, 0, 2 * PI);
        c.fill();
    } else if (seen) {
        float a0 = -PI * .5f, a1 = a0 + 2 * PI * float(seen) / float(P_COUNT - 1);
        c.beginPath();
        c.moveTo(pcx, pcy);
        c.arc(pcx, pcy, 8, a0, a1);
        c.closePath();
        c.fill();
    }

    // community links; the tooltip under a button fades in while it is hovered
    static const cp6::Path2D eyes = parsePath(kDiscordEyes), bubble = parsePath(kDiscordBubble),
                             alien = parsePath(kReddit);
    float la = game_.ui.hudW();
    for (int i = 0; i < 2; i++) {
        const Link &k = kLinks[i];
        float cx = la - k.fromRight, cy = 40;
        roundRect(c, cx - 20, cy - 20, 40, 40, 2.5f);
        c.fillStyle(Color(hover == MENU_DISCORD + i ? mix(k.fill, 0xffffff, .1f) : k.fill));
        c.fill();
        c.lineWidth(2.5f);
        c.strokeStyle(Color(k.stroke));
        c.stroke();
        c.save();
        c.fillStyle(Color(0xffffff));
        if (i == 0) {
            c.translate(cx - 15, cy - 15);
            c.scale(.125f);
            c.fill(eyes);
            c.fill(bubble);
        } else {
            c.translate(cx - 12, cy - 12.5f);
            c.fill(alien);
        }
        c.restore();
    }

    // descriptions of gallery petals, each fading in while its card is hovered
    for (auto &t : galleryTips_) {
        t.k += ((t.over ? 1.f : 0.f) - t.k) * k7;
        if (t.k < .01f || !t.type) continue;
        c.setTransform(u, 0, 0, u, 0, 0);
        petalTooltip(c, t.type, t.x, t.top, t.k, Wu, Hu);
    }
    for (int i = 0; i < 2; i++) {
        float &a = linkTipAlpha_[i];
        a += ((hover == MENU_DISCORD + i ? 1.f : 0.f) - a) * ease(.7f, dt);
        if (a < .01f) continue;
        c.setTransform(u, 0, 0, u, 0, 0);
        BoxLine line{kLinks[i].tip, 16};
        textPanel(c, &line, 1, la - kLinks[i].fromRight, 65, BoxPlace::Middle, 0, a, Wu, Hu);
    }
    c.globalAlpha(1);
}

void Menus::openLink(int target) {
    if (target == MENU_DISCORD || target == MENU_REDDIT) ui_open(kLinks[target - MENU_DISCORD].url);
}

// the inside of the open panel; also measures how tall it is for scrolling
// top is where the content starts, scrolled; y and h are the visible part
void Menus::drawPanel(Context &c, double now, int panel, float x, float top, float w, float h) {
    float u = game_.ui.hudScale(), mx = in.mx / u, my = in.my / u, y = top + panelScroll_[panel], a = c.globalAlpha();
    bool inside = panel == panel_ && mx >= x && mx <= x + w && my >= y && my <= y + h;
    // the hover area is the size of an unknown card; the tooltip points at `above` over its centre
    // (40 in the card's own scale: drop cards are drawn at .7)
    auto hoverCard = [&](int item, int type, float cx, float cy, float size, float above) {
        GalleryTip &t = galleryTips_[item & 511];
        t.type = type;
        t.x = cx;
        t.top = cy - above + 12.5f;
        t.over = inside && std::fabs(mx - cx) <= size * .5f && std::fabs(my - cy) <= size * .5f;
    };
    switch (panel) {
        case MENU_SETTINGS: {
            int list[5], n = settingsList(list);
            for (int i = 0; i < n; i++) {
                float bx = x + 17.5f, by = y + 18 + 45 * i;
                c.lineWidth(4);
                c.fillStyle(Color(setting(list[i]) ? 0xdddddd : 0x666666));
                c.strokeStyle(Color(0x333333));
                c.beginPath();
                c.rect(bx - 12.5f, by - 12.5f, 25, 25);
                c.fill();
                c.stroke();
                textBox(c, kSettingLabel[list[i]], x + 40, by - 11.2f, 16);
            }
            contentH_ = h;
            break;
        }
        case MENU_PETALS: {
            const auto &order = petalOrder();
            for (int i = 0; i < int(order.size()); i++) {
                float cx = x + 45 + 70 * float(i % 4), cy = top + 35 + 70 * float(i / 4);
                if (cy < y - 40 || cy > y + h + 40) continue;
                int t = order[i];
                if (seenPetal_[t]) {
                    card(c, cx, cy, 55, t, now, a);
                    hoverCard(i, t, cx, cy, 60, 40);
                } else
                    unknownCard(c, cx, cy, 60);
            }
            contentH_ = float((order.size() + 3) / 4) * 70 + 10;
            break;
        }
        case MENU_MOBS: {
            float ry = top + 5;
            int mi = -1;
            for (int t : mobOrder()) {
                mi++;
                const MobDef &info = mobDef(t);
                std::vector<std::string> lines;
                float rh = 45.2f;
                if (seenMob_[t]) {
                    lines = wrapText(c, info.desc, 14, DESC_W);
                    rh = mobTextBottom(int(lines.size())) + 82;
                }
                if (ry + rh >= y && ry <= y + h) {
                    c.fillStyle(Color(BLUE_DARK));
                    roundRect(c, x, ry, ROW_W, rh, 5);
                    c.fill();
                    if (!seenMob_[t]) {
                        outlinedText(c, "?", x + ROW_W * .5f, ry + rh * .5f, 18);
                    } else {
                        textBox(c, mobName(t), x + 10, ry + 10, 18);
                        textBox(c, rarityName(mobRarity(t)), x + 10, ry + 35.2f, 14, rarityColor(mobRarity(t)));
                        for (int i = 0; i < int(lines.size()); i++)
                            textBox(c, lines[i], x + 10, ry + 59.8f + 19.6f * float(i), 14);
                        // the mob's picture
                        float bx = x + ROW_W - 70, by = ry + 10;
                        c.save();
                        c.fillStyle(Color(BLUE));
                        roundRect(c, bx, by, 60, 60, 5);
                        c.fill();
                        c.clip();
                        c.translate(bx + 30, by + 30);
                        c.rotate(-PI * .75f);
                        Icon ic = mobIcon(t);
                        c.scale(ic.scale);
                        c.translate(-ic.back, 0);
                        Look l;
                        l.ui = true;
                        l.time = now;
                        if (t == M_CENTIPEDE || t == M_EVIL_CENTIPEDE) {
                            c.save();
                            c.translate(-70, 0);
                            drawMob(c, t == M_CENTIPEDE ? M_CENTIPEDE_BODY : M_EVIL_CENTIPEDE_BODY, ic.r, l);
                            c.restore();
                        }
                        drawMob(c, t, ic.r, l);
                        c.restore();
                        // what it drops
                        float cy = ry + mobTextBottom(int(lines.size())) + 34.57f;
                        for (int i = 0; i < info.dropCount; i++) {
                            const Drop &drop = info.drops[info.dropCount - 1 - i];  // likeliest first
                            float cx = x + 35.07f + 49 * float(i);
                            int p = drop.petal;
                            if (seenPetal_[p]) {
                                card(c, cx, cy, 38.5f, p, now, a);
                                hoverCard(64 + mi * 8 + i, p, cx, cy, 42, 28);
                            } else
                                unknownCard(c, cx, cy, 42);
                            outlinedText(c, percent(drop.chance), cx, cy + 31.43f, 9.8f);
                        }
                    }
                }
                ry += rh + 10;
            }
            contentH_ = ry - top - 5;
            break;
        }
        case MENU_CHANGELOG: {
            int count;
            const ChangelogEntry *log = changelog(count);
            float cy = top;
            for (int e = 0; e < count; e++) {
                if (cy + 28 >= y && cy <= y + h) textBox(c, log[e].date, x, cy, 20);
                cy += 38;
                for (int k = 0; k < 8 && log[e].lines[k]; k++) {
                    std::string line = log[e].lines[k];
                    bool first = true;
                    size_t i = 0;
                    while (i <= line.size()) {
                        size_t j = line.find('\n', i);
                        if (j == std::string::npos) j = line.size();
                        for (const auto &row : wrapText(c, line.substr(i, j - i), 14, 435)) {
                            if (cy + 19.6f >= y && cy <= y + h) {
                                if (first) textBox(c, "-", x + 10, cy, 14);
                                textBox(c, row, x + 20.7f, cy, 14);
                            }
                            first = false;
                            cy += 19.6f;
                        }
                        i = j + 1;
                    }
                }
                cy += 10;
                c.lineWidth(7);
                c.strokeStyle(Color(BLUE_DARK));
                c.lineCap(cp6::Cap::Round);
                c.beginPath();
                c.moveTo(x + 5, cy + 10);
                c.lineTo(x + 459, cy + 10);
                c.stroke();
                c.lineCap(cp6::Cap::Butt);
                cy += 20;
            }
            outlinedText(c, "Older changelog entries not available", x + w * .5f, cy + 9.8f, 14);
            cy += 29.6f;
            contentH_ = cy - top;
            break;
        }
        default: break;
    }
}

}  // namespace game
