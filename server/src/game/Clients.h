// Who is connected: their messages, what each one can see, and the packets they get.
#pragma once
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

#include "Reader.h"
#include "game/Components.h"
#include "game/System.h"

namespace game {

class Clients : public System {
public:
    explicit Clients(Game &game);

    int connect();
    void disconnect(int client);
    void receive(int client, const uint8_t *b, size_t n, double now);
    void moveCameras();
    void waitToSpawn(double dt);
    void buildPackets();

    std::map<int, Client> all;  // in the order they joined
    std::unordered_map<int, std::vector<uint8_t>> packets;

private:
    void onTrash(Client &c, net::Reader &r, bool apply);
    void onSwap(Client &c, net::Reader &r, bool apply);
    void onInput(Client &c, net::Reader &r, bool apply);
    void handle(Client &c, uint32_t type, net::Reader &r, bool apply);
    std::vector<uint32_t> visibleTo(Client &c);
    void buildPacket(Client &c, std::vector<uint8_t> &out);

    int nextClient_ = 1;
};

}  // namespace game
