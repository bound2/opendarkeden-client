#!/usr/bin/env bash
# Build the effect viewer with ASan, then run it from the game data directory.
# For a fresh Windows tree, set CMAKE_TOOLCHAIN_FILE as described in README.md.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build/debug-asan}"
DATA_DIR="${DARKEDEN_DIR:-$SCRIPT_DIR/DarkEden}"
JOBS="${NPROCS:-${NUMBER_OF_PROCESSORS:-}}"
if [ -z "$JOBS" ]; then
    JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)"
fi

# Reconfigure reused trees too, so USE_ASAN is never silently left off.
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
cmake --build "$BUILD_DIR" --config Debug --target effect_viewer --parallel "$JOBS"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"

if [ ! -d "$DATA_DIR/Data/Image" ]; then
    echo "Game images not found at: $DATA_DIR/Data/Image" >&2
    echo "Set DARKEDEN_DIR to the directory containing Data/." >&2
    exit 1
fi

# Visual Studio puts executables under the selected configuration; Ninja does not.
for VIEWER in "$BUILD_DIR/bin/Debug/effect_viewer.exe" \
              "$BUILD_DIR/bin/Debug/effect_viewer" \
              "$BUILD_DIR/bin/effect_viewer.exe" \
              "$BUILD_DIR/bin/effect_viewer"; do
    if [ -f "$VIEWER" ]; then
        cd "$DATA_DIR"
        exec "$VIEWER" "$@"
    fi
done
echo "effect_viewer executable not found under $BUILD_DIR/bin" >&2
exit 1
