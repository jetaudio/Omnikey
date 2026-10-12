#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/omnikey-updater-tests.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
xcrun clang -fobjc-arc -Wno-deprecated-declarations \
  tests/mac_updater_validation_tests.m -framework Cocoa -o "$test_dir/validation"
"$test_dir/validation"
python3 tests/mac_updater_tests.py
