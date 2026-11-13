#!/bin/sh
# Matte screenshot smoke test, separate from verify's low-memory window start.
# Requires Debian 13 packages: xvfb, xauth, dbus-x11, GTK4, libadwaita.
set -eu
cd "$(dirname "$0")/.."
command -v xvfb-run >/dev/null || { echo "Missing xvfb-run" >&2; exit 2; }
command -v dbus-run-session >/dev/null || { echo "Missing dbus-run-session" >&2; exit 2; }
test -x build/matte || { echo "Build Matte before running the screenshot test" >&2; exit 2; }
data="$PWD/build/matte-share/se.wenuyos.app.matte"
test -f "$data/kort.json" || { echo "Generated lesson data missing" >&2; exit 2; }
out="${1:-$PWD/build/matte-soft-images}"
mkdir -p "$out"
out=$(cd "$out" && pwd)
# Use a disposable XDG directory so that no real student profile is touched.
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
export XDG_DATA_HOME="$tmp/data"
export XDG_CONFIG_HOME="$tmp/config"
export MATTE_DATA="$data" WENUYOS_PERF=low WENUYOS_GFX=soft
export GDK_BACKEND=x11 GSK_RENDERER=cairo
export LIBGL_ALWAYS_SOFTWARE=1
# GSK_RENDERER=cairo och LIBGL_ALWAYS_SOFTWARE=1 räcker i Debian 13.
# GDK_DEBUG=gl-disable stöds inte i GTK 4.18 och ger ett falskt varningsmeddelande.
for spec in "1024x600 hem" "1024x600 ova:brak-delar;lattlast" "1600x900 karta" "1600x900 ova:brak-procent-decimal"; do
    size=${spec%% *}
    view=${spec#* }
    case "$view" in
        hem) file="small-home.png";;
        ova:brak-delar*) file="small-easy-reading.png";;
        karta) file="large-map.png";;
        *) file="large-fractions.png";;
    esac
    export MATTE_STORLEK="$size" MATTE_VY="$view" MATTE_SKARMBILD="$out/$file"
    timeout 35s dbus-run-session -- xvfb-run -a -s "-screen 0 ${size}x24" ./build/matte
    python3 - "$out/$file" <<'PY'
import pathlib, struct, sys
file = pathlib.Path(sys.argv[1])
data = file.read_bytes()
if data[:8] != b'\x89PNG\r\n\x1a\n' or len(data) < 2048:
    raise SystemExit(f"Invalid or empty image: {file}")
w, h = struct.unpack(">II", data[16:24])
if min(w, h) < 300:
    raise SystemExit(f"Incorrect dimensions: {w}x{h}: {file}")
print(f"OK screenshot: {file.name} ({w}x{h}, {len(data)} bytes)")
PY
done
echo "Four Cairo software-rendered screenshots were produced in $out"
