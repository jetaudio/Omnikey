#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
bash scripts/fetch-rime.sh
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/omnikey-rime-tests.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
xcrun clang++ -std=c++14 -fobjc-arc -g -I Sources/Omnikey/macOS/ModernKey \
  Sources/Omnikey/macOS/ModernKey/OKRime.mm tests/rime_test.mm \
  -framework Cocoa -framework Carbon -o "$test_dir/rime-tests"
"$test_dir/rime-tests" "$PWD/Sources/Omnikey/macOS/ThirdParty/Rime" 2>"$test_dir/stderr" || { cat "$test_dir/stderr"; exit 1; }
