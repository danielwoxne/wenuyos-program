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
for spec in "1024x600 hem" "1024x600 ova:brak-delar;lattlast" "1280x720 ova:ekvationer-tva-steg" "1600x900 karta" "1600x900 ova:brak-procent-decimal" "1600x900 ova:gy-andragradsfunktioner"; do
    size=${spec%% *}
    view=${spec#* }
    case "$view" in
        hem) file="small-home.png";;
        ova:brak-delar*) file="small-easy-reading.png";;
        ova:ekvationer-tva-steg*) file="mid-secondary-school.png";;
        ova:gy-andragradsfunktioner*) file="large-upper-secondary.png";;
        karta) file="large-map.png";;
        *) file="large-fractions.png";;
    esac
    # Bekräfta nivåklassen inne i GTK, inte enbart att PNG:n kunde sparas.
    case "$view" in
        ova:ekvationer-tva-steg*) export MATTE_TEST_VISUELL=matte-visuell-akademisk MATTE_TEST_KORT_REGEL=1;;
        ova:gy-andragradsfunktioner*) export MATTE_TEST_VISUELL=matte-visuell-professionell MATTE_TEST_KORT_REGEL=1;;
        *) unset MATTE_TEST_VISUELL MATTE_TEST_KORT_REGEL;;
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
# Äkta återställningsprov över TVÅ GTK-processer, på syntetisk elev i XDG-temp.
# Första starten skriver på papperet; den andra måste återställa samma försök.
unset MATTE_TEST_VISUELL MATTE_TEST_KORT_REGEL
export MATTE_STORLEK=1024x600
export MATTE_VY='ova:brak-delar;kladd:48|+27'
export MATTE_SKARMBILD="$out/paper-before-restart.png"
timeout 35s dbus-run-session -- xvfb-run -a -s '-screen 0 1024x600x24' ./build/matte
python3 - "$XDG_DATA_HOME/se.wenuyos.app.matte" <<'PY'
import json, pathlib, sys
root = pathlib.Path(sys.argv[1])
papers = list((root / 'raknepapper').glob('*/*.json'))
if len(papers) != 1:
    raise SystemExit(f'Expected exactly one saved paper, got {len(papers)}')
p = json.loads(papers[0].read_text(encoding='utf-8'))
if p.get('format') != 'se.wenuyos.matte.raknepapper' or len(p.get('rutor', [])) < 2:
    raise SystemExit('Working paper was not written after input')
profile = json.loads((root / 'elev.json').read_text(encoding='utf-8'))
if profile.get('papper_aktivt', {}).get('id') != p.get('id'):
    raise SystemExit('Saved pupil profile does not refer to paper')
print('OK first session: paper and active task saved locally')
PY
export MATTE_VY='kort:brak-delar' MATTE_TEST_PAPPER_RESTORE=1
export MATTE_SKARMBILD="$out/paper-after-restart.png"
timeout 35s dbus-run-session -- xvfb-run -a -s '-screen 0 1024x600x24' ./build/matte
unset MATTE_TEST_PAPPER_RESTORE
echo "Six course screenshots plus two actual autosave/restart screenshots were produced in $out"
