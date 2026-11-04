// The quadtree everything with a hitbox lives in, by slot. Things that straddle a node's middle
// lines stay in it; the rest go down to the quarter they fit in once a node holds five. The order
// things are stored and visited in decides the order collisions are handled in, so it follows
// fixed rules: items are swapped out of a node's list, nodes split at five items unless nine deep,
// and quarters fold back into their parent once they and it hold four items or fewer.
#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace game {

class QuadTree {
public:
    struct Node {
        int parent = -1;
        float x = 0, y = 0, w = 0, h = 0;
        std::vector<uint16_t> items;  // slots
        int kids = -1;                // first of the four quarters in the node pool, or -1
    };
    struct Box {
        float x = 0, y = 0, w = 0, h = 0;
    };

    explicit QuadTree(size_t slots) : items_(slots) {}

    // the root's real size; everything in the tree comes out and has to go in again
    void setRoot(float x, float y, float w, float h);
    // an item's bounding square for this tick: only the first one set counts until newTick
    void setBox(uint16_t slot, float x, float y, float size);
    void newTick(uint16_t slot) { items_[slot].boxed = false; }
    const Box &box(uint16_t slot) const { return items_[slot].box; }

    // moves an item to the node its box belongs in
    void place(uint16_t slot);
    void remove(uint16_t slot);
    bool contains(uint16_t slot) const { return items_[slot].node >= 0; }
    // folds back the quarters that have emptied out
    void merge() { merge(0); }

    // Calls f(slot) for every item whose box overlaps the rectangle: a node's own items first, then
    // its quarters top-left, bottom-left, top-right, bottom-right. Stops as soon as f says so.
    // Items added on the way (by whatever f does) wait for the next visit.
    bool visit(float x, float y, float w, float h, const std::function<bool(uint16_t)> &f) {
        return visit(0, x, y, w, h, f);
    }

    const std::vector<Node> &nodes() const { return nodes_; }

private:
    struct Item {
        Box box;
        bool boxed = false;
        int node = -1;
    };

    int home(uint16_t slot) const;
    void insert(int node, uint16_t slot);
    void take(uint16_t slot);
    int newKids();
    void free(int node);
    void merge(int node);
    bool visit(int node, float x, float y, float w, float h, const std::function<bool(uint16_t)> &f);

    std::vector<Item> items_;
    std::vector<Node> nodes_ = std::vector<Node>(1);  // the root first
    std::vector<int> freeKids_;
};

}  // namespace game
