#!/bin/bash
# Builds the IBus engine and the GTK settings app, runs the engine regression
# tests, and packages everything as a .deb in dist/.
#   sudo apt install build-essential cmake pkg-config libibus-1.0-dev libadwaita-1-dev
set -euo pipefail
cd "$(dirname "$0")/.."
jobs="${JOBS:-4}"

test_dir="$(mktemp -d "${TMPDIR:-/tmp}/omnikey-engine-tests.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
g++ -std=c++17 -DLINUX -Wno-deprecated-declarations -I Sources/Omnikey/engine \
  Sources/Omnikey/engine/*.cpp tests/engine_regression.cpp -o "$test_dir/engine-tests"
"$test_dir/engine-tests"

cmake -S Sources/Omnikey/linux -B build/linux -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j "$jobs"
(cd build/linux && cpack -G DEB -B "$PWD/../../dist" >/dev/null)
rm -rf dist/_CPack_Packages
ls -1 dist/*.deb
