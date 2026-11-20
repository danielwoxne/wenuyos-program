#!/usr/bin/env python3
"""Konsoler och hemdatorer som eget program: fönstrets logik och programfilerna."""
import importlib.machinery, importlib.util, pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import kärna  # noqa: E402

R, K = kärna.ladda()
ROOT = pathlib.Path(__file__).resolve().parents[1]
n = 0
def ok(c, msg):
    global n
    if not c:
        print("FEL:", msg); sys.exit(1)
    n += 1

loader = importlib.machinery.SourceFileLoader("konsmain", str(ROOT / "main.py"))
spec = importlib.util.spec_from_loader("konsmain", loader)
M = importlib.util.module_from_spec(spec)
loader.exec_module(M)
K.flatpak_has = lambda a: False
rows = M.systems(R, K)
ok([r[0] for r in rows] == K.ORDER and len(rows) == 15, "alla 15 system visas, i Retrospels ordning")
psp = next(r for r in rows if r[0] == "PSP")
ok(psp[2] == "PPSSPP" and psp[4][:2] == ["flatpak", "install"], "PSP: PPSSPP från Flathub när man trycker Installera")
ok(all(r[4] for r in rows if not r[3]), "varje system som saknar sin emulator har ett installationskommando")
ok("C64" not in K.SYSTEMS and "Amiga" not in K.SYSTEMS, "inte C64 (eget program) och inte Amiga (i Retrospel)")
win = (ROOT / "main.py").read_text(encoding="utf-8")
ok(not re.search(r"urllib|requests|curl |wget", win), "fönstret kopplar inte upp sig (installationen sker synligt i terminalen)")
desk = (ROOT / "data/se.wenuyos.app.konsoler.desktop").read_text(encoding="utf-8")
ok(all(k in desk for k in ("Name[sv]=Konsoler och hemdatorer", "Comment[sv]=", "Keywords[sv]=", "Categories=Game;Emulator;")), ".desktop: båda språken")
# Retrospel läser bara beskrivningen; programmets kommando körs i en egen process
import json, os, subprocess, tempfile
j = json.load(open(ROOT / "retro-tillagg.json", encoding="utf-8"))
ok(R.check_tillagg(j, str(ROOT))[1][0]["id"] == "NES" and len(R.TILLAGG) == 15, "beskrivningen godkänns av Retrospel, alla 15 system")
ps = R.TILLAGG["PlayStation"]
ps.emulator_for = lambda s: ("Mednafen", ["/usr/games/mednafen"], None)
import shutil as _sh
bdir = os.path.join(R.RETRO, "PlayStation", "BIOS"); _sh.rmtree(bdir, ignore_errors=True)
text, btn = ps.describe("PlayStation", ["mednafen"])
ok("BIOS" in text and btn and btn[0] == "bios", "PlayStation utan BIOS: Retrospel säger det och visar Lägg in BIOS…")
ok(R.command({"system": "PlayStation", "files": ["/x.cue"]}, {}) == "bios", "PlayStation utan BIOS: Retrospel ber om BIOS i stället för att starta")
junk = os.path.join(tempfile.mkdtemp(), "skräp.bin"); open(junk, "wb").write(b"x" * 1000)
ok(ps.add_file(junk) == [], "Lägg in en fil som inte är en BIOS: programmets kommando svarar tom lista")
r = subprocess.run([sys.executable, "-I", str(ROOT / "retro_kor.py"), "play", "Okänt", "/x.nes"], capture_output=True, text=True,
                   env={**os.environ, "WENUYOS_RETRO": os.environ["WENUYOS_RETRO"]})
ok(r.returncode == 2, "programmets kommando: ett okänt system startar ingenting")
print(f"Konsoler, programmet: {n} prov godkända")
