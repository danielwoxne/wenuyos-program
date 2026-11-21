"""Laddar Retrospel (wenuyos-retro från WenuyOS) med programmets tillägg i en tillfällig hemmapp.
Retrospel hittas i $WENUYOS_RETRO, /usr/bin/wenuyos-retro eller en New-wenuyos-utcheckning bredvid det här förrådet.
Saknas det: proven hoppas över (kod 77) – de körs på en WenuyOS-dator eller med WENUYOS_RETRO=…"""
import importlib.machinery, importlib.util, os, pathlib, shutil, sys, tempfile

HERE = pathlib.Path(__file__).resolve().parents[1]


def hitta():
    for p in (os.environ.get("WENUYOS_RETRO"), "/usr/bin/wenuyos-retro",
              str(HERE.parents[1] / "new-wenuyos/rootfs/usr/bin/wenuyos-retro")):
        if p and os.path.isfile(p): return p
    return None


def ladda(home=None):
    """→ (Retrospel, tillägget). Sätter HOME m.m. till en tillfällig mapp om den inte redan är satt av provet."""
    core = hitta()
    if not core:
        print("Retrospel (wenuyos-retro) hittades inte – sätt WENUYOS_RETRO=<sökväg>. Proven hoppas över.")
        sys.exit(77)
    if home is None and not os.environ.get("HOME", "").startswith(tempfile.gettempdir()):
        home = tempfile.mkdtemp()
    if home:
        os.environ.update(HOME=home, XDG_CONFIG_HOME=os.path.join(home, ".config"), XDG_DATA_HOME=os.path.join(home, ".local/share"))
    os.environ.setdefault("LANG", "sv_SE.UTF-8")
    os.environ["WENUYOS_RETRO"] = core
    sys.path.insert(0, str(HERE))
    import retro_kor
    R = retro_kor.core()
    R.load_tillagg([str(HERE)])   # som när programmet är installerat (beskrivningen, ingen kod)
    if not R.TILLAGG:
        print("FEL: retro-tillagg.json lästes inte in"); sys.exit(1)
    _, K = retro_kor.load(R)
    return R, K
