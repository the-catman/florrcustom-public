// cp6: a small Canvas2D-like drawing context that renders with WebGL2.
// Every shape becomes one instanced quad; the fragment shader evaluates its distance field.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace cp6 {

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    Color() = default;
    constexpr Color(uint32_t rgb, float alpha = 1.f)
        : r(uint8_t(rgb >> 16)),
          g(uint8_t(rgb >> 8)),
          b(uint8_t(rgb)),
          a(uint8_t(alpha <= 0   ? 0
                    : alpha >= 1 ? 255
                                 : alpha * 255.f + .5f)) {}
    uint32_t packed() const { return uint32_t(r) | uint32_t(g) << 8 | uint32_t(b) << 16 | uint32_t(a) << 24; }
    static Color lerp(Color x, Color y, float t);
};

struct Matrix {
    float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
    void apply(float x, float y, float &ox, float &oy) const {
        ox = a * x + c * y + e;
        oy = b * x + d * y + f;
    }
    float scale() const;
    Matrix operator*(const Matrix &m) const;  // this then m in local space (this * m)
};

enum class Cap : uint8_t { Butt, Round, Square };
enum class Align : uint8_t { Left, Center, Right };
enum class Baseline : uint8_t { Alphabetic, Middle, Top, Bottom };

struct Path2D {
    // device-independent points: subpaths as (count, closed) headers followed by points
    struct Sub {
        int start, count;
        bool closed;
    };
    std::vector<float> pts;  // x, y pairs
    std::vector<Sub> subs;
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void closePath();
    void arc(float x, float y, float r, float a0, float a1, bool ccw = false);
    void ellipse(float x, float y, float rx, float ry, float rot, float a0, float a1, bool ccw = false);
    void quadraticCurveTo(float cx, float cy, float x, float y);
    void bezierCurveTo(float c1x, float c1y, float c2x, float c2y, float x, float y);
    void rect(float x, float y, float w, float h);
    void clear() {
        pts.clear();
        subs.clear();
        circle = false;
    }
    float detail = 1;  // segments per unit of curvature, set from the transform when flattening
    // a lone full circle can be drawn as an ellipse primitive
    bool circle = false;
    float cx = 0, cy = 0, cr = 0;
    int ops = 0;

private:
    void ensure(float x, float y);
};

struct State {
    Matrix m;
    Color fill{0x000000}, stroke{0x000000};
    float lineWidth = 1, alpha = 1;
    uint32_t recolor = 0;  // fills and strokes set take this colour, as light as they were
    float recolorK = 0;    // how far, 0 not at all
    Cap cap = Cap::Butt;
    float fontSize = 10;
    Align align = Align::Left;
    Baseline baseline = Baseline::Alphabetic;
    // active clips, innermost last; clipOff points at their (offset, count) list in the path data
    static constexpr int MAX_CLIPS = 4;
    int clips[MAX_CLIPS][2] = {};
    int clipCount = 0, clipOff = 0;
};

class Context {
public:
    bool init();
    void beginFrame(uint32_t clearRgb);
    void endFrame();
    int width() const { return w_; }
    int height() const { return h_; }
    float dpr() const { return dpr_; }

    void save();
    void restore();
    void setTransform(float a, float b, float c, float d, float e, float f);
    void resetTransform() { setTransform(1, 0, 0, 1, 0, 0); }
    void transform(float a, float b, float c, float d, float e, float f);
    void translate(float x, float y);
    void rotate(float r);
    void scale(float x, float y);
    void scale(float s) { scale(s, s); }
    const Matrix &matrix() const { return s_.m; }

    void globalAlpha(float a) { s_.alpha = a; }
    float globalAlpha() const { return s_.alpha; }
    void fillStyle(Color c) { s_.fill = s_.recolorK > 0 ? recolored(c) : c; }
    void strokeStyle(Color c) { s_.stroke = s_.recolorK > 0 ? recolored(c) : c; }
    void recolor(uint32_t rgb, float k) {
        s_.recolor = rgb;
        s_.recolorK = k;
    }
    void lineWidth(float w) { s_.lineWidth = w; }
    void lineCap(Cap c) { s_.cap = c; }
    void font(float size) { s_.fontSize = size; }
    void textAlign(Align a) { s_.align = a; }
    void textBaseline(Baseline b) { s_.baseline = b; }

    // the current path (points stored in device space)
    void beginPath();
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void closePath();
    void arc(float x, float y, float r, float a0, float a1, bool ccw = false);
    void ellipse(float x, float y, float rx, float ry, float rot, float a0, float a1, bool ccw = false);
    void quadraticCurveTo(float cx, float cy, float x, float y);
    void bezierCurveTo(float c1x, float c1y, float c2x, float c2y, float x, float y);
    void rect(float x, float y, float w, float h);
    void fill(bool evenOdd = false);
    void stroke();
    void clip();
    // a Path2D in local coordinates, transformed by the current matrix
    void fill(const Path2D &p, bool evenOdd = false);
    void stroke(const Path2D &p);
    void clip(const Path2D &p);
    // a circle clip tested exactly per pixel, keeping what is inside it, or outside it
    void clipCircle(float x, float y, float r, bool outside = false);

    void fillRect(float x, float y, float w, float h);
    void strokeRect(float x, float y, float w, float h);
    void fillCircle(float x, float y, float r);
    // circle with fill and a stroke drawn over it, as one primitive
    void circle(float x, float y, float r, bool doFill, bool doStroke);
    // lines on a grid of spacing s with offset (ox, oy), covering the rect
    void grid(float x, float y, float w, float h, float s, float ox, float oy);

    float measureText(const std::string &s);
    void fillText(const std::string &s, float x, float y);
    void strokeText(const std::string &s, float x, float y);

    // Everything drawn until endLayer goes into a transparent layer the size of the canvas, which
    // endLayer then puts on the canvas at the given alpha, as one picture: shapes that overlap
    // inside the layer cover each other fully instead of each blending with what is under it.
    // Layers don't nest.
    void beginLayer();
    void endLayer(float alpha);

private:
    Color recolored(Color c) const;
    struct Inst {
        float e, f, a, b;
        float c, d, lx, ly;
        float hx, hy, kind, sw;
        float p0, p1, p2, p3;
        float p4, alpha, clipOff, clipCount;
        uint32_t fill, stroke;
    };
    static_assert(sizeof(Inst) == 88, "instance layout");
    struct Glyph {
        float adv, x0, y0, x1, y1, u0, v0, u1, v1;
        bool ok;
    };

    Inst &push(int kind, const Matrix &m);
    void pathInstance(const Path2D &p, bool doFill, bool doStroke, bool evenOdd, bool device);
    int uploadPath(const Path2D &p, const Matrix *m);
    void addClip(int off, int count);
    void addCircleClip(float cx, float cy, float r, bool outside);
    void text(const std::string &s, float x, float y, bool stroke);
    const Glyph &glyph(uint32_t cp);
    bool mergeable(int kind) const;

    std::vector<Inst> inst_;
    std::vector<float> path_;
    std::vector<int> batches_;
    std::vector<int> marks_;  // (instance index, 1 layer starts / 2 layer ends) pairs
    bool inLayer_ = false;
    std::vector<State> stack_;
    State s_;
    Path2D cur_;
    std::unordered_map<uint32_t, Glyph> glyphs_;
    bool fontReady_ = false;
    int w_ = 1, h_ = 1;
    float dpr_ = 1;
    uint32_t clear_ = 0;
    // fill() followed by stroke() of the same path merges into one instance
    int lastFill_ = -1;
    int lastPathOps_ = -1;
};

}  // namespace cp6
