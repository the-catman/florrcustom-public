#include "cp6/Context.h"

#include <algorithm>
#include <cmath>

extern "C" {
int cp6_init();
void cp6_size(void *out);
void cp6_flush(const void *inst, int count, const float *path, int texels, const int *batches, int nbatch,
               uint32_t clear);
void cp6_glyph(uint32_t cp, float *out);
int cp6_font_ready();
}

namespace cp6 {

enum Kind { RECT = 0, ELLIPSE = 1, PATH = 2, GLYPH = 4, IMAGE = 5, GRID = 6 };
static constexpr float TAU = 6.28318530718f;

Color Color::lerp(Color x, Color y, float t) {
    Color o;
    o.r = uint8_t(x.r + (y.r - x.r) * t + .5f);
    o.g = uint8_t(x.g + (y.g - x.g) * t + .5f);
    o.b = uint8_t(x.b + (y.b - x.b) * t + .5f);
    o.a = uint8_t(x.a + (y.a - x.a) * t + .5f);
    return o;
}

float Matrix::scale() const {
    return std::sqrt(std::max(std::fabs(a * d - b * c), 1e-12f));
}

Matrix Matrix::operator*(const Matrix &m) const {
    Matrix o;
    o.a = a * m.a + c * m.b;
    o.b = b * m.a + d * m.b;
    o.c = a * m.c + c * m.d;
    o.d = b * m.c + d * m.d;
    o.e = a * m.e + c * m.f + e;
    o.f = b * m.e + d * m.f + f;
    return o;
}

// ---- Path2D ----

void Path2D::moveTo(float x, float y) {
    subs.push_back({int(pts.size() / 2), 1, false});
    pts.push_back(x);
    pts.push_back(y);
    ops++;
    circle = false;
}
void Path2D::ensure(float x, float y) {
    if (subs.empty() || subs.back().closed) moveTo(x, y);
}
void Path2D::lineTo(float x, float y) {
    if (subs.empty()) {
        moveTo(x, y);
        return;
    }
    if (subs.back().closed) {
        float sx = pts[subs.back().start * 2], sy = pts[subs.back().start * 2 + 1];
        moveTo(sx, sy);
    }
    pts.push_back(x);
    pts.push_back(y);
    subs.back().count++;
    ops++;
    circle = false;
}
void Path2D::closePath() {
    if (subs.empty()) return;
    subs.back().closed = true;
    ops++;
}
void Path2D::arc(float x, float y, float r, float a0, float a1, bool ccw) {
    ellipse(x, y, r, r, 0, a0, a1, ccw);
}
void Path2D::ellipse(float x, float y, float rx, float ry, float rot, float a0, float a1, bool ccw) {
    float sweep = a1 - a0;
    if (!ccw) {
        if (sweep >= TAU)
            sweep = TAU;
        else if (sweep < 0)
            sweep = std::fmod(sweep, TAU) + TAU;
    } else {
        if (sweep <= -TAU)
            sweep = -TAU;
        else if (sweep > 0)
            sweep = std::fmod(sweep, TAU) - TAU;
    }
    bool full = std::fabs(std::fabs(sweep) - TAU) < 1e-4f;
    bool wasEmpty = subs.empty();
    float r = std::max(rx, ry) * detail;
    int n = std::max(3, std::min(256, int(std::ceil(std::fabs(sweep) * std::sqrt(std::max(r, 1.f)) * 1.1f))));
    float cr_ = std::cos(rot), sr = std::sin(rot);
    bool starts = subs.empty() || subs.back().closed;
    for (int i = 0; i <= n; i++) {
        if (full && i == n) break;
        float t = a0 + sweep * i / n;
        float px = std::cos(t) * rx, py = std::sin(t) * ry;
        float X = x + px * cr_ - py * sr, Y = y + px * sr + py * cr_;
        if (i == 0) {
            if (subs.empty() || subs.back().closed)
                moveTo(X, Y);
            else
                lineTo(X, Y);
        } else
            lineTo(X, Y);
    }
    if (full && starts) subs.back().closed = true;
    if (full && wasEmpty && rx == ry) {
        circle = true;
        cx = x;
        cy = y;
        cr = rx;
    }
}
void Path2D::quadraticCurveTo(float qx, float qy, float x, float y) {
    if (subs.empty()) moveTo(qx, qy);
    float x0 = pts[pts.size() - 2], y0 = pts[pts.size() - 1];
    float len = std::hypot(qx - x0, qy - y0) + std::hypot(x - qx, y - qy);
    int n = std::max(2, std::min(64, int(std::sqrt(len * detail) * 1.2f)));
    for (int i = 1; i <= n; i++) {
        float t = float(i) / n, u = 1 - t;
        lineTo(u * u * x0 + 2 * u * t * qx + t * t * x, u * u * y0 + 2 * u * t * qy + t * t * y);
    }
}
void Path2D::bezierCurveTo(float c1x, float c1y, float c2x, float c2y, float x, float y) {
    if (subs.empty()) moveTo(c1x, c1y);
    float x0 = pts[pts.size() - 2], y0 = pts[pts.size() - 1];
    float len = std::hypot(c1x - x0, c1y - y0) + std::hypot(c2x - c1x, c2y - c1y) + std::hypot(x - c2x, y - c2y);
    int n = std::max(2, std::min(96, int(std::sqrt(len * detail) * 1.4f)));
    for (int i = 1; i <= n; i++) {
        float t = float(i) / n, u = 1 - t;
        float a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
        lineTo(a * x0 + b * c1x + c * c2x + d * x, a * y0 + b * c1y + c * c2y + d * y);
    }
}
void Path2D::rect(float x, float y, float w, float h) {
    moveTo(x, y);
    lineTo(x + w, y);
    lineTo(x + w, y + h);
    lineTo(x, y + h);
    closePath();
    moveTo(x, y);
}

// ---- Context ----

bool Context::init() {
    if (!cp6_init()) return false;
    inst_.reserve(4096);
    path_.reserve(1 << 16);
    return true;
}

void Context::beginFrame(uint32_t clearRgb) {
    struct {
        int w, h;
        float dpr;
    } sz;
    cp6_size(&sz);
    w_ = sz.w;
    h_ = sz.h;
    dpr_ = sz.dpr;
    clear_ = clearRgb;
    inst_.clear();
    path_.clear();
    batches_.clear();
    stack_.clear();
    marks_.clear();
    inLayer_ = false;
    s_ = State();
    cur_.clear();
    lastFill_ = -1;
    if (!fontReady_ && cp6_font_ready()) {
        fontReady_ = true;
        glyphs_.clear();
    }
}

void Context::endFrame() {
    if (inLayer_) endLayer(1);
    int n = int(inst_.size());
    // batches are (start, count, image) triples; image -1 switches drawing to the layer and clears
    // it, -2 switches back to the canvas, and -3 draws with the layer as the image
    batches_.clear();
    int start = 0;
    auto batch = [&](int from, int count, int img) {
        batches_.push_back(from);
        batches_.push_back(count);
        batches_.push_back(img);
    };
    for (size_t k = 0; k < marks_.size(); k += 2) {
        int at = marks_[k], what = marks_[k + 1];
        if (at > start) batch(start, at - start, 0);
        if (what == 1) {
            batch(at, 0, -1);
            start = at;
        } else {
            batch(at, 0, -2);
            batch(at, 1, -3);
            start = at + 1;
        }  // the next instance is the layer
    }
    if (n > start) batch(start, n - start, 0);
    while (path_.size() % 2048) path_.push_back(0);
    cp6_flush(inst_.data(), n, path_.data(), int(path_.size() / 2), batches_.data(), int(batches_.size() / 3), clear_);
}

void Context::beginLayer() {
    if (inLayer_) return;
    inLayer_ = true;
    marks_.push_back(int(inst_.size()));
    marks_.push_back(1);
    lastFill_ = -1;
}

void Context::endLayer(float alpha) {
    if (!inLayer_) return;
    inLayer_ = false;
    marks_.push_back(int(inst_.size()));
    marks_.push_back(2);
    // the layer covers the canvas; it is stored bottom up, so v runs from 1 at the top to 0
    Inst &i = push(5, Matrix{1, 0, 0, 1, 0, 0});
    i.lx = w_ * .5f;
    i.ly = h_ * .5f;
    i.hx = w_ * .5f;
    i.hy = h_ * .5f;
    i.p0 = 0;
    i.p1 = 1;
    i.p2 = 1;
    i.p3 = 0;
    i.alpha = alpha;
    i.clipOff = 0;
    i.clipCount = 0;
    lastFill_ = -1;
}

// c in the recolour's hue and saturation at its own lightness (HSL): black stays black and
// white stays white
Color Context::recolored(Color c) const {
    auto lightness = [](Color x) { return (std::max({x.r, x.g, x.b}) + std::min({x.r, x.g, x.b})) / 510.f; };
    Color to(s_.recolor);
    float l = lightness(c), lt = lightness(to);
    float roomT = 1 - std::fabs(2 * lt - 1), k = roomT > 0 ? (1 - std::fabs(2 * l - 1)) / roomT : 0;
    auto at = [&](uint8_t v) { return uint8_t(std::clamp((l + (v / 255.f - lt) * k) * 255.f + .5f, 0.f, 255.f)); };
    Color o;
    o.r = at(to.r);
    o.g = at(to.g);
    o.b = at(to.b);
    o = Color::lerp(c, o, s_.recolorK);
    o.a = c.a;
    return o;
}

void Context::save() {
    stack_.push_back(s_);
}
void Context::restore() {
    if (stack_.empty()) return;
    s_ = stack_.back();
    stack_.pop_back();
    lastFill_ = -1;
}
void Context::setTransform(float a, float b, float c, float d, float e, float f) {
    s_.m = {a, b, c, d, e, f};
    lastFill_ = -1;
}
void Context::transform(float a, float b, float c, float d, float e, float f) {
    s_.m = s_.m * Matrix{a, b, c, d, e, f};
    lastFill_ = -1;
}
void Context::translate(float x, float y) {
    transform(1, 0, 0, 1, x, y);
}
void Context::rotate(float r) {
    float c = std::cos(r), s = std::sin(r);
    transform(c, s, -s, c, 0, 0);
}
void Context::scale(float x, float y) {
    transform(x, 0, 0, y, 0, 0);
}

Context::Inst &Context::push(int kind, const Matrix &m) {
    inst_.emplace_back();
    Inst &i = inst_.back();
    i.e = m.e;
    i.f = m.f;
    i.a = m.a;
    i.b = m.b;
    i.c = m.c;
    i.d = m.d;
    i.kind = float(kind);
    i.sw = 0;
    i.p0 = i.p1 = i.p2 = i.p3 = i.p4 = 0;
    i.alpha = s_.alpha;
    i.clipOff = float(s_.clipOff);
    i.clipCount = float(s_.clipCount);
    i.fill = 0;
    i.stroke = 0;
    lastFill_ = -1;
    return i;
}

// The current path keeps device-space points; apply the matrix while building.
void Context::beginPath() {
    cur_.clear();
    cur_.ops++;
    lastFill_ = -1;
}
void Context::moveTo(float x, float y) {
    float X, Y;
    s_.m.apply(x, y, X, Y);
    cur_.moveTo(X, Y);
}
void Context::lineTo(float x, float y) {
    float X, Y;
    s_.m.apply(x, y, X, Y);
    cur_.lineTo(X, Y);
}
void Context::closePath() {
    cur_.closePath();
}
void Context::arc(float x, float y, float r, float a0, float a1, bool ccw) {
    ellipse(x, y, r, r, 0, a0, a1, ccw);
}
void Context::ellipse(float x, float y, float rx, float ry, float rot, float a0, float a1, bool ccw) {
    // build in local space, then transform the new points
    Path2D tmp;
    tmp.detail = s_.m.scale();
    if (!cur_.subs.empty() && !cur_.subs.back().closed) {
        tmp.moveTo(0, 0);  // placeholder so the arc connects with lineTo
    }
    bool connect = !tmp.subs.empty();
    tmp.ellipse(x, y, rx, ry, rot, a0, a1, ccw);
    bool wasEmpty = cur_.subs.empty();
    int start = connect ? 1 : 0;
    for (size_t k = start; k < tmp.pts.size() / 2; k++) {
        float X, Y;
        s_.m.apply(tmp.pts[k * 2], tmp.pts[k * 2 + 1], X, Y);
        if (k == size_t(start) && !connect)
            cur_.moveTo(X, Y);
        else
            cur_.lineTo(X, Y);
    }
    if (!tmp.subs.empty() && tmp.subs.back().closed) cur_.closePath();
    if (tmp.circle && wasEmpty && std::fabs(s_.m.a - s_.m.d) < 1e-5f && std::fabs(s_.m.b + s_.m.c) < 1e-5f) {
        cur_.circle = true;
        s_.m.apply(x, y, cur_.cx, cur_.cy);
        cur_.cr = rx * s_.m.scale();
    }
}
void Context::quadraticCurveTo(float cx, float cy, float x, float y) {
    float a, b, c, d;
    s_.m.apply(cx, cy, a, b);
    s_.m.apply(x, y, c, d);
    cur_.detail = 1;
    cur_.quadraticCurveTo(a, b, c, d);
}
void Context::bezierCurveTo(float c1x, float c1y, float c2x, float c2y, float x, float y) {
    float a, b, c, d, e, f;
    s_.m.apply(c1x, c1y, a, b);
    s_.m.apply(c2x, c2y, c, d);
    s_.m.apply(x, y, e, f);
    cur_.detail = 1;
    cur_.bezierCurveTo(a, b, c, d, e, f);
}
void Context::rect(float x, float y, float w, float h) {
    moveTo(x, y);
    lineTo(x + w, y);
    lineTo(x + w, y + h);
    lineTo(x, y + h);
    closePath();
}

int Context::uploadPath(const Path2D &p, const Matrix *m) {
    int off = int(path_.size() / 2);
    for (const auto &s : p.subs) {
        if (s.count < 2) continue;
        path_.push_back(float(s.count));
        path_.push_back(s.closed ? 1.f : 0.f);
        for (int k = 0; k < s.count; k++) {
            float x = p.pts[(s.start + k) * 2], y = p.pts[(s.start + k) * 2 + 1];
            if (m) m->apply(x, y, x, y);
            path_.push_back(x);
            path_.push_back(y);
        }
    }
    return int(path_.size() / 2) - off;
}

void Context::pathInstance(const Path2D &p, bool doFill, bool doStroke, bool evenOdd, bool device) {
    // device: points already in device space (current path); otherwise local (Path2D object)
    const Matrix &m = s_.m;
    float sc = m.scale();
    if (p.circle && device) {
        Matrix id;
        Inst &i = push(ELLIPSE, id);
        i.lx = p.cx;
        i.ly = p.cy;
        i.hx = i.hy = p.cr;
        if (doFill) i.fill = s_.fill.packed();
        if (doStroke) {
            i.stroke = s_.stroke.packed();
            i.sw = s_.lineWidth * sc;
        }
        return;
    }
    int off = int(path_.size() / 2);
    int cnt = uploadPath(p, device ? nullptr : &m);
    if (!cnt) return;
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    for (int k = off; k < off + cnt;) {
        int n = int(path_[k * 2]);
        k++;
        for (int j = 0; j < n; j++, k++) {
            float x = path_[k * 2], y = path_[k * 2 + 1];
            x0 = std::min(x0, x);
            x1 = std::max(x1, x);
            y0 = std::min(y0, y);
            y1 = std::max(y1, y);
        }
    }
    Matrix id;
    Inst &i = push(PATH, id);
    i.lx = (x0 + x1) * .5f;
    i.ly = (y0 + y1) * .5f;
    i.hx = (x1 - x0) * .5f;
    i.hy = (y1 - y0) * .5f;
    i.p0 = float(off);
    i.p1 = float(cnt);
    i.p2 = evenOdd ? 1.f : 0.f;
    i.p3 = float(int(s_.cap));
    if (doFill) i.fill = s_.fill.packed();
    if (doStroke) {
        i.stroke = s_.stroke.packed();
        i.sw = s_.lineWidth * sc;
    }
}

void Context::fill(bool evenOdd) {
    pathInstance(cur_, true, false, evenOdd, true);
    if (!inst_.empty()) {
        lastFill_ = int(inst_.size()) - 1;
        lastPathOps_ = cur_.ops;
    }
}
void Context::stroke() {
    if (lastFill_ == int(inst_.size()) - 1 && lastFill_ >= 0 && lastPathOps_ == cur_.ops) {
        Inst &i = inst_[lastFill_];
        if (i.alpha == s_.alpha && i.p3 == float(int(s_.cap))) {
            i.stroke = s_.stroke.packed();
            i.sw = s_.lineWidth * s_.m.scale();
            lastFill_ = -1;
            return;
        }
    }
    pathInstance(cur_, false, true, false, true);
}
void Context::fill(const Path2D &p, bool evenOdd) {
    pathInstance(p, true, false, evenOdd, false);
    if (!inst_.empty()) {
        lastFill_ = int(inst_.size()) - 1;
        lastPathOps_ = -2 - p.ops;
    }
}
void Context::stroke(const Path2D &p) {
    if (lastFill_ == int(inst_.size()) - 1 && lastFill_ >= 0 && lastPathOps_ == -2 - p.ops) {
        Inst &i = inst_[lastFill_];
        if (i.alpha == s_.alpha && i.p3 == float(int(s_.cap))) {
            i.stroke = s_.stroke.packed();
            i.sw = s_.lineWidth * s_.m.scale();
            lastFill_ = -1;
            return;
        }
    }
    pathInstance(p, false, true, false, false);
}
// a pixel is drawn only inside every active clip; past MAX_CLIPS the innermost one is replaced
void Context::addClip(int off, int count) {
    if (s_.clipCount < State::MAX_CLIPS) s_.clipCount++;
    s_.clips[s_.clipCount - 1][0] = off;
    s_.clips[s_.clipCount - 1][1] = count;
    s_.clipOff = int(path_.size() / 2);
    for (int k = 0; k < s_.clipCount; k++) {
        path_.push_back(float(s_.clips[k][0]));
        path_.push_back(float(s_.clips[k][1]));
    }
}
// a clip's count is negative for an exact circle: -1 keeps the inside, -2 the outside; its
// offset points at (cx, cy), (r, 0) in device pixels
void Context::addCircleClip(float cx, float cy, float r, bool outside) {
    int off = int(path_.size() / 2);
    path_.push_back(cx);
    path_.push_back(cy);
    path_.push_back(r);
    path_.push_back(0);
    addClip(off, outside ? -2 : -1);
}
void Context::clipCircle(float x, float y, float r, bool outside) {
    const Matrix &m = s_.m;
    addCircleClip(m.a * x + m.c * y + m.e, m.b * x + m.d * y + m.f, r * m.scale(), outside);
}
void Context::clip() {
    if (cur_.circle) {
        addCircleClip(cur_.cx, cur_.cy, cur_.cr, false);
        return;
    }
    int off = int(path_.size() / 2);
    addClip(off, uploadPath(cur_, nullptr));
}
void Context::clip(const Path2D &p) {
    int off = int(path_.size() / 2);
    addClip(off, uploadPath(p, &s_.m));
}

void Context::fillRect(float x, float y, float w, float h) {
    Inst &i = push(RECT, s_.m);
    i.lx = x + w * .5f;
    i.ly = y + h * .5f;
    i.hx = std::fabs(w) * .5f;
    i.hy = std::fabs(h) * .5f;
    i.fill = s_.fill.packed();
}
void Context::strokeRect(float x, float y, float w, float h) {
    Inst &i = push(RECT, s_.m);
    i.lx = x + w * .5f;
    i.ly = y + h * .5f;
    i.hx = std::fabs(w) * .5f;
    i.hy = std::fabs(h) * .5f;
    i.stroke = s_.stroke.packed();
    i.sw = s_.lineWidth;
}
void Context::fillCircle(float x, float y, float r) {
    circle(x, y, r, true, false);
}
void Context::circle(float x, float y, float r, bool doFill, bool doStroke) {
    Inst &i = push(ELLIPSE, s_.m);
    i.lx = x;
    i.ly = y;
    i.hx = i.hy = r;
    if (doFill) i.fill = s_.fill.packed();
    if (doStroke) {
        i.stroke = s_.stroke.packed();
        i.sw = s_.lineWidth;
    }
}
void Context::grid(float x, float y, float w, float h, float s, float ox, float oy) {
    Inst &i = push(GRID, s_.m);
    i.lx = x + w * .5f;
    i.ly = y + h * .5f;
    i.hx = w * .5f;
    i.hy = h * .5f;
    i.p0 = s;
    i.p1 = ox;
    i.p2 = oy;
    i.stroke = s_.stroke.packed();
    i.sw = s_.lineWidth;
}

// ---- text ----

static uint32_t nextCp(const std::string &s, size_t &i) {
    unsigned char c = s[i++];
    if (c < 0x80) return c;
    int n = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : 1;
    uint32_t cp = c & (0x3f >> n);
    while (n-- && i < s.size()) cp = cp << 6 | (s[i++] & 0x3f);
    return cp;
}

const Context::Glyph &Context::glyph(uint32_t cp) {
    auto it = glyphs_.find(cp);
    if (it != glyphs_.end()) return it->second;
    Glyph g{};
    float out[9];
    cp6_glyph(cp, out);
    g.adv = out[0];
    g.x0 = out[1];
    g.y0 = out[2];
    g.x1 = out[3];
    g.y1 = out[4];
    g.u0 = out[5];
    g.v0 = out[6];
    g.u1 = out[7];
    g.v1 = out[8];
    g.ok = g.x1 > g.x0;
    return glyphs_.emplace(cp, g).first->second;
}

float Context::measureText(const std::string &s) {
    if (!fontReady_) return s.size() * s_.fontSize * .55f;
    float w = 0;
    for (size_t i = 0; i < s.size();) w += glyph(nextCp(s, i)).adv;
    return w * s_.fontSize / 48.f;
}

void Context::text(const std::string &s, float x, float y, bool stroke) {
    if (!fontReady_ || s.empty()) return;
    float k = s_.fontSize / 48.f;
    float w = measureText(s);
    if (s_.align == Align::Center)
        x -= w * .5f;
    else if (s_.align == Align::Right)
        x -= w;
    // the browser's em box for Ubuntu: .8074 of the size above the alphabetic baseline, .1926 below
    if (s_.baseline == Baseline::Middle)
        y += s_.fontSize * .3074f;
    else if (s_.baseline == Baseline::Top)
        y += s_.fontSize * .8074f;
    else if (s_.baseline == Baseline::Bottom)
        y -= s_.fontSize * .1926f;
    float pen = x;
    for (size_t i = 0; i < s.size();) {
        const Glyph &g = glyph(nextCp(s, i));
        if (g.ok) {
            Inst &in = push(GLYPH, s_.m);
            float gx0 = pen + g.x0 * k, gx1 = pen + g.x1 * k, gy0 = y + g.y0 * k, gy1 = y + g.y1 * k;
            in.lx = (gx0 + gx1) * .5f;
            in.ly = (gy0 + gy1) * .5f;
            in.hx = (gx1 - gx0) * .5f;
            in.hy = (gy1 - gy0) * .5f;
            in.p0 = g.u0;
            in.p1 = g.v0;
            in.p2 = g.u1;
            in.p3 = g.v1;
            in.p4 = 24.f * k;  // 2 * PAD at the 48px base
            if (stroke) {
                in.stroke = s_.stroke.packed();
                in.sw = s_.lineWidth;
            } else
                in.fill = s_.fill.packed();
        }
        pen += g.adv * k;
    }
}
void Context::fillText(const std::string &s, float x, float y) {
    text(s, x, y, false);
}
void Context::strokeText(const std::string &s, float x, float y) {
    text(s, x, y, true);
}

}  // namespace cp6
