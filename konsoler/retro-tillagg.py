"""Konsoler och hemdatorer i Retrospel – tillägget (programmet Konsoler och hemdatorer, wenuyos-program/konsoler).

NES, SNES, Game Boy (Color, Advance), Sega Mega Drive/Master System/Game Gear, PC Engine, PlayStation, Nintendo 64,
Nintendo DS, GameCube/Wii och PSP – och hemdatorerna Atari ST, ZX Spectrum, MSX och Atari 800.
Retrospel (wenuyos-retro, i WenuyOS) läser ~/Program/*/retro-tillagg.py och lägger in systemen här. Koden körs i
Retrospels namnrymd: T, SV, RETRO, XDG_DATA, uniq, playlist_files, low_profile, flatpak_has, load_cfg, os, re,
shutil, subprocess och json finns redan (se TILLAGG_API i wenuyos-retro). Flyttat ur systemet 20 nov 2026 (Daniel
20 nov 00.00: "Ta bort även konsolerna" → konsoler och hemdatorer, eget program).
"""
API = 1


def _s(name, ext, folder_only=(), short=None, heavy=False, icon="input-gaming-symbolic"):
    return {"name": name, "short": short or name, "icon": icon, "ext": list(ext), "folder_only": list(folder_only),
            "heavy": heavy, "sub": "", "launchers": True}


SYSTEMS = {
    "NES": _s("NES", (".nes", ".unf", ".unif")),
    "SNES": _s("Super Nintendo", (".sfc", ".smc")),
    "GameBoy": _s(T("Game Boy and Game Boy Color", "Game Boy och Game Boy Color"), (".gb", ".gbc"), short="Game Boy"),
    "GameBoyAdvance": _s("Game Boy Advance", (".gba",)),
    "MegaDrive": _s(T("Sega Mega Drive, Master System and Game Gear", "Sega Mega Drive, Master System och Game Gear"),
                    (".md", ".gen", ".smd", ".sms", ".gg"), short="Sega"),
    "PCEngine": _s("PC Engine", (".pce",)),
    "PlayStation": _s("PlayStation", (".cue", ".m3u", ".toc", ".ccd"), folder_only=(".cue", ".m3u", ".toc", ".ccd")),
    "AtariST": _s("Atari ST", (".st", ".msa", ".dim"), folder_only=(".dim",), icon="media-floppy-symbolic"),
    "ZXSpectrum": _s("ZX Spectrum", (".tap", ".tzx", ".z80", ".sna", ".szx"), icon="media-tape-symbolic"),
    "MSX": _s("MSX", (".rom", ".mx1", ".mx2"), folder_only=(".rom",)),
    "Atari800": _s("Atari 800/XL", (".atr", ".xex", ".xfd", ".car"), icon="media-floppy-symbolic"),
    # KOMPATIBILITET.md steg 5: Nintendo 64, Nintendo DS, GameCube/Wii och PSP. Skivavbilder (.iso) bara i systemets mapp.
    "N64": _s("Nintendo 64", (".n64", ".z64", ".v64")),
    "NintendoDS": _s("Nintendo DS", (".nds",)),
    "GameCube": _s(T("GameCube and Wii", "GameCube och Wii"), (".gcm", ".rvz", ".wbfs", ".gcz", ".ciso", ".iso"),
                   folder_only=(".iso",), short="GameCube/Wii", heavy=True),
    "PSP": _s("PlayStation Portable", (".cso", ".iso"), folder_only=(".iso",), short="PSP", heavy=True),
}
ORDER = ["NES", "SNES", "GameBoy", "GameBoyAdvance", "MegaDrive", "PCEngine", "PlayStation", "AtariST", "ZXSpectrum",
         "MSX", "Atari800", "N64", "NintendoDS", "GameCube", "PSP"]
