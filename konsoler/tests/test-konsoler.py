#!/usr/bin/env python3
"""Konsoler och hemdatorer (programmet, förut en del av Retrospel i systemet): spelkonsoler och hemdatorer (Daniel 14 nov: "Finns det fler program och spel som kan emuleras" →
spelkonsoler via Mednafen och hemdatorerna Atari ST, ZX Spectrum, MSX och Atari 800).

Provar tillägget retro-tillagg.py i Retrospel (wenuyos-retro, se tests/kärna.py) med en tillfällig hemmapp och påhittade filer (inga riktiga spel,
BIOS eller TOS): vilket system en fil hör till, att .tap känns igen på innehållet, skivspel och .m3u, att BIOS/TOS
känns igen och läggs i rätt mapp (även ur en zip), kommandona till emulatorerna och tangenterna för bärbara datorer.
"""
import importlib.machinery, importlib.util, io, json, os, pathlib, sys, tempfile, zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]   # programmappen konsoler/
sys.path.insert(0, str(ROOT / "tests"))
import kärna  # noqa: E402
fails = []
count = [0]


def check(ok, what):
    count[0] += 1
    print(("  ok   " if ok else "  FEL  ") + what)
    if not ok:
        fails.append(what)


home = tempfile.mkdtemp()
os.environ.update(HOME=home, XDG_CONFIG_HOME=os.path.join(home, ".config"), XDG_DATA_HOME=os.path.join(home, ".local/share"),
                  LANG="sv_SE.UTF-8")
os.environ.pop("WENUYOS_GFX", None)
R, K = kärna.ladda()   # Retrospel (wenuyos-retro) med det här tillägget
RETRO = R.RETRO


def put(rel, data=b"\0" * 64):
    p = os.path.join(RETRO, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "wb") as f: f.write(data)
    return p


# ---- vilket system
check(R.system_of("/tmp/x/Mario (USA).nes") == "NES", ".nes → NES")
check(R.system_of("/tmp/x/Zelda.sfc") == "SNES" and R.system_of("/tmp/x/a.smc") == "SNES", ".sfc/.smc → SNES")
check(R.system_of("/tmp/x/a.gbc") == "GameBoy" and R.system_of("/tmp/x/a.gba") == "GameBoyAdvance", "Game Boy och Advance")
check(R.system_of("/tmp/x/Sonic.md") == "MegaDrive" and R.system_of("/tmp/x/a.gg") == "MegaDrive", "Mega Drive och Game Gear")
check(R.system_of("/tmp/x/a.st") == "AtariST" and R.system_of("/tmp/x/a.atr") == "Atari800" and R.system_of("/tmp/x/a.mx2") == "MSX",
      "Atari ST, Atari 800, MSX")
check(R.system_of("/tmp/x/a.adf") == "Amiga" and R.system_of("/tmp/x/a.d64") is None, "Amiga som förut; C64 är ett eget program")
check(R.system_of("/tmp/x/Album.cue") is None and R.system_of("/tmp/x/lista.m3u") is None and R.system_of("/tmp/x/a.rom") is None,
      "utanför ~/Retro: .cue (ljud-cd), .m3u (spellista) och .rom gissas inte")
check(R.system_of(os.path.join(RETRO, "PlayStation", "Spel", "Crash.cue")) == "PlayStation", "i ~/Retro/PlayStation: .cue är ett spel")
check(R.system_of(os.path.join(RETRO, "MSX", "ROM", "Knightmare.rom")) == "MSX", "i ~/Retro/MSX: .rom är ett spel")
zx = put("../Hämtat/manic.tap", b"\x13\x00\x00\x00ManicMiner" + b"\0" * 20)
c64 = put("../Hämtat/spel.tap", b"C64-TAPE-RAW\x01\0\0\0" + b"\0" * 20)
check(R.system_of(zx) == "ZXSpectrum" and R.system_of(c64) == "ZXSpectrum", ".tap utan C64-programmet: Spectrum (det enda system som har .tap här)")
check(R.system_of(os.path.join(RETRO, "ZXSpectrum", "x.tap")) == "ZXSpectrum", ".tap i Spectrum-mappen")

