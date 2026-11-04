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

## Layout

- `shared`: what the client and server agree on: components, field numbers, message types, the packet encoding, and the catalog of petals and mobs.
- `server/src`: `game` is the game itself, one class per part (`Flowers`, `Petals`, `Mobs`, `Combat`, ...) with `Game` running the tick; `world` is the entities and what each client sees; `platform` is the entry point (HTTP and WebSocket).
- `client/src`: `sim` keeps the world up to date from the server, `net` is the link to the server, `game` draws the world and the UI, `cp6` is a Canvas2D-like drawing context on WebGL2.
- `client/web`: the page around the client.

## License

MIT.
