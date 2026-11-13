#!/bin/sh
# verktyg/skarmbild.sh ut.png "hem" – fönstret som bild, utan skärm (Xvfb). Körs i Debian 13-miljön.
# MATTE_VY: hem · kort:<id> · ova:<id> · svara:<text> · placering · oversikt · mork · lek  (flera med ;)
cd "$(dirname "$0")/.."
export MATTE_DATA="$PWD/share/se.wenuyos.app.matte" MATTE_SKARMBILD="$1" MATTE_VY="${2:-hem}"
export XDG_DATA_HOME="${XDG_DATA_HOME:-/tmp/matte-test-data}" LANG=sv_SE.UTF-8 GDK_BACKEND=x11 GSK_RENDERER=cairo
exec dbus-run-session -- xvfb-run -a -s "-screen 0 ${MATTE_SKARM:-1280x860}x24" ./build/matte
