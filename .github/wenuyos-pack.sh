#!/bin/bash
# Packa ett program i det gemensamma programförrådet som en .wapp – samma som `wenuyos app pack` på en WenuyOS-dator,
# men utan resten av WenuyOS (körs i CI med WenuyOS SDK-arkivet: wenuyos-get, libwenuy, wenuyos-verify).
#
#   .github/wenuyos-pack.sh <programmapp> <ut.wapp> [--sign]
#
# Lua-spel (game.ini) och Python/JS-program (main.py/main.js + data/*.desktop) packas som de är.
# C++ (CMakeLists.txt) byggs och installeras i en tom mapp; Rust (Cargo.toml) byggs; bara programfilen och data/ följer med.
# --sign: signera med programnyckeln (gpg; i CI importerad från hemligheten WENUYOS_PROGRAM_KEY).
set -euo pipefail
dir=$(cd "$1" && pwd); out=$2; shift 2
GET=${WENUYOS_GET:-/usr/lib/wenuyos/wenuyos-get}
mkdir -p "$(dirname "$out")"
if [ -f "$dir/game.ini" ] || { { [ -f "$dir/main.py" ] || [ -f "$dir/main.js" ]; } && ls "$dir"/data/*.desktop >/dev/null 2>&1; }; then
  exec python3 "$GET" --pack "$dir" -o "$out" "$@"
fi
st=$(mktemp -d); trap 'rm -rf "$st"' EXIT
if [ -f "$dir/Cargo.toml" ]; then
  n=$(sed -n 's/^name *= *"\(.*\)"/\1/p' "$dir/Cargo.toml" | head -1)
  (cd "$dir" && cargo build --release)
  install -Dm755 "$dir/target/release/$n" "$st/bin/$n"
elif [ -f "$dir/CMakeLists.txt" ]; then
  n=$(sed -n 's/^project(\([A-Za-z0-9_.-]*\).*/\1/p' "$dir/CMakeLists.txt" | head -1)
  cmake -S "$dir" -B "$dir/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build "$dir/build" -j"$(nproc)"
  cmake --install "$dir/build" --prefix "$st" >/dev/null
else
  echo "Inget program i $dir (game.ini, main.py/main.js + data/*.desktop, Cargo.toml eller CMakeLists.txt)" >&2; exit 1
fi
cp -r "$dir/data" "$st/" 2>/dev/null || true
cp "$dir"/LICENSE* "$dir"/README* "$st/" 2>/dev/null || true
python3 "$GET" --pack "$dir" --staged "$st" --exec "bin/$n" -o "$out" "$@"
