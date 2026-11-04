// The WebSocket link to the server: queues and resends messages, and hands each update to the
// simulation.
#pragma once
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "Writer.h"
#include "sim/Simulation.h"

namespace net {

// reliable server messages that arrive after the entity section:
// 11 = entity was hit (id), 14 = a card goes into inventory slot (id, slot), others carry text
struct Event {
    uint32_t type = 0;
    uint32_t id = 0;
    int slot = 0;
    std::string text;
};

class Conn {
public:
    void open();
    void close();
    bool isOpen() const { return open_; }
    // queues a message; it is resent with every packet until the server acks its sequence number
    void send(uint8_t type, const std::vector<uint8_t> &payload = {});
    // call once per frame: sends pending messages (at most every 30 ms) and applies replies
    void frame(sim::Simulation &sim, double now);
    bool gotUpdate() const { return updates_ > 0; }
    // true once after the server went away (everything it had told us is gone)
    bool dropped() {
        bool d = dropped_;
        dropped_ = false;
        return d;
    }
    int updates() const { return updates_; }
    std::vector<Event> events;

private:
    struct Message {
        uint32_t seq;
        std::vector<uint8_t> bytes;
    };

    void connect();
    void apply(sim::Simulation &sim, const uint8_t *b, size_t n, double now);

    std::deque<Message> pending_;
    uint32_t seq_ = 0;
    bool open_ = false;
    bool dropped_ = false;
    double retryAt_ = 0;
    double lastSend_ = 0;
    int updates_ = 0;
    std::vector<uint8_t> in_;
};

}  // namespace net
