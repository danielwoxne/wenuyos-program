#!/usr/bin/env python3
"""Konsoler och hemdatorer: Nintendo 64, Nintendo DS, GameCube/Wii och PSP (docs/planer/KOMPATIBILITET.md steg 5 i
New-wenuyos) – filtyper, mappar, kommandon, installation (Debian/Flathub). Flyttat från tools/check/test-fler-spel.py."""
import os, pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import kärna  # noqa: E402

R, K = kärna.ladda()
n = 0
def ok(c, msg):
    global n
    if not c:
        print("FEL:", msg); sys.exit(1)
    n += 1

ok(R.system_of("/tmp/x/Mario.z64") == "N64" and R.system_of("/tmp/x/a.n64") == "N64" and R.system_of("/tmp/x/a.nds") == "NintendoDS", "N64 och DS på filtypen")
ok(R.system_of("/tmp/x/a.rvz") == "GameCube" and R.system_of("/tmp/x/a.cso") == "PSP", "GameCube/Wii (.rvz) och PSP (.cso)")
ok(R.system_of("/tmp/x/skiva.iso") is None, "en .iso utanför Retro gissas inte (kan vara vad som helst)")
ok(R.system_of(os.path.join(R.RETRO, "PSP", "Spel", "a.iso")) == "PSP" and R.system_of(os.path.join(R.RETRO, "GameCube", "a.iso")) == "GameCube",
   ".iso i systemets mapp: mappen avgör")
for s in ("N64", "NintendoDS", "GameCube", "PSP"):
    ok(s in R.ORDER and s in R.SYSTEM_NAME and s in R.SYSTEM_ICON, f"{s}: visas i Retrospel")
orig = K.emulator_for
K.emulator_for = lambda s: ("X", ["emu"], None)
ok(K.command({"system": "N64", "files": ["/a.z64"], "id": "/a.z64"}, {}) == ["emu", "--windowed", "/a.z64"], "N64: i ett fönster")
ok(K.command({"system": "N64", "files": ["/a.z64"], "id": "/a.z64"}, {"console_fullscreen": True}) == ["emu", "--fullscreen", "/a.z64"], "N64: helskärm när den är vald")
ok(K.command({"system": "NintendoDS", "files": ["/a.nds"], "id": "/a.nds"}, {}) == ["emu", "/a.nds"], "DS: DeSmuME med spelet")
ok(K.command({"system": "GameCube", "files": ["/a.rvz"], "id": "/a.rvz"}, {}) == ["emu", "-b", "-e", "/a.rvz"], "GameCube: Dolphin startar spelet direkt")
ok(K.command({"system": "PSP", "files": ["/a.cso"], "id": "/a.cso"}, {}) == ["emu", "@@", "/a.cso", "@@"], "PSP: filen lämnas in i sandlådan (Flatpak)")
K.emulator_for = orig
K.flatpak_has = lambda a: False
name, pre, inst = K.emulator_for("PSP")
ok(name == "PPSSPP" and pre is None and inst == ["flatpak", "install", "-y", "--noninteractive", "flathub", "org.ppsspp.PPSSPP"], "PSP saknas: PPSSPP från Flathub")
ok(K.EMULATORS["N64"][2] == "mupen64plus-ui-console" and K.EMULATORS["NintendoDS"][2] == "desmume" and K.EMULATORS["GameCube"][2] == "dolphin-emu",
   "N64, DS, GameCube: Debian-paket")
ok(R.HEAVY == {"GameCube", "PSP"} == K.HEAVY, "GameCube och PSP märks som tunga på 2 GB")
ok("tunga" in "".join(K.describe("PSP", ["emu"])[0] or "").lower() or not R.low_profile(), "Retrospel säger i förväg att de tunga systemen kan gå långsamt på 2 GB")

print(f"Konsoler: N64, DS, GameCube, PSP – {n} prov godkända")