# ---- biblioteket
put("NES/ROM/Super Mario Bros. (World).nes")
put("PlayStation/Final Fantasy VII (Disc 1).cue", b'FILE "ff1.bin" BINARY\n')
put("PlayStation/Final Fantasy VII (Disc 2).cue", b'FILE "ff2.bin" BINARY\n')
put("PlayStation/Crash/Crash.m3u", b"Crash (Disc 1).cue\n")
put("PlayStation/Crash/Crash (Disc 1).cue", b'FILE "c.bin" BINARY\n')
put("PlayStation/BIOS/scph1001.bin")
put("AtariST/TOS/etos.img")
put("AtariST/Spel/Xenon 2 (Disk 1).st"); put("AtariST/Spel/Xenon 2 (Disk 2).st")
games = R.scan()
by = {(g["system"], g["name"]): g for g in games}
check(("NES", "Super Mario Bros") in by, "NES-spel i ~/Retro/NES/ROM (wenuyos retro setup) hittas")
ff = by.get(("PlayStation", "Final Fantasy VII"))
check(ff is not None and len(ff["files"]) == 2, "PlayStation: två skivor blir ett spel")
crash = [g for g in games if g["system"] == "PlayStation" and "Crash" in g["name"]]
check(len(crash) == 1 and crash[0]["files"][0].endswith(".m3u"), ".m3u visas, skivorna den räknar upp inte en gång till")
check(not any("scph" in g["name"].lower() or "etos" in g["name"].lower() for g in games), "BIOS och TOS är inga spel")
st = by.get(("AtariST", "Xenon 2"))
check(st is not None and len(st["files"]) == 2, "Atari ST: två disketter blir ett spel")

# ---- BIOS och TOS
ps1 = bytearray(524288)
for at, txt in ((100, b"Sony Computer Entertainment Inc."), (200, b"System ROM Version 4.5 05/25/00 E")): ps1[at:at + len(txt)] = txt
tos = bytearray(262144); tos[0] = 0x60; tos[8:11] = b"\x00\xe0\x00"; tos[0x2C:0x30] = b"ETOS"
kick = bytearray(524288); kick[0:2] = b"\x11\x14"
check(K.ps1_kind(bytes(ps1)) == "eu" and K.ps1_kind(bytes(kick)) is None, "PlayStation-BIOS och region känns igen")
check(K.tos_kind(bytes(tos)) == "emutos" and K.tos_kind(bytes(kick)) is None and K.tos_kind(bytes(ps1)) is None,
      "TOS känns igen, en Kickstart eller BIOS är ingen TOS")
dl = os.path.join(home, "Hämtat"); os.makedirs(dl, exist_ok=True)
with open(os.path.join(dl, "SCPH5502.BIN"), "wb") as f: f.write(ps1)
buf = os.path.join(dl, "emutos-256k-1.4.zip")
with zipfile.ZipFile(buf, "w") as z:
    z.writestr("emutos-256k-1.4/etos256us.img", bytes(tos)); z.writestr("emutos-256k-1.4/readme.txt", "EmuTOS")
added = R.add_roms([os.path.join(dl, "SCPH5502.BIN"), buf])
check(sorted(m for r in added for m in r["models"]) == ["AtariST", "PlayStation"], "Lägg in: BIOS och TOS (ur zip) – inte som Kickstart")
check(os.path.isfile(os.path.join(K.PS1_BIOS_DIR, "SCPH5502.BIN")) and os.path.isfile(os.path.join(K.TOS_DIR, "etos256us.img")),
      "BIOS i PlayStation/BIOS, TOS i AtariST/TOS")
check(not any(f.endswith((".BIN", ".img")) for f in os.listdir(R.KICKDIR)), "inget hamnar bland Kickstart-ROM:arna")
check(len(R.add_roms([os.path.join(dl, "SCPH5502.BIN")])) == 1 and len(os.listdir(K.PS1_BIOS_DIR)) == 2, "samma BIOS två gånger: ingen kopia")
check(K.ps1_bios().get("eu", "").endswith("SCPH5502.BIN") and K.atari_tos().endswith("etos256us.img"), "BIOS och TOS används")

# ---- kommandona
cfg = {}
cmd = K.mednafen_command(["mednafen"], ff, cfg)
check(cmd[-1].endswith(".m3u") and "-psx.bios_sanity" in cmd and cmd[cmd.index("-psx.bios_na") + 1].endswith("SCPH5502.BIN"),
      "PlayStation: .m3u för skivbyte, en BIOS för alla regioner")
check(open(cmd[-1]).read().splitlines() == ff["files"], ".m3u räknar upp skivorna i ordning")
mario = by[("NES", "Super Mario Bros")]
cmd = K.mednafen_command(["mednafen"], mario, cfg)
check(cmd[-1] == mario["files"][0] and "-video.driver" not in cmd and cmd[cmd.index("-video.fs") + 1] == "0", "NES i ett fönster")
os.environ["WENUYOS_GFX"] = "soft"
check(K.mednafen_command(["mednafen"], mario, cfg)[-3:-1] == ["-video.driver", "sdl"], "utan grafikdrivrutin: Mednafen utan OpenGL")
os.environ.pop("WENUYOS_GFX")
os.rename(K.PS1_BIOS_DIR, K.PS1_BIOS_DIR + "-x")
check(K.mednafen_command(["mednafen"], ff, cfg) == "bios", "PlayStation utan BIOS: Retrospel ber om den")
os.rename(K.PS1_BIOS_DIR + "-x", K.PS1_BIOS_DIR)
R.emulator_for = K.emulator_for = lambda system: ("X", ["emu"], None)
c = R.command(st, cfg)
check(c[:3] == ["emu", "--tos", K.atari_tos()] and c[c.index("--disk-a") + 1].endswith("(Disk 1).st") and "--disk-b" in c and "--fast-boot" in c,
      "Atari ST: TOS, diskett 1 i A och 2 i B, snabbstart med EmuTOS")
