#!/bin/bash
# Builds the server into build/server.
set -e
cd "$(dirname "$0")"
# configure again each time so new source files are picked up
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release > /dev/null
ninja -C build server
