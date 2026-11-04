#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <string>

#include "game/Game.h"

static game::Game g;

static float dpr() {
    return float(emscripten_get_device_pixel_ratio());
}

static bool onMouse(int type, const EmscriptenMouseEvent *e, void *) {
    float k = dpr();
    g.in.mx = float(e->targetX) * k;
    g.in.my = float(e->targetY) * k;
    if (type == EMSCRIPTEN_EVENT_MOUSEDOWN || type == EMSCRIPTEN_EVENT_MOUSEUP) {
        bool down = type == EMSCRIPTEN_EVENT_MOUSEDOWN;
        if (g.onButton(e->button, down)) return true;  // the HUD took it
        if (e->button == 0) {
            g.in.left = down;
            if (down) {
                g.in.clicked = true;
                g.in.clickX = g.in.mx;
                g.in.clickY = g.in.my;
            }
        }
        if (e->button == 2) g.in.right = down;
    }
    return type != EMSCRIPTEN_EVENT_MOUSEMOVE;
}

static bool onKey(int type, const EmscriptenKeyboardEvent *e, void *) {
    g.onKey(e->key, e->code, type == EMSCRIPTEN_EVENT_KEYDOWN);
    // keep the browser from acting on keys the game uses
    return !(e->ctrlKey || e->metaKey) && std::string(e->key) != "F5" && std::string(e->key) != "F12";
}

// with touch controls every finger is tracked; without, the browser turns touches into mouse events
static bool onTouch(int type, const EmscriptenTouchEvent *e, void *) {
    if (type == EMSCRIPTEN_EVENT_TOUCHSTART) g.noteTouch();
    if (!g.touchMode()) return false;
    float k = dpr();
    int phase = type == EMSCRIPTEN_EVENT_TOUCHSTART ? 0 : type == EMSCRIPTEN_EVENT_TOUCHMOVE ? 1 : 2;
    for (int i = 0; i < e->numTouches; i++) {
        const EmscriptenTouchPoint &t = e->touches[i];
        if (t.isChanged) g.onTouch(phase, t.identifier, float(t.targetX) * k, float(t.targetY) * k);
    }
    return true;
}

static bool onBlur(int, const EmscriptenFocusEvent *, void *) {
    g.in.left = g.in.right = g.in.space = g.in.shift = false;
    g.in.up = g.in.down = g.in.leftKey = g.in.rightKey = false;
    return false;
}

static void tick() {
    g.frame(emscripten_get_now());
}

int main() {
    if (!g.init()) {
        emscripten_run_script(
            "document.body.innerHTML='<p style=\"color:#fff;font:20px sans-serif;padding:20px\">WebGL 2 is not "
            "available.</p>'");
        return 0;
    }
    const char *win = EMSCRIPTEN_EVENT_TARGET_WINDOW;
    emscripten_set_mousemove_callback("#canvas", nullptr, true, onMouse);
    emscripten_set_mousedown_callback("#canvas", nullptr, true, onMouse);
    emscripten_set_mouseup_callback(win, nullptr, true, onMouse);
    emscripten_set_keydown_callback(win, nullptr, true, onKey);
    emscripten_set_keyup_callback(win, nullptr, true, onKey);
    emscripten_set_touchstart_callback("#canvas", nullptr, true, onTouch);
    emscripten_set_touchmove_callback("#canvas", nullptr, true, onTouch);
    emscripten_set_touchend_callback("#canvas", nullptr, true, onTouch);
    emscripten_set_touchcancel_callback("#canvas", nullptr, true, onTouch);
    emscripten_set_blur_callback(win, nullptr, true, onBlur);
    emscripten_set_main_loop(tick, 0, false);
    return 0;
}