K.ZX_ROMS = os.path.join(home, "zxroms"); os.makedirs(K.ZX_ROMS); open(os.path.join(K.ZX_ROMS, "opense.rom"), "wb").close()
check(R.command({"system": "ZXSpectrum", "files": [zx], "id": zx}, cfg) == ["emu", "--rom-48", os.path.join(K.ZX_ROMS, "opense.rom"), zx],
      "ZX Spectrum: Fuse med filen och fria Open SE BASIC")
open(os.path.join(K.ZX_ROMS, "48.rom"), "wb").close()
check(R.command({"system": "ZXSpectrum", "files": [zx], "id": zx}, cfg) == ["emu", zx], "ZX Spectrum: Sinclairs ROM när den finns")
check(R.command({"system": "MSX", "files": ["/a.rom"], "id": "/a.rom"}, cfg) == ["emu", "-carta", "/a.rom"], "MSX: kassetten i port A")
check(R.command({"system": "Atari800", "files": ["/a.xex"], "id": "/a.xex"}, {"console_fullscreen": True}) == ["emu", "-fullscreen", "/a.xex"],
      "Atari 800 (helskärm när den är vald)")
for f in os.listdir(K.TOS_DIR): os.remove(os.path.join(K.TOS_DIR, f))
check(R.command(st, cfg) == "tos", "Atari ST utan TOS: Retrospel ber om den")

# ---- emulatorerna installeras från Debian
src = (ROOT / "retro-tillagg.py").read_text(encoding="utf-8")
check(all(f'"{p}")' in src for p in ("mednafen", "hatari", "fuse-emulator-gtk", "openmsx", "atari800")), "paketen finns i Debian 13 (apt)")

# ---- tangenterna
keys = K.mednafen_keys_cfg()
check("nes.input.port1.gamepad.up keyboard 0x0 82" in keys and "nes.input.port1.gamepad.a keyboard 0x0 27" in keys,
      "NES: piltangenterna och X (inte det numeriska tangentbordet)")
check("psx.input.port1.gamepad.cross keyboard 0x0 27" in keys and "snes.input.port1.gamepad.y keyboard 0x0 4" in keys, "PlayStation och SNES")
env = K.mednafen_env()
check(env["MEDNAFEN_HOME"] == K.MEDNAFEN_HOME and open(os.path.join(K.MEDNAFEN_HOME, "mednafen.cfg")).read() == keys,
      "Mednafens mapp gömd i ~/.local/share/wenuyos, tangenterna skrivs första gången")
with open(os.path.join(K.MEDNAFEN_HOME, "mednafen.cfg"), "w") as f: f.write("egna\n")
K.mednafen_env()
check(open(os.path.join(K.MEDNAFEN_HOME, "mednafen.cfg")).read() == "egna\n", "dina egna ändringar skrivs aldrig över")

# ---- startmenyn
R.sync_launchers(games, {})
names = os.listdir(R.APPS_DIR)
texts = [open(os.path.join(R.APPS_DIR, n)).read() for n in names]
check(any("Name=Super Mario Bros\n" in t and "GenericName=NES-spel" in t for t in texts), "spelen syns i startmenyn och sökfältet")

# ---- Retrospel ber om BIOS/TOS och förklarar tangenterna (tilläggets describe)
d = dict((s, K.describe(s, ["emu"])) for s in ("NES", "PlayStation", "AtariST"))
check("Alt+Skift+1" in (d["NES"][0] or ""), "NES: tangenterna förklaras under rubriken")
check(d["PlayStation"][1] is None and "Alt+Skift+1" in (d["PlayStation"][0] or ""), "PlayStation med BIOS: ingen fråga om BIOS, bara tangenterna")
check("Lägg in BIOS…" in src and "Lägg in TOS…" in src and "Alt+Skift+1" in src, "Retrospel förklarar tangenterna och ber om BIOS/TOS")

print(f"Retrospel, fler system: {count[0] - len(fails)} av {count[0]} prov godkända")
sys.exit(1 if fails else 0)
