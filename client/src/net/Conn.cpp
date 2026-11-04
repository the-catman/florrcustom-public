#include "net/Conn.h"

#include <cstdio>

#include "Cipher.h"
#include "Reader.h"

extern "C" {
void net_open();
void net_send(const void *data, int len);
int net_state();
int net_next(void *buf, int cap);
int ui_user_agent(char *buf, int cap);
int ui_language(char *buf, int cap);
}

namespace net {

namespace {

constexpr int kSocketClosed = 2;

void reportBadUpdate(const std::vector<uint8_t> &body) {
    static int reports = 0;
    if (reports++ >= 5) return;
    std::string hex;
    char t[4];
    for (uint8_t b : body) {
        std::snprintf(t, sizeof t, "%02x", b);
        hex += t;
    }
    std::printf("update parse failed: %s\n", hex.c_str());
}

}  // namespace

void Conn::open() {
    connect();
}

void Conn::connect() {
    net_open();
    open_ = true;
    seq_ = 0;
    pending_.clear();
    updates_ = 0;
    char userAgent[512], language[64];
    ui_user_agent(userAgent, sizeof userAgent);
    ui_language(language, sizeof language);
    Writer w;
    w.u8(1);
    w.str(userAgent);
    w.u8(0);
    w.str(language);
    send(protocol::MSG_HELLO, w.b);
}

void Conn::close() {
    open_ = false;
}

void Conn::send(uint8_t type, const std::vector<uint8_t> &payload) {
    Writer w;
    w.varint(++seq_);
    w.u8(type);
    w.bytes(payload.data(), payload.size());
    pending_.push_back({seq_, std::move(w.b)});
}

void Conn::frame(sim::Simulation &sim, double now) {
    if (!open_) return;
    events.clear();
    // a lost server: start over with an empty world, and try again every second
    if (net_state() == kSocketClosed) {
        if (updates_ || !retryAt_) {
            sim.clear();
            updates_ = 0;
            dropped_ = true;
            retryAt_ = now + 1000;
        }
        if (now >= retryAt_) {
            retryAt_ = now + 1000;
            connect();
        }
        return;
    }
    if (!pending_.empty() && now - lastSend_ >= 30) {
        std::vector<uint8_t> packet;
        for (const Message &m : pending_) packet.insert(packet.end(), m.bytes.begin(), m.bytes.end());
        cipher(packet.data(), packet.size());
        net_send(packet.data(), int(packet.size()));
        lastSend_ = now;
    }
    if (in_.size() < 65536) in_.resize(65536);
    while (true) {
        int n = net_next(in_.data(), int(in_.size()));
        if (n == -1) break;
        if (n < 0) {
            in_.resize(size_t(-n) * 2);
            continue;
        }
        apply(sim, in_.data(), size_t(n), now);
    }
}

void Conn::apply(sim::Simulation &sim, const uint8_t *b, size_t n, double now) {
    if (n < 2 || b[0] != protocol::kPacketUpdate) return;
    Reader header(b, n);
    header.skip(1);
    header.varint();
    std::vector<uint8_t> body(b + header.pos(), b + n);
    cipher(body.data(), body.size());

    Reader r(body.data(), body.size());
    uint32_t ack = r.varint();
    while (!pending_.empty() && pending_.front().seq <= ack) pending_.pop_front();
    if (!sim.decodeUpdate(r, now)) {
        reportBadUpdate(body);
        return;
    }
    updates_++;
    r.skip(1);
    // reliable messages: [varint k][varint type][payload] until k is 0
    while (!r.bad()) {
        if (!r.varint() || r.bad()) break;
        Event event;
        event.type = r.varint();
        if (event.type == protocol::EVENT_HIT) {
            event.id = r.ref();
        } else if (event.type == protocol::EVENT_PICKUP) {
            event.id = r.ref();
            event.slot = int(r.u8());
        } else if (event.type == 9 || event.type < 2 || event.type >= 15) {
            event.text = r.str();
        } else {
            break;  // the client has no handler for these
        }
        if (r.bad()) break;
        events.push_back(std::move(event));
    }
}

}  // namespace net
