# florrcustom

A from-scratch C++ reimplementation of both the old florr.io game server and client.

## Timeline

- **2025:** florr.io puts its old client up at https://static.florr.io/old/.
- **This project:** a new server in C++ (`server/`) and a new client in C++/Emscripten (`client/`) drawing with WebGL2. Both speak the old florr protocol. As far as I know, this is the first florrcustom.

## Building

uWebSockets is a submodule. After cloning, fetch it and its uSockets (its other submodules aren't needed):
```
git submodule update --init
git -C server/third_party/uWebSockets submodule update --init uSockets
```

**Server:**
```
cd server && ./build.sh
```
Needs CMake, Ninja, a C++17 compiler and zlib. The output is `server/build/server`.

**Client:**
```
cd client && ./build.sh
```
Needs the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) in `~/emsdk`, CMake and Ninja. The output goes to `client/dist`.

## Running

```
cd server && ./run.sh
```
The server also hosts the client, so open http://localhost:8098.

Environment variables:
- `PORT`: the port to listen on (8098 by default).
- `HOST`: the address to listen on (0.0.0.0 by default).
- `STATIC`: folders to serve the page from, in order, separated by `:` (`client/dist` by default).

## Docker

```
docker compose up -d --build
```
This builds the client and server into one image and runs it, with the game on port 8098 like running it by hand. `GAME_PORT` changes the port.

To serve it over HTTPS, put [Caddy](https://caddyserver.com) in front:
```
docker compose -f compose.yaml -f compose.caddy.yaml up -d --build
```
By default it serves https://localhost with Caddy's own certificate. Set `SITE_ADDRESS` to your domain to get a real one (ports 80 and 443 need to be reachable). `HTTP_PORT` and `HTTPS_PORT` change its ports.

Or play through a Cloudflare quick tunnel, without opening any ports. Cloudflare handles HTTPS, so this doesn't need Caddy:
```
docker compose -f compose.yaml -f compose.tunnel.yaml up -d --build
docker compose -f compose.yaml -f compose.tunnel.yaml logs tunnel | grep trycloudflare.com
```
The second command prints the address. It changes whenever the tunnel restarts.

Other commands like `logs` and `down` need the same `-f` files, so take a setup down with its files before switching to another. To skip typing them, put `COMPOSE_FILE=compose.yaml:compose.caddy.yaml` (or `compose.tunnel.yaml`, or all three) in a `.env` file next to `compose.yaml`, along with any of the settings above.

The game container holds only the static server binary and the client, and runs as a non-root user with no capabilities on a read-only filesystem. With Caddy or the tunnel it also sits on a network with no internet access and isn't published itself, so players reach it only through them. On its own it needs a published port, which Docker can't give a container on a network without internet access.

## Layout

- `shared`: what the client and server agree on: components, field numbers, message types, the packet encoding, and the catalog of petals and mobs.
- `server/src`: `game` is the game itself, one class per part (`Flowers`, `Petals`, `Mobs`, `Combat`, ...) with `Game` running the tick; `world` is the entities and what each client sees; `platform` is the entry point (HTTP and WebSocket).
- `client/src`: `sim` keeps the world up to date from the server, `net` is the link to the server, `game` draws the world and the UI, `cp6` is a Canvas2D-like drawing context on WebGL2.
- `client/web`: the page around the client.

## License

MIT.
