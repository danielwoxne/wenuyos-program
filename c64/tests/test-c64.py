#!/usr/bin/env python3
"""Commodore 64 som eget program (Daniel 19 nov 2026): tillägget i Retrospel och fönstrets logik.
Påhittade ROM-filer och spel i en tillfällig hemmapp – inga riktiga ROM:ar, ingen VICE behövs."""
import importlib.machinery, importlib.util, os, pathlib, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import kärna  # noqa: E402

R, K = kärna.ladda()
ROOT = pathlib.Path(__file__).resolve().parents[1]
fails, total = [], 0


def check(ok, what):
    global total
    total += 1
    print(("  ok   " if ok else "  FEL  ") + what)
    if not ok:
        fails.append(what)


# ---- Retrospel känner igen C64
check("C64" in R.ORDER and R.SYSTEM_NAME["C64"] == "Commodore 64" and "C64" in R.LAUNCHER_SYSTEMS, "C64 syns i Retrospel, spelen i startmenyn")
check(R.system_of("/tmp/x/Boulder Dash.d64") == "C64" and R.system_of("/tmp/x/a.prg") == "C64" and R.system_of("/tmp/x/a.crt") == "C64",
      ".d64, .prg och .crt → C64")
check(R.elsewhere("/tmp/x/a.d64") is None and "c64" not in R.ELSEWHERE, "med programmet installerat föreslår Retrospel det inte längre")
tap = os.path.join(R.RETRO, "C64", "Spel", "band.tap"); os.makedirs(os.path.dirname(tap)); open(tap, "wb").write(b"C64-TAPE-RAW\0\0\0\0")
check(R.system_of(tap) == "C64", ".tap i C64-mappen")
check(R.SKIP_BY_SYSTEM["C64"] == {"ROM"} and "ROM" not in R.SKIP_DIRS, "C64:s ROM-mapp är inga spel – men NES-spel i ~/Retro/NES/ROM hittas ändå")

# ---- ROM-filerna känns igen på innehållet
basic = bytearray(8192); basic[4:12] = b"CBMBASIC"
kernal = bytearray(8192); kernal[0x1FFC:0x1FFE] = b"\xe2\xfc"
chargen = bytearray(4096); chargen[:8] = bytes([0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x62, 0x3C, 0x00])
dos = bytearray(16384); dos[100:107] = b"CBM DOS"
check(K.c64_kind(bytes(basic)) == "basic" and K.c64_kind(bytes(kernal)) == "kernal" and K.c64_kind(bytes(chargen)) == "chargen"
      and K.c64_kind(bytes(dos)) == "dos1541" and K.c64_kind(b"x" * 8192) is None, "KERNAL, BASIC, tecken och 1541 känns igen")
s = K.c64_set()
check(s["open"] and s["kernal"].endswith("open-roms/C64/kernal") and s["dos1541"] is None, "utan egna ROM:ar: Open ROMs, virtuell diskettstation")
check(K.add_file("basic.bin", bytes(basic)) and K.add_file("kernal.bin", bytes(kernal)) and K.add_file("annat.bin", b"x" * 100) == [],
      "Lägg in: BASIC och KERNAL tas emot, annat inte")
s = K.c64_set()
check(not s["open"] and s["kernal"].startswith(K.C64DIR) and s["basic"].startswith(K.C64DIR), "egna KERNAL och BASIC används ihop")
kick = os.path.join(R.KICKDIR); os.makedirs(kick, exist_ok=True)
p = os.path.join(R.HOME, "chargen.bin"); open(p, "wb").write(bytes(chargen))
added = R.add_roms([p])
check(len(added) == 1 and added[0]["models"] == ["C64"] and not os.listdir(R.KICKDIR), "Retrospels Lägg in: C64-ROM:en hamnar hos C64, inte bland Kickstart")

# ---- VICE-kommandot
K.host_joysticks = lambda: ["THEC64 Joystick"]
cmd = K.vice_command(["x64sc"], [], {})
check(cmd[:2] == ["x64sc", "-kernal"] and "-joydev2" in cmd and "-VICIIfull" not in cmd, "tom C64: egna ROM:ar, joysticken i port 2, i ett fönster")
cmd = K.vice_command(["x64sc"], ["/spel.d64"], {"c64_joyport": 1, "c64_fullscreen": True})
check("-joydev1" in cmd and "-VICIIfull" in cmd and cmd[-2:] == ["-autostart", "/spel.d64"], "spel: port 1 och helskärm när det är valt")
K.emulator_for = lambda s: ("VICE", ["x64sc"], None)
g = {"system": "C64", "files": [tap], "id": tap, "dir": os.path.dirname(tap), "name": "band"}
check(K.command(g, {})[-2:] == ["-autostart", tap], "programmets kod: VICE startar spelet")
T64 = R.TILLAGG["C64"]
T64.emulator_for = lambda s: ("VICE", ["/usr/bin/x64sc"], None)
rc = R.command(g, {})
check(rc[1:] == ["-I", str(ROOT / "retro_kor.py"), "play", "C64", tap], "Retrospel startar spelet genom programmets kommando – i en egen process")
check(not any(isinstance(v, type(R)) for v in vars(T64).values()) and not hasattr(R, "retro_kod"),
      "Retrospel har inte läst in programmets kod (bara beskrivningen)")
check(K.describe("C64", None) == (None, None) and K.describe("C64", ["x64sc"])[0] is None, "med egna ROM:ar: inget att förklara")

# ---- fönstret
loader = importlib.machinery.SourceFileLoader("c64main", str(ROOT / "main.py"))
spec = importlib.util.spec_from_loader("c64main", loader)
M = importlib.util.module_from_spec(spec)
loader.exec_module(M)
rom = K.c64_roms()[0]["file"]
check(not M.remove_rom(K, "/etc/passwd") and os.path.exists("/etc/passwd"), "Ta bort: bara filer i ~/Retro/C64/ROM")
check(M.remove_rom(K, rom) and not os.path.exists(rom), "Ta bort: en egen ROM-fil")
M.set_cfg(R, "c64_joyport", 1)
check(M.joyport(R) == 1 and R.load_cfg()["c64_joyport"] == 1, "joystickporten sparas där Retrospel läser den")
win = (ROOT / "main.py").read_text(encoding="utf-8")
check(not any(w in win for w in ("urllib", "requests", "curl", "wget")), "fönstret kopplar inte upp sig")
desk = (ROOT / "data/se.wenuyos.app.c64.desktop").read_text(encoding="utf-8")
check(all(k in desk for k in ("Name[sv]=", "Comment[sv]=", "Keywords[sv]=", "Categories=Game;Emulator;")), ".desktop: båda språken, ett emulatorprogram")
toml = (ROOT / "wenuyos-app.toml").read_text(encoding="utf-8")
check('"vice"' in toml and '"open-roms"' in toml, "VICE och Open ROMs installeras med programmet (Debian)")

print(f"\nCommodore 64: {total - len(fails)} av {total} prov godkända")
sys.exit(1 if fails else 0)