SKIP_DIRS = {"BIOS", "TOS", "Sparningar"}
HEAVY = {s for s, v in SYSTEMS.items() if v["heavy"]}
MEDNAFEN = ("NES", "SNES", "GameBoy", "GameBoyAdvance", "MegaDrive", "PCEngine", "PlayStation")
# system → (namn, program, Debian-paket). Mednafen ligger i /usr/games (inte alltid i PATH).
EMULATORS = {"Mednafen": ("Mednafen", "mednafen", "mednafen"), "AtariST": ("Hatari", "hatari", "hatari"),
             "ZXSpectrum": ("Fuse", "fuse-gtk", "fuse-emulator-gtk"), "MSX": ("openMSX", "openmsx", "openmsx"),
             "Atari800": ("Atari800", "atari800", "atari800"),
             "N64": ("Mupen64Plus", "mupen64plus", "mupen64plus-ui-console"), "NintendoDS": ("DeSmuME", "desmume", "desmume"),
             "GameCube": ("Dolphin", "dolphin-emu", "dolphin-emu")}
PPSSPP = "org.ppsspp.PPSSPP"   # PSP (Flathub, GPL-2.0)


def emulator_for(system):
    """→ (namn, kommando-prefix eller None om saknas, installationskommando)"""
    if system == "PSP":   # PPSSPP finns inte i Debian – Flathub
        if flatpak_has(PPSSPP): return ("PPSSPP", ["flatpak", "run", "--file-forwarding", PPSSPP], None)
        return ("PPSSPP", None, ["flatpak", "install", "-y", "--noninteractive", "flathub", PPSSPP])
    if system in MEDNAFEN: system = "Mednafen"
    label, exe, pkg = EMULATORS[system]
    path = shutil.which(exe) or (os.path.join("/usr/games", exe) if os.path.isfile(os.path.join("/usr/games", exe)) else None)
    return (label, [exe if shutil.which(exe) else path] if path else None, ["sudo", "apt-get", "install", "-y", pkg])


def command(game, cfg):
    system = game["system"]
    name, pre, _ = emulator_for(system)
    if not pre: return None
    if system in MEDNAFEN:
        return mednafen_command(pre, game, cfg)
    if system == "AtariST":
        tos = atari_tos()
        if not tos: return "tos"
        cmd = pre + ["--tos", tos, "--confirm-quit", "false", "-f" if cfg.get("console_fullscreen") else "-w"]
        if tos_is_emutos(tos): cmd += ["--fast-boot", "true"]
        cmd += ["--disk-a", game["files"][0]]
        if len(game["files"]) > 1: cmd += ["--disk-b", game["files"][1]]   # spel på två disketter: den andra i B
        return cmd
    if system == "ZXSpectrum":   # utan Sinclairs ROM: fria Open SE BASIC direkt (annars en ruta om saknad ROM varje gång)
        own = os.path.isfile(os.path.join(ZX_ROMS, "48.rom"))
        return pre + ([] if own or not os.path.isfile(os.path.join(ZX_ROMS, "opense.rom")) else ["--rom-48", os.path.join(ZX_ROMS, "opense.rom")]) + [game["files"][0]]
    if system == "MSX":
        return pre + (["-fullscreen"] if cfg.get("console_fullscreen") else []) + ["-carta", game["files"][0]]
    if system == "Atari800":
        return pre + (["-fullscreen"] if cfg.get("console_fullscreen") else ["-windowed", "-win-width", "672", "-win-height", "480"]) + [game["files"][0]]
    if system == "N64":
        return pre + (["--fullscreen"] if cfg.get("console_fullscreen") else ["--windowed"]) + [game["files"][0]]
    if system == "NintendoDS":
        return pre + [game["files"][0]]
    if system == "GameCube":   # -b: stäng Dolphin när spelet avslutas, -e: starta spelet direkt
        return pre + ["-b", "-e", game["files"][0]]
    if system == "PSP":        # Flatpak: filen lämnas in med @@ … @@ (sandlådan ser den)
        return pre + ["@@", game["files"][0], "@@"]
    return None


