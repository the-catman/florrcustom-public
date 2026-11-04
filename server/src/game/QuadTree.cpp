#include "game/QuadTree.h"

#include <utility>

namespace game {

void QuadTree::setRoot(float x, float y, float w, float h) {
    Node &root = nodes_[0];
    for (uint16_t s : root.items) items_[s].node = -1;
    free(0);
    root.items.clear();
    root.h = h;
    root.w = w;
    root.y = y;
    root.x = x;
}

void QuadTree::setBox(uint16_t slot, float x, float y, float size) {
    Item &item = items_[slot];
    if (item.boxed) return;
    item.boxed = true;
    item.box.w = item.box.h = size;
    item.box.y = y;
    item.box.x = x;
}

void QuadTree::place(uint16_t slot) {
    insert(home(slot), slot);
}

void QuadTree::remove(uint16_t slot) {
    take(slot);
    items_[slot] = Item();
}

// the node an item should be reinserted from: the nearest one up from where it is that it
// still lies strictly inside, or the root
int QuadTree::home(uint16_t slot) const {
    const Item &item = items_[slot];
    const Box &b = item.box;
    int n = item.node;
    if (n < 0) return 0;
    while (nodes_[n].parent >= 0) {
        const Node &t = nodes_[n];
        if (!(b.x < t.x) && !(b.y < t.y) && !(b.x + b.w >= t.x + t.w) && !(b.y + b.h >= t.y + t.h)) return n;
        n = t.parent;
    }
    return n;
}

void QuadTree::take(uint16_t slot) {
    Item &item = items_[slot];
    if (item.node < 0) return;
    auto &v = nodes_[item.node].items;
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i] != slot) continue;
        std::swap(v[i], v.back());
        v.pop_back();
        break;
    }
    item.node = -1;
}

int QuadTree::newKids() {
    if (!freeKids_.empty()) {
        int k = freeKids_.back();
        freeKids_.pop_back();
        return k;
    }
    int k = int(nodes_.size());
    nodes_.resize(nodes_.size() + 4);
    return k;
}

void QuadTree::insert(int node, uint16_t slot) {
    Item &item = items_[slot];
    const Box &b = item.box;
    // down to the smallest node it fits in
    int c = node;
    while (nodes_[c].kids >= 0) {
        const Node &t = nodes_[c];
        float mx = t.x + t.w * 0.5f, my = t.y + t.h * 0.5f;
        int q;
        if (mx > b.x + b.w) {
            if (my > b.y + b.h)
                q = 0;
            else if (my <= b.y)
                q = 2;
            else
                break;
        } else if (mx <= b.x) {
            if (my > b.y + b.h)
                q = 1;
            else if (my <= b.y)
                q = 3;
            else
                break;
        } else {
            break;
        }
        c = t.kids + q;
    }
    if (item.node == c) return;
    take(slot);
    item.node = c;
    nodes_[c].items.push_back(slot);
    if (nodes_[c].kids >= 0 || nodes_[c].items.size() < 5) return;
    int depth = 0;
    for (int a = nodes_[c].parent; a >= 0; a = nodes_[a].parent) depth++;
    if (depth > 8) return;
    // split into quarters and sort the items down
    int k = newKids();
    Node &t = nodes_[c];
    t.kids = k;
    float hw = t.w * 0.5f, hh = t.h * 0.5f, x = t.x, y = t.y;
    const float qx[4] = {x, hw + x, x, hw + x}, qy[4] = {y, y, hh + y, hh + y};
    for (int i = 0; i < 4; i++) {
        Node &q = nodes_[k + i];
        q = Node();
        q.parent = c;
        q.x = qx[i];
        q.y = qy[i];
        q.w = hw;
        q.h = hh;
    }
    std::vector<uint16_t> items;
    items.swap(nodes_[c].items);
    for (uint16_t s : items) {
        items_[s].node = -1;
        insert(c, s);
    }
}

// drops a node's quarters, and everything under them, out of the tree
void QuadTree::free(int node) {
    int k = nodes_[node].kids;
    if (k < 0) return;
    for (int i = 0; i < 4; i++) {
        for (uint16_t s : nodes_[k + i].items) items_[s].node = -1;
        free(k + i);
        nodes_[k + i].items.clear();
    }
    freeKids_.push_back(k);
    nodes_[node].kids = -1;
}

void QuadTree::merge(int node) {
    int k = nodes_[node].kids;
    if (k < 0) return;
    for (int i = 0; i < 4; i++) merge(k + i);
    size_t n = nodes_[node].items.size();
    for (int i = 0; i < 4; i++) {
        if (nodes_[k + i].kids >= 0) return;
        n += nodes_[k + i].items.size();
    }
    if (n > 4) return;
    for (int i = 0; i < 4; i++) {
        for (uint16_t s : nodes_[k + i].items) {
            nodes_[node].items.push_back(s);
            items_[s].node = node;
        }
        nodes_[k + i].items.clear();
    }
    free(node);
}

bool QuadTree::visit(int node, float x, float y, float w, float h, const std::function<bool(uint16_t)> &f) {
    size_t n = nodes_[node].items.size();
    for (size_t i = 0; i < n && i < nodes_[node].items.size(); i++) {
        uint16_t s = nodes_[node].items[i];
        const Box &b = items_[s].box;
        if (x > b.x + b.w || x + w < b.x || y > b.y + b.h || y + h < b.y) continue;
        if (f(s)) return true;
    }
    int k = nodes_[node].kids;
    if (k < 0) return false;
    float mx = nodes_[node].x + nodes_[node].w * 0.5f;
    if (x < mx) {
        float my = nodes_[node].y + nodes_[node].h * 0.5f;
        if (y < my && visit(k, x, y, w, h, f)) return true;
        my = nodes_[node].y + nodes_[node].h * 0.5f;
        if (y + h >= my && visit(k + 2, x, y, w, h, f)) return true;
    }
    mx = nodes_[node].x + nodes_[node].w * 0.5f;
    if (x + w >= mx) {
        float my = nodes_[node].y + nodes_[node].h * 0.5f;
        if (y < my && visit(k + 1, x, y, w, h, f)) return true;
        my = nodes_[node].y + nodes_[node].h * 0.5f;
        if (y + h >= my && visit(k + 3, x, y, w, h, f)) return true;
    }
    return false;
}

}  // namespace game
