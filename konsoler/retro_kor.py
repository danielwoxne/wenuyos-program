#!/usr/bin/python3
"""Konsoler och hemdatorer – programmets kommando för Retrospel (wenuyos-retro i WenuyOS).

Retrospel kör aldrig programmets kod själv (ChatGPT 20 nov): det läser bara retro-tillagg.json. När användaren
trycker Spela eller Lägg in startar Retrospel det här kommandot i en egen process:
  retro_kor.py play <system> <fil …>   räkna ut emulatorns kommando (retro-kod.py) och starta den
  retro_kor.py add <fil>               lägg in en BIOS/ROM-fil → JSON-lista med det som lades in
Koden i retro-kod.py får Retrospels hjälpfunktioner (load(), nedan) – i den här processen, inte i Retrospels.
"""
import importlib.machinery, importlib.util, json, os, sys, types

HERE = os.path.dirname(os.path.realpath(__file__))
SHARED = ["T", "SV", "HOME", "RETRO", "XDG_DATA", "uniq", "rom_bytes", "host_joysticks", "playlist_files", "cue_files",
          "low_profile", "flatpak_has", "load_cfg", "save_cfg", "nice_name", "os", "re", "shutil", "subprocess", "json", "sys"]


def core():
    """Retrospel som bibliotek (WENUYOS_RETRO eller /usr/bin/wenuyos-retro)"""
    path = os.environ.get("WENUYOS_RETRO") or "/usr/bin/wenuyos-retro"
    loader = importlib.machinery.SourceFileLoader("wenuyos_retro", path)
    spec = importlib.util.spec_from_loader("wenuyos_retro", loader)
    R = importlib.util.module_from_spec(spec)
    loader.exec_module(R)
    return R


def load(R=None):
    """→ (Retrospel, programmets kod)"""
    R = R or core()
    K = types.ModuleType("retro_kod")
    K.__file__ = os.path.join(HERE, "retro-kod.py")
    for k in SHARED:
        setattr(K, k, getattr(R, k))
    exec(compile(open(K.__file__, encoding="utf-8").read(), K.__file__, "exec"), K.__dict__)
    return R, K


def note(R, text):
    try:
        import subprocess
        subprocess.run(["notify-send", "-a", "Retrospel", text], timeout=5)
    except Exception:
        print(text, file=sys.stderr)


def main(a):
    if len(a) >= 3 and a[0] == "play":
        R, K = load()
        system, files = a[1], a[2:]
        if system not in K.SYSTEMS:
            return 2
        game = {"system": system, "files": files, "id": files[0], "dir": os.path.dirname(files[0]),
                "name": os.path.splitext(os.path.basename(files[0]))[0]}
        cmd = K.command(game, R.load_cfg())
        if cmd in ("bios", "tos"):
            note(R, R.T("Add the BIOS first (open the program)", "Lägg in BIOS först (öppna programmet)") if cmd == "bios"
                 else R.T("Add TOS first (open the program)", "Lägg in TOS först (öppna programmet)"))
            return 3
        if not isinstance(cmd, list) or not cmd:
            note(R, R.T("The emulator is not installed", "Emulatorn är inte installerad"))
            return 2
        env = (K.env_for(cmd) if hasattr(K, "env_for") else None) or dict(os.environ)
        os.execvpe(cmd[0], cmd, env)
    if len(a) == 2 and a[0] == "add":
        R, K = load()
        p = a[1]
        try:
            if os.path.getsize(p) > 64 * 1048576:
                raise OSError
            data = open(p, "rb").read() if p.lower().endswith(".zip") else R.rom_bytes(p)
        except OSError:
            print("[]")
            return 0
        out = K.add_file(os.path.basename(p), data) or []
        print(json.dumps([{k: v for k, v in r.items() if isinstance(v, (str, int, list))} for r in out], ensure_ascii=False))
        return 0
    print(__doc__.strip(), file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
