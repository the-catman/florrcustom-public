#!/bin/bash
cd "$(dirname "$0")"
source ~/emsdk/emsdk_env.sh > /dev/null 2>&1
emcmake cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release > /dev/null
ninja -C build client
