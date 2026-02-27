#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"

cmake -S . -B "$BUILD_DIR" -DUSE_FETCHCONTENT_RAYLIB=OFF
cmake --build "$BUILD_DIR" -j
"./$BUILD_DIR/submarine_noir"
