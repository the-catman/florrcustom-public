// The cards at the bottom of the screen: where each one is on its way to, dragging them about, and
// picking them with the keyboard.
#pragma once
#include <cstdint>
#include <vector>

#include "cp6/Context.h"
#include "net/Conn.h"
#include "sim/Simulation.h"

namespace game {

class Game;
struct Input;

class Inventory {
public:
    explicit Inventory(Game &game);

    static int primarySlots(int level);

    // slot list: the primary slots first (5 to 8 by level), then 8 secondary ones; 16 is the trash
    int primaryCount() const;
    int slotType(int slot) const;
    int slotAt(float x, float y) const;  // canvas pixels; -1 when over none
    bool dragging() const { return drag_.active; }
    bool stripOpen() const { return stripOpen_; }

    void onKey(const char *code);
    bool onButton(bool down);
    // a card picked up from the ground starts from here, in HUD units
    void notePickup(int slot, float x, float y, double now);
    void update(double now);  // once a frame, before drawing
    void draw(cp6::Context &c, double now, int level, float Wu, float Hu);
    void drawTooltips(cp6::Context &c, double now);

private:
    // every card glides to where it belongs: its slot, the pointer while dragged, the slot it is
    // held over, or the slot it was sent to until the server answers. New cards grow in and cards
    // that go away shrink out.
    struct Card {
        int type = 0;
        float x = 0, y = 0, size = 0, alpha = 0;  // HUD units, as drawn
        float dragK = 0, px = 0, py = 0;          // held, and where the pointer was
        float overK = 0, ox = 0, oy = 0, os = 0;  // blended into this slot position and size
        int target = -1;                          // sent to this slot, waiting for the server
        double until = 0;
        double born = 0;
        float reload = 1;    // as shown: drops at once, eases back up to the server's value
        float wedgeOff = 1;  // the reload wedge fades out in the second row
        float tipK = 0;      // its tooltip, shown while hovered
    };
    struct Drag {
        bool active = false;
        int from = -1;
        int type = 0;
    };
    struct Pickup {
        bool on = false;
        float x = 0, y = 0;
        double at = 0;
    };

    void layout(float Wu, float Hu, int level, float *px, float *py, float *sx, float *sy, int &n) const;
    void updateCards(double now, float dt, const float *px, const float *py, const float *sx, const float *sy, int n);
    void sendCard(int from, int to);  // a card was put in another slot (16: the trash)
    bool slotBusy(int slot) const;
    bool slotPlace(int slot, float &x, float &y, float &size) const;  // HUD units; false without a flower
    int cardAt(float x, float y) const;                               // HUD units; the card drawn there, or -1

    Game &game_;
    sim::Simulation &sim_;
    cp6::Context &ctx_;
    net::Conn &conn_;
    Input &in;

    Card cards_[16];
    std::vector<Card> leaving_;
    uint32_t cardsOwner_ = 0;
    int cardsPrimary_ = 0;
    Pickup pickup_[16];
    Drag drag_;
    int dragOver_ = -1;  // where the dragged card would land: a slot it is held over, or -1
    double flyLast_ = 0;
    double tipTime_ = 0;

    // keyboard inventory: E and Q pick a card in the second row, a digit swaps it into that slot,
    // T deletes it; it lets go after 5 seconds without a key
    bool selOn_ = false;
    int selSlot_ = 0;
    double selKeyAt_ = 0;
    float selAlpha_ = 0;
    float trashAlpha_ = 0;
    float selWiggle_[16] = {};
    // the strip under the inventory grows over it while the mouse is inside, and movement stops
    bool stripOpen_ = false;
    float stripK_ = 0;
};

}  // namespace game