def describe(system, pre):
    """text under rubriken i Retrospel, och en knapp ("bios", etikett, webbplats) eller None"""
    if system == "PlayStation" and not ps1_bios():
        return (T("Needs the BIOS from your own PlayStation (a 512 kB file, e.g. SCPH1001.BIN).",
                  "Behöver BIOS från din egen PlayStation (en fil på 512 kB, t.ex. SCPH1001.BIN)."),
                ("bios", T("Add BIOS…", "Lägg in BIOS…"), None) if pre else None)
    if system == "AtariST" and not atari_tos():
        return (T("Needs TOS: your own, or the free EmuTOS – download the zip file from emutos.sourceforge.io and add it here.",
                  "Behöver TOS: din egen, eller fria EmuTOS – hämta zip-filen från emutos.sourceforge.io och lägg in den här."),
                ("bios", T("Add TOS…", "Lägg in TOS…"), "https://emutos.sourceforge.io/download.html") if pre else None)
    if pre and system in MEDNAFEN:
        return (T("Arrow keys and Z/X · Enter = Start · Alt+Shift+1 in the game sets up a gamepad",
                  "Piltangenterna och Z/X · Enter = Start · Alt+Skift+1 i spelet ställer in en handkontroll"), None)
    if system in HEAVY and low_profile():
        return (T("Heavy: needs a faster computer than one with 2 GB of memory – the games may be slow.",
                  "Tungt: behöver en snabbare dator än en med 2 GB minne – spelen kan gå långsamt."), None)
    if pre and system == "MSX":
        return (T("ROM cartridges, with the free C-BIOS", "ROM-kassetter, med fria C-BIOS"), None)
    return (None, None)


def add_file(name, data):
    """PlayStation-BIOS eller Atari ST-TOS (även inuti en .zip, t.ex. EmuTOS) → lista med info"""
    if name.lower().endswith(".zip"):
        import io, zipfile
        out = []
        try:
            with zipfile.ZipFile(io.BytesIO(data)) as z:
                for zi in z.infolist():
                    if zi.is_dir() or zi.file_size > 1048576: continue
                    r = add_bios(os.path.basename(zi.filename), z.read(zi))
                    if r: out.append(r)
        except zipfile.BadZipFile:
            pass
        return out
    if len(data) not in (196608, 262144, 524288, 1048576):
        return []
    r = add_bios(name, data)
    return [r] if r else []


def filter_groups(system, groups):
    """PlayStation: skivor som en .m3u redan räknar upp visas inte en gång till"""
    if system != "PlayStation": return groups
    listed = set()
    for files in groups.values():
        for p in files:
            if p.lower().endswith(".m3u"): listed.update(playlist_files(p))
    groups = {k: [p for p in v if p not in listed] for k, v in groups.items()}
    return {k: v for k, v in groups.items() if v}


def env_for(cmd):
    if any(os.path.basename(c) == "mednafen" for c in cmd[:2]): return mednafen_env()
    if any(os.path.basename(c) == "openmsx" for c in cmd[:2]) and os.environ.get("WAYLAND_DISPLAY"):
        return {**os.environ, "SDL_VIDEODRIVER": "wayland,x11"}   # openMSX provar annars bara X11
    return None


ZX_ROMS = "/usr/share/spectrum-roms"   # opense-basic (fritt) följer med Fuse i Debian


# ---------------- Mednafen: konsolerna ----------------
# Egen mapp (inställningar, sparningar) gömd i ~/.local/share/wenuyos/mednafen. Första gången skrivs tangenter som
# passar en bärbar dator: piltangenterna styr, Z/X (och A/S, Q/W) är knapparna, Enter = Start, höger Skift = Select.
# Mednafens egna standard ligger på det numeriska tangentbordet, som Chromebooks och de flesta bärbara saknar.
# Handkontroll: Alt+Skift+1 i spelet ställer in knapparna (Mednafen frågar en i taget).
MEDNAFEN_HOME = os.path.join(XDG_DATA, "wenuyos", "mednafen")
_K = {"up": 82, "down": 81, "left": 80, "right": 79, "z": 29, "x": 27, "c": 6, "a": 4, "s": 22, "q": 20, "w": 26,
      "e": 8, "r": 21, "enter": 40, "rshift": 229}
