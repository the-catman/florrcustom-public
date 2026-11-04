// Drawing helpers shared by the HUD and the title screen menus, in UI units.
#pragma once
#include <string>

#include "cp6/Context.h"

namespace game {

// a petal card: rarity background, frame, picture and name; reload darkens it while below 1
void card(cp6::Context &c, float x, float y, float size, int type, double now, float alpha, float reload = 1,
          float wedgeAlpha = 1);
// white text with a black outline, centred vertically on y
void outlinedText(cp6::Context &c, const std::string &s, float x, float y, float size,
                  cp6::Color fill = cp6::Color(0xffffff), cp6::Align a = cp6::Align::Center);
inline bool inBox(float x, float y, float x0, float y0, float x1, float y1) {
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}
// a rounded rectangle path
void roundRect(cp6::Context &c, float x, float y, float w, float h, float r);
// lines of text in a translucent dark box, kept 10 units inside the screen. x is the box's centre;
// y is its top edge plus 5 for Below, its middle line for Middle, and its bottom edge minus 5 for Above.
// extraW widens the box on the right. Returns the area inside the padding.
struct BoxLine {
    std::string text;
    float size;
    uint32_t color = 0xffffff;
    float dx = 0;
};
enum class BoxPlace { Below, Middle, Above };
struct BoxArea {
    float x, y, w, h;
};
BoxArea textPanel(cp6::Context &c, const BoxLine *lines, int count, float x, float y, BoxPlace place, float extraW,
                  float alpha, float Wu, float Hu);
// name, rarity and description of a petal in a dark box above a card whose top edge is at top
void petalTooltip(cp6::Context &c, int type, float cx, float top, float alpha, float Wu, float Hu);

}  // namespace game
