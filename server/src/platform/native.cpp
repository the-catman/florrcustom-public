// One shared game for every WebSocket connection, and the page itself over HTTP on the same port.
//   PORT    default 8098
//   HOST    default 0.0.0.0
//   STATIC  folders the page's files are looked up in, in order, separated by ':'
//           (default: the client's dist folder)
#include "App.h"

#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "game/Game.h"

namespace {

struct Socket {
    int client = -1;
};
using WS = uWS::WebSocket<false, true, Socket>;

std::unique_ptr<game::Game> g_game;
std::unordered_set<WS *> g_sockets;
std::vector<std::string> g_roots;
std::random_device g_rd;

double nowMs() {
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

std::string env(const char *name, const std::string &fallback) {
    const char *v = std::getenv(name);
    return v && *v ? v : fallback;
}

std::string exeDir() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return ".";
    std::string p(buf, size_t(n));
    return p.substr(0, p.find_last_of('/'));
}

bool readFile(const std::string &path, std::string &out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

const char *mimeFor(const std::string &path) {
    auto ends = [&](const char *e) {
        size_t n = std::strlen(e);
        return path.size() >= n && path.compare(path.size() - n, n, e) == 0;
    };
    if (ends(".html")) return "text/html; charset=utf-8";
    if (ends(".js")) return "application/javascript; charset=utf-8";
    if (ends(".wasm")) return "application/wasm";
    if (ends(".png")) return "image/png";
    if (ends(".ico")) return "image/x-icon";
    return "application/octet-stream";
}

void tick(us_timer_t *) {
    g_game->tick(nowMs());
    for (WS *ws : g_sockets) {
        const auto &p = g_game->packet(ws->getUserData()->client);
        ws->send(std::string_view(reinterpret_cast<const char *>(p.data()), p.size()), uWS::OpCode::BINARY);
    }
}

}  // namespace

int main() {
    int port = std::atoi(env("PORT", "8098").c_str());
    std::string host = env("HOST", "0.0.0.0");
    std::string here = exeDir();
    std::stringstream roots(env("STATIC", here + "/../../client/dist"));
    for (std::string r; std::getline(roots, r, ':');)
        if (!r.empty()) g_roots.push_back(r);

    g_game = std::make_unique<game::Game>(g_rd(), nowMs());
    g_game->setEntropy([] { return uint32_t(g_rd()); });
    g_game->tankMode = env("MODE", "tank") != "flower";
    g_game->devKeys = env("DEV_KEYS", "") == "1";

    uWS::App app;
    app.ws<Socket>("/*", {
                             .compression = uWS::DISABLED,
                             .maxPayloadLength = 65536,
                             .idleTimeout = 10,
                             .maxBackpressure = 4 * 1024 * 1024,
                             .closeOnBackpressureLimit = true,
                             .open =
                                 [](WS *ws) {
                                     ws->getUserData()->client = g_game->connect();
                                     g_sockets.insert(ws);
                                 },
                             .message =
                                 [](WS *ws, std::string_view m, uWS::OpCode op) {
                                     if (op != uWS::OpCode::BINARY) return;
                                     g_game->receive(ws->getUserData()->client,
                                                     reinterpret_cast<const uint8_t *>(m.data()), m.size(), nowMs());
                                 },
                             .close =
                                 [](WS *ws, int, std::string_view) {
                                     g_sockets.erase(ws);
                                     g_game->disconnect(ws->getUserData()->client);
                                 },
                         });

    app.get("/*", [](auto *res, auto *req) {
        std::string url(req->getUrl());
        if (url == "/") url = "/index.html";
        std::string body;
        if (url.find("..") == std::string::npos)
            for (const std::string &r : g_roots)
                if (readFile(r + url, body)) {
                    res->writeHeader("Content-Type", mimeFor(url));
                    res->writeHeader("Cache-Control", "no-cache");
                    res->end(body);
                    return;
                }
        res->writeStatus("404 Not Found")->end();
    });

    us_timer_t *timer = us_create_timer(reinterpret_cast<us_loop_t *>(uWS::Loop::get()), 0, 0);
    us_timer_set(timer, tick, 50, 50);

    app.listen(host, port, [&](auto *token) {
        if (token)
            std::printf("serving on http://%s:%d/\n", host == "0.0.0.0" ? "localhost" : host.c_str(), port);
        else
            std::printf("couldn't listen on %s:%d\n", host.c_str(), port);
        std::fflush(stdout);
    });
    app.run();
    return 1;
}
