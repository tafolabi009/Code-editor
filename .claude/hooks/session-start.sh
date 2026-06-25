#!/bin/bash
#
# SessionStart hook for Claude Code on the web.
#
# Installs the system build dependencies and pre-configures a build directory so
# that `cmake --build build` and `ctest` work immediately in a fresh remote
# session. Idempotent and non-interactive; safe to re-run.
set -euo pipefail

# Only run in remote (Claude Code on the web) sessions; local machines are
# expected to already have a toolchain.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
    exit 0
fi

PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"

export DEBIAN_FRONTEND=noninteractive

# Refresh the package index first (the base image's index can be stale).
sudo apt-get update -y

# Required to build the core library, tests and the SIMD assembly.
sudo apt-get install -y --no-install-recommends \
    cmake ninja-build g++ nasm \
    nlohmann-json3-dev libgtest-dev

# Optional, only needed for the GUI build (BUILD_GUI=ON / USE_SYSTEM_IMGUI).
# Best-effort: don't fail the whole session if a GUI package is unavailable.
sudo apt-get install -y --no-install-recommends \
    libglfw3-dev libgl1-mesa-dev libimgui-dev libstb-dev || \
    echo "session-start: GUI dependencies unavailable; core + tests will still build."

# Pre-configure and build the core library + tests (ASM enabled since NASM is
# present). This warms the cached container so the first test run is fast.
# If a stale cache from a different source path exists, wipe and reconfigure.
configure() {
    cmake -S "$PROJECT_DIR" -B "$PROJECT_DIR/build" -G Ninja \
        -DBUILD_GUI=OFF -DBUILD_TESTS=ON -DBUILD_BENCHMARKS=OFF -DENABLE_ASM=ON
}
if ! configure; then
    echo "session-start: stale build cache detected, reconfiguring cleanly."
    rm -rf "$PROJECT_DIR/build"
    configure
fi
cmake --build "$PROJECT_DIR/build"

echo "session-start: dependencies installed and build/ configured."
