#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/omnikey-engine-tests.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
xcrun clang++ -std=c++14 -g -fsanitize=address,undefined \
  -I Sources/Omnikey/engine Sources/Omnikey/engine/*.cpp tests/engine_regression.cpp \
  -o "$test_dir/engine-tests"
"$test_dir/engine-tests"