_DIRS = {"up": "up", "down": "down", "left": "left", "right": "right"}
MEDNAFEN_KEYS = {
    "nes.input.port1.gamepad": {**_DIRS, "b": "z", "a": "x", "start": "enter", "select": "rshift"},
    "snes.input.port1.gamepad": {**_DIRS, "b": "z", "a": "x", "y": "a", "x": "s", "l": "q", "r": "w", "start": "enter", "select": "rshift"},
    "gb.input.builtin.gamepad": {**_DIRS, "b": "z", "a": "x", "start": "enter", "select": "rshift"},
    "gba.input.builtin.gamepad": {**_DIRS, "b": "z", "a": "x", "shoulder_l": "q", "shoulder_r": "w", "start": "enter", "select": "rshift"},
    "md.input.port1.gamepad": {**_DIRS, "a": "z", "b": "x", "c": "c", "start": "enter"},
    "sms.input.port1.gamepad": {**_DIRS, "fire1": "z", "fire2": "x", "pause": "enter"},
    "gg.input.builtin.gamepad": {**_DIRS, "button1": "z", "button2": "x", "start": "enter"},
    "pce.input.port1.gamepad": {**_DIRS, "ii": "z", "i": "x", "run": "enter", "select": "rshift"},
    "psx.input.port1.gamepad": {**_DIRS, "cross": "x", "circle": "c", "square": "z", "triangle": "s", "l1": "q", "r1": "w",
                                "l2": "e", "r2": "r", "start": "enter", "select": "rshift"},
}


def mednafen_keys_cfg():
    """raderna i mednafen.cfg med WenuyOS tangenter"""
    lines = ["; WenuyOS (wenuyos-retro): piltangenterna och Z/X – ändra med Alt+Skift+1 i ett spel"]
    for prefix, keys in MEDNAFEN_KEYS.items():
        for btn, key in keys.items(): lines.append(f"{prefix}.{btn} keyboard 0x0 {_K[key]}")
    return "\n".join(lines) + "\n"


def mednafen_env():
    os.makedirs(MEDNAFEN_HOME, exist_ok=True)
    cf = os.path.join(MEDNAFEN_HOME, "mednafen.cfg")
    if not os.path.exists(cf):   # bara första gången – sedan är det dina (Mednafen sparar ändringarna själv)
        try:
            with open(cf, "w") as f: f.write(mednafen_keys_cfg())
        except OSError:
            pass
    return {**os.environ, "MEDNAFEN_HOME": MEDNAFEN_HOME}


def m3u_for(files):
    """flera skivor (Disc 1, Disc 2 …) → en .m3u i cachen; Mednafen byter skiva med F6/F8"""
    import hashlib
    d = os.path.join(os.path.expanduser("~/.cache/wenuyos/retro"))
    os.makedirs(d, exist_ok=True)
    out = os.path.join(d, hashlib.sha1("\n".join(files).encode()).hexdigest()[:12] + ".m3u")
    with open(out, "w") as f: f.write("\n".join(files) + "\n")
    return out


def mednafen_command(pre, game, cfg):
    files = game["files"]
    target = files[0] if len(files) == 1 or game["system"] != "PlayStation" else m3u_for(files)
    cmd = pre + ["-video.fs", "1" if cfg.get("console_fullscreen") else "0", "-sound.driver", "sdl"]
    if os.environ.get("WENUYOS_GFX") == "soft": cmd += ["-video.driver", "sdl"]   # utan grafikdrivrutin: ingen OpenGL
    if game["system"] == "PlayStation":
        bios = ps1_bios()
        if not bios: return "bios"
        # en BIOS räcker för alla regioner (regionskontrollen av – annars krävs tre olika filer)
        for region in ("na", "eu", "jp"):
            cmd += [f"-psx.bios_{region}", bios.get(region) or next(iter(bios.values()))]
        cmd += ["-psx.bios_sanity", "0"]
    return cmd + [target]


