#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/omnikey-mac-tests.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
xcrun clang -g -fobjc-arc -fobjc-weak -fsanitize=address,undefined -I Sources/Omnikey/macOS/ModernKey \
  -c Sources/Omnikey/macOS/ModernKey/OKCandidatePanel.m -o "$test_dir/OKCandidatePanel.o"
xcrun clang++ -std=c++14 -g -fobjc-arc -fobjc-weak -fsanitize=address,undefined \
  -I Sources/Omnikey/engine -I Sources/Omnikey/macOS/ModernKey \
  Sources/Omnikey/engine/*.cpp Sources/Omnikey/macOS/ModernKey/OKRime.mm \
  "$test_dir/OKCandidatePanel.o" tests/mac_event_tests.mm \
  -framework Cocoa -framework Carbon -o "$test_dir/event-tests"
# Chinese-mode typing runs when scripts/fetch-rime.sh has prepared Rime.
rime="$PWD/Sources/Omnikey/macOS/ThirdParty/Rime"
if [ -f "$rime/.stamp" ]; then export OMNIKEY_RIME="$rime"; fi
"$test_dir/event-tests" 2>"$test_dir/stderr" || { cat "$test_dir/stderr"; exit 1; }
xcrun clang -g -fobjc-arc -fobjc-weak -fsanitize=address,undefined \
  -I Sources/Omnikey/macOS/ModernKey tests/mac_tap_tests.m \
  -framework Cocoa -o "$test_dir/tap-tests"
"$test_dir/tap-tests"
