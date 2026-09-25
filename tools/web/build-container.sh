#!/usr/bin/env bash
set -euo pipefail

# Run inside emscripten/emsdk:6.0.10 with /src read-only and /work a volume.
# Keep compilation on the Linux volume; Windows bind-mount stat calls are slow.
mkdir -p /work/source
tar -C /src --exclude=.git --exclude=build --exclude=node_modules -cf - . |
    tar -C /work/source -xf -
emcmake cmake -S /work/source -B /work/build-wasm \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_ENGINE=OFF -DBUILD_TESTS=OFF
cmake --build /work/build-wasm --target DarkEden web_sprite_tests transport_tests -j "${WEB_BUILD_JOBS:-8}"