# ---------------- BIOS och TOS (Inställningar → Emulatorer, eller knappen i Retrospel) ----------------
# PlayStation behöver Sonys BIOS (din egen, från din konsol). Atari ST behöver TOS: din egen, eller EmuTOS – fri
# (GPL) men inte i Debian, så du hämtar zip-filen från emutos.sourceforge.io och lägger in den här.
PS1_BIOS_DIR = os.path.join(RETRO, "PlayStation", "BIOS")
TOS_DIR = os.path.join(RETRO, "AtariST", "TOS")
PS1_REGION = re.compile(rb"System ROM Version\s+([0-9.]+)\s+[0-9/]+\s+([AEJ])")


def ps1_kind(data):
    """→ "na" | "eu" | "jp" | "unknown" (en PlayStation-BIOS men regionen syns inte) | None"""
    if not data or len(data) != 524288 or b"Sony Computer Entertainment" not in data: return None
    m = PS1_REGION.search(data)
    return {b"A": "na", b"E": "eu", b"J": "jp"}[m.group(2)] if m else "unknown"


def tos_kind(data):
    """→ "emutos" | "tos" | None. TOS börjar med ett hopp (0x60) och har sin startadress på plats 8."""
    if not data or len(data) not in (196608, 262144, 524288, 1048576) or data[0] != 0x60: return None
    if data[8:11] not in (b"\x00\xfc\x00", b"\x00\xe0\x00"): return None
    return "emutos" if b"EmuTOS" in data[:65536] or data[0x2C:0x30] == b"ETOS" else "tos"


def tos_is_emutos(path):
    try: return tos_kind(open(path, "rb").read()) == "emutos"
    except OSError: return False


def _files_in(d):
    try: return sorted(os.path.join(d, f) for f in os.listdir(d) if not f.startswith("."))
    except OSError: return []


def ps1_bios():
    """→ {region: fil} för dina PlayStation-BIOS ("unknown" räknas som alla)"""
    out = {}
    for f in _files_in(PS1_BIOS_DIR):
        try: k = ps1_kind(open(f, "rb").read()) if os.path.getsize(f) == 524288 else None
        except OSError: k = None
        if k == "unknown":
            for r in ("na", "eu", "jp"): out.setdefault(r, f)
        elif k: out[k] = f
    return out


def atari_tos():
    """din egen TOS före EmuTOS; inom EmuTOS den största (512k: alla språk)"""
    best = None
    for f in _files_in(TOS_DIR):
        try: data = open(f, "rb").read() if os.path.getsize(f) <= 1048576 else None
        except OSError: data = None
        k = tos_kind(data)
        if k:
            score = (k == "tos", len(data))
            if not best or score > best[0]: best = (score, f)
    return best[1] if best else None


def bios_list():
    out = []
    for f in _files_in(PS1_BIOS_DIR):
        try: k = ps1_kind(open(f, "rb").read()) if os.path.getsize(f) == 524288 else None
        except OSError: k = None
        out.append({"file": f, "system": "PlayStation", "ok": bool(k), "region": k or ""})
    for f in _files_in(TOS_DIR):
        try: k = tos_kind(open(f, "rb").read()) if os.path.getsize(f) <= 1048576 else None
        except OSError: k = None
        out.append({"file": f, "system": "AtariST", "ok": bool(k), "kind": k or ""})
    return out


def add_bios(name, data):
    """PlayStation-BIOS eller Atari ST-TOS → rätt mapp. → info eller None om det inte är någon av dem"""
    import zlib
    if ps1_kind(data): d, sysname = PS1_BIOS_DIR, "PlayStation"
    elif tos_kind(data): d, sysname = TOS_DIR, "AtariST"
    else: return None
    os.makedirs(d, exist_ok=True)
    crc = zlib.crc32(data) & 0xFFFFFFFF
    for f in _files_in(d):   # samma fil finns redan
        try:
            if zlib.crc32(open(f, "rb").read()) & 0xFFFFFFFF == crc: return {"file": f, "name": os.path.basename(f), "models": [sysname]}
        except OSError: pass
    dst = uniq(os.path.join(d, os.path.basename(name)))
    with open(dst, "wb") as f: f.write(data)
    return {"file": dst, "name": os.path.basename(dst), "models": [sysname]}
