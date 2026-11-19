"""Commodore 64 i Retrospel – tillägget (programmet Commodore 64, wenuyos-program/c64).

Retrospel (wenuyos-retro, i WenuyOS) läser ~/Program/*/retro-tillagg.py och lägger in systemen här. Koden körs i
Retrospels namnrymd: T, SV, RETRO, XDG_DATA, uniq, rom_bytes, host_joysticks, load_cfg, save_cfg, os, re, shutil,
subprocess och json finns redan (se TILLAGG_API i wenuyos-retro). Flyttat ur systemet 20 nov 2026 (Daniel 19 nov:
"Ta även bort Commodore emuleringen" → bara C64, eget program).
"""
API = 1

SYSTEMS = {
    "C64": {"name": "Commodore 64", "short": "Commodore 64", "icon": "input-gaming-symbolic",
            "ext": [".d64", ".d71", ".d81", ".g64", ".t64", ".tap", ".prg", ".crt"], "folder_only": [], "sub": "Spel",
            "launchers": True},
}
ORDER = ["C64"]
SKIP_DIRS = {"ROM"}
EMULATOR = ("VICE", "x64sc", "vice")


def emulator_for(system):
    """→ (namn, kommando-prefix eller None om saknas, installationskommando)"""
    label, exe, pkg = EMULATOR
    path = shutil.which(exe)
    return (label, [path] if path else None, ["sudo", "apt-get", "install", "-y", pkg, "open-roms"])


def command(game, cfg):
    _, pre, _ = emulator_for("C64")
    return vice_command(pre, game["files"], cfg) if pre else None


def describe(system, pre):
    """text under rubriken i Retrospel, och en knapp ("rom", etikett, webbplats) eller None"""
    if pre and not c64_roms():
        return (T("Uses the free Open ROMs. Your own C64 ROMs: open Commodore 64.",
                  "Använder fria Open ROMs. Egna C64-ROM:ar: öppna Commodore 64."), None)
    return (None, None)


def add_file(name, data):
    """en ROM-fil som släpps i Retrospel → lista med info (tom om det inte är en C64-ROM)"""
    if not data or len(data) > 16400:
        return []
    kind = c64_kind(data)
    if not kind:
        return []
    tmp = os.path.join(os.path.expanduser("~/.cache/wenuyos"), "c64-rom-in")
    os.makedirs(tmp, exist_ok=True)
    p = os.path.join(tmp, os.path.basename(name) or "rom.bin")
    with open(p, "wb") as f:
        f.write(data)
    try:
        out = add_c64_rom(p, data, kind)
    finally:
        try: os.remove(p)
        except OSError: pass
    for r in out: r["models"] = ["C64"]
    return out


def env_for(cmd):
    return None


# ---------------- C64-ROM:ar (Inställningar → Emulatorer) ----------------
# Dina egna, lagliga ROM-filer (t.ex. från C64 Forever) i ~/Retro/C64/ROM. Typen känns igen på innehållet
# (inte på namnet): BASIC har "CBMBASIC", teckenuppsättningen börjar med "@", 1541-ROM:en har "CBM DOS".
# Kända original får sitt namn ur kontrollsumman. Utan egna används Open ROMs (fri ersättning, LGPL-3).
C64DIR = os.path.join(RETRO, "C64", "ROM")
OPEN_ROMS = "/usr/share/open-roms/C64"
C64_TYPES = {"kernal": "KERNAL", "basic": "BASIC", "chargen": T("Character set", "Teckenuppsättning"),
             "dos1541": T("1541 disk drive", "1541-diskettstation")}
C64_KNOWN = {   # crc32: namn
    0xDBE3E7C7: "KERNAL 901227-03", 0xA5C687B3: "KERNAL 901227-02", 0xDCE782FA: "KERNAL 901227-01",
    0xF833D117: "BASIC V2 901226-01", 0xEC4272EE: "Chargen 901225-01",
    0x899FA3C5: "1541 DOS 325302-01/901229-05",
}
CHARGEN_AT = bytes([0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x62, 0x3C, 0x00])   # tecknet "@"


def c64_kind(data):
    """→ "kernal" | "basic" | "chargen" | "dos1541" | "basic+kernal" | None"""
    if not data: return None
    n = len(data)
    if n == 8192 and data[4:12] == b"CBMBASIC": return "basic"
    if n == 16384 and data[4:12] == b"CBMBASIC": return "basic+kernal"   # 16 kB-dump (C64C 251913-01)
    if n == 8192:
        reset = data[0x1FFC] | data[0x1FFD] << 8
        if 0xE000 <= reset <= 0xFFFF and b"CBMBASIC" not in data: return "kernal"
    if n == 4096 and data[:8] == CHARGEN_AT: return "chargen"
    if n == 16384 and b"CBM DOS" in data: return "dos1541"
    return None


def c64_info(path):
    import zlib
    data = rom_bytes(path)
    kind = c64_kind(data)
    info = {"file": path, "kind": kind or "", "ok": kind is not None, "known": False,
            "name": T("Not a C64 ROM", "Inte en C64-ROM")}
    if kind:
        crc = zlib.crc32(data) & 0xFFFFFFFF
        info["known"] = crc in C64_KNOWN
        info["name"] = C64_KNOWN.get(crc) or (C64_TYPES[kind] + T(" (other version)", " (annan version)"))
    return info


