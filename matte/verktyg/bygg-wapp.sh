#!/bin/sh
# verktyg/bygg-wapp.sh [ut-mapp] – bygger Matte i Debian 13 (amd64) och packar matte-<version>.wapp.
# Kräver: cmake g++ pkg-config libadwaita-1-dev libsqlite3-dev liblua5.4-dev libsodium-dev python3
# och WenuyOS installationsmodul (WENUYOS_PY = mappen med wenuyos_install.py).
set -e
cd "$(dirname "$0")/.."
UT=$(realpath "${1:-..}")
V=$(sed -n 's/^version = "\(.*\)"/\1/p' wenuyos-app.toml)
WENUYOS_PY=${WENUYOS_PY:-/usr/lib/wenuyos/python}

python3 verktyg/expandera.py innehall share/se.wenuyos.app.matte    # kontrollerar innehållet först
python3 verktyg/testfall.py innehall tester/fall.json
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
cmake --build build -j"$(nproc)"
cmake --build build --target matte-test -j"$(nproc)" >/dev/null
./build/matte-test tester share/se.wenuyos.app.matte

STAGE=build/stage/matte
rm -rf build/stage && mkdir -p "$STAGE"
cmake --install build --prefix "$PWD/$STAGE" >/dev/null
strip --strip-debug "$STAGE/bin/matte"          # funktionsnamnen kvar för wenuyos prestanda
cp -r data wenuyos-app.toml README.md "$STAGE/"
cp /usr/share/common-licenses/GPL-3 "$STAGE/LICENSE"

python3 - "$STAGE" "$UT/matte-$V.wapp" "$WENUYOS_PY" <<'PY'
import sys
sys.path.insert(0, sys.argv[3])
import wenuyos_install as w
w.pack(sys.argv[1], out=sys.argv[2], log=lambda *a: None)
print("äkthet:", w.authenticity(sys.argv[2]))
print("innehåll:", w.scan_archive(sys.argv[2]))
PY
ls -la "$UT/matte-$V.wapp"
