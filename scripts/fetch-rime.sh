#!/bin/bash
# Download librime and the Rime Pinyin data at pinned versions, verify them and
# precompile the schema. Output (git-ignored): Sources/Omnikey/macOS/ThirdParty/Rime
#   lib/librime.1.dylib   universal librime, embedded in Omnikey.app/Contents/Frameworks
#   shared/               Rime configuration read at run time
#   build/                prebuilt dictionaries, so the first launch needs no deployment
#   licenses/             license texts of the bundled components
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
out_dir="$repo_dir/Sources/Omnikey/macOS/ThirdParty/Rime"

LIBRIME_TAG=1.17.0
LIBRIME_ASSET=rime-33e7814-macOS-universal.tar.bz2
LIBRIME_SHA256=11d8dc663c6ec06d5ccb6111ba664a9e7b631b703ac6acd07cffbac664021850
DATA=(
  "rime-prelude 082425ea0684bca36474415d4a0e8db9b016487e 66239ba4745d54471e5ce5540b45e86ec663ff5dba004cb23c95531a3bdec00f"
  "rime-essay 054920de4f54c9e5994276a96a4fc2a35cb51aa3 3339b40aae91b0216d393aeccc6a8f7daad56a1f3a5e1ffcf464c80690d1b6ce"
  "rime-pinyin-simp 0c6861ef7420ee780270ca6d993d18d4101049d0 46f37114a7929ecc01003a236803c8b1e5198382e6a21f83fae036604a6b08bf"
)
config="$repo_dir/Sources/Omnikey/macOS/Rime/default.custom.yaml"
LAYOUT=2  # bump when the output layout changes
stamp="$LAYOUT $LIBRIME_SHA256 ${DATA[*]} $(shasum -a 256 "$config" | cut -d' ' -f1)"
if [ -f "$out_dir/.stamp" ] && [ "$(cat "$out_dir/.stamp")" = "$stamp" ]; then
  exit 0
fi

work="$(mktemp -d "${TMPDIR:-/tmp}/omnikey-rime.XXXXXX")"
trap 'rm -rf "$work"' EXIT
fetch() { # url sha256 output
  curl -fsSL --retry 3 -o "$3" "$1"
  echo "$2  $3" | shasum -a 256 -c - >/dev/null
}

fetch "https://github.com/rime/librime/releases/download/$LIBRIME_TAG/$LIBRIME_ASSET" "$LIBRIME_SHA256" "$work/librime.tar.bz2"
tar -xjf "$work/librime.tar.bz2" -C "$work"
mkdir -p "$work/source" "$work/user" "$work/build" "$work/licenses"
for item in "${DATA[@]}"; do
  read -r name commit sha <<<"$item"
  fetch "https://codeload.github.com/rime/$name/tar.gz/$commit" "$sha" "$work/$name.tar.gz"
  tar -xzf "$work/$name.tar.gz" -C "$work"
  find "$work/$name-$commit" -maxdepth 1 \( -name '*.yaml' -o -name '*.txt' \) -exec cp {} "$work/source/" \;
  cp "$work/$name-$commit/LICENSE" "$work/licenses/$name-LICENSE.txt"
done
cp "$config" "$work/source/default.custom.yaml"
curl -fsSL --retry 3 -o "$work/licenses/librime-LICENSE.txt" \
  "https://raw.githubusercontent.com/rime/librime/$LIBRIME_TAG/LICENSE"

DYLD_LIBRARY_PATH="$work/dist/lib" "$work/dist/bin/rime_deployer" --build "$work/user" "$work/source" "$work/build" >/dev/null 2>&1
test -f "$work/build/pinyin_simp.table.bin"

rm -rf "$out_dir"
mkdir -p "$out_dir/lib" "$out_dir/shared"
cp "$work/dist/lib/librime.1.dylib" "$out_dir/lib/"
# The dictionary source lets Rime confirm the prebuilt table is current.
for file in default.yaml default.custom.yaml key_bindings.yaml punctuation.yaml symbols.yaml \
            pinyin_simp.schema.yaml pinyin_simp.dict.yaml; do
  cp "$work/source/$file" "$out_dir/shared/"
done
cp -R "$work/build" "$out_dir/build"
cp -R "$work/licenses" "$out_dir/licenses"
echo "$stamp" > "$out_dir/.stamp"
echo "Rime $LIBRIME_TAG ready in $out_dir"