def c64_roms():
    try:
        files = sorted(os.path.join(C64DIR, f) for f in os.listdir(C64DIR) if not f.startswith(".") and f != "rom.key")
    except OSError:
        return []
    return [c64_info(f) for f in files if os.path.isfile(f)]


def add_c64_rom(path, data, kind):
    """spara okrypterad (VICE läser bara vanliga filer); en 16 kB BASIC+KERNAL delas i två"""
    import zlib
    os.makedirs(C64DIR, exist_ok=True)
    base = os.path.splitext(os.path.basename(path))[0]
    parts = [(data[:8192], "basic"), (data[8192:], "kernal")] if kind == "basic+kernal" else [(data, kind)]
    out = []
    for blob, k in parts:
        crc = zlib.crc32(blob) & 0xFFFFFFFF
        dst = os.path.join(C64DIR, f"{k}-{base}.bin" if not base.lower().startswith(k) else f"{base}.bin")
        if any(zlib.crc32(rom_bytes(r["file"]) or b"") & 0xFFFFFFFF == crc for r in c64_roms()):
            continue   # samma ROM finns redan
        dst = uniq(dst)
        with open(dst, "wb") as f: f.write(blob)
        out.append(c64_info(dst))
    return out


def c64_set():
    """vilka ROM:ar VICE ska starta med → {kernal, basic, chargen, dos1541 (eller None), open: bool}
    Egna KERNAL+BASIC används ihop (de hör ihop); saknas någon av dem används Open ROMs för båda."""
    have = {}
    for r in c64_roms():
        if r["ok"] and (r["kind"] not in have or (r["known"] and not have[r["kind"]]["known"])):
            have[r["kind"]] = r
    pick = lambda k: have[k]["file"] if k in have else None
    own = bool(pick("kernal") and pick("basic"))
    s = {"open": not own, "dos1541": pick("dos1541")}
    for k in ("kernal", "basic", "chargen"):
        s[k] = (pick(k) if own or k == "chargen" else None) or os.path.join(OPEN_ROMS, k)
    if not os.path.isfile(s["kernal"]): s["missing"] = True   # varken egna eller Open ROMs
    return s


def first_prg(image):
    """första programmet på en diskettavbild → .prg i cachen (när ingen 1541-ROM finns och Open ROMs
    används går det inte att läsa disketten "på riktigt" – programmet läggs då direkt i minnet)"""
    import hashlib
    cache = os.path.join(os.path.expanduser("~/.cache/wenuyos/c64"), hashlib.sha1(image.encode()).hexdigest()[:12])
    os.makedirs(cache, exist_ok=True)
    out = os.path.join(cache, "start.prg")
    if os.path.isfile(out) and os.path.getmtime(out) >= os.path.getmtime(image): return out
    try:
        lst = subprocess.run(["c1541", "-attach", image, "-list"], capture_output=True, timeout=20).stdout.decode("latin-1")
    except Exception:
        return None
    for l in lst.splitlines():
        m = re.match(r'\s*\d+\s+"([^"]+)"\s+prg', l, re.I)
        if m:
            name = m.group(1)
            if os.path.exists(out): os.remove(out)
            subprocess.run(["c1541", "-attach", image, "-read", name, out], capture_output=True, timeout=20)
            return out if os.path.isfile(out) else None
    return None


def vice_command(pre, files, cfg):
    """x64sc i ett vanligt fönster med rätt ROM:ar. Diskettstation: riktig 1541 om du har dess ROM,
    annars WenuyOS "virtuella" station (räcker för de flesta spel)."""
    s = c64_set()
    cmd = pre + ["-kernal", s["kernal"], "-basic", s["basic"], "-chargen", s["chargen"], "+confirmonexit"]
    if cfg.get("c64_fullscreen"): cmd.append("-VICIIfull")
    # joystick (t.ex. TheC64-joysticken via USB): den första i port 2 – där de flesta C64-spel läser den –
    # och en andra i port 1. Utan joystick: piltangenterna i port 2 är inte VICE standard, så inget ändras.
    joys = host_joysticks()
    if joys:
        port = 1 if cfg.get("c64_joyport") == 1 else 2
        cmd += [f"-joydev{port}", "4"]
        if len(joys) > 1: cmd += [f"-joydev{3 - port}", "5"]
    rt = os.environ.get("XDG_RUNTIME_DIR", "")
    if rt and not os.path.exists(os.path.join(rt, "pulse", "native")): cmd.append("+sound")   # inget ljudsystem: tyst i stället för felruta
    if s["dos1541"]:
        cmd += ["-dos1541", s["dos1541"], "-drive8truedrive"]
    else:
        cmd += ["+drive8truedrive", "-virtualdev8"]
    if not files: return cmd   # en tom C64 (BASIC)
    f = files[0]
    disk = os.path.splitext(f)[1].lower() in (".d64", ".d71", ".d81", ".g64")
    if s["open"] and not s["dos1541"] and disk:
        prg = first_prg(f)
        if prg:   # Open ROMs + ingen 1541: programmet direkt i minnet, disketten i station 8 för resten
            return cmd + ["-autostartprgmode", "1", "-8", f, "-autostart", prg]
    return cmd + ["-autostart", f]
