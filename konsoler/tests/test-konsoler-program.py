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
print(f"Konsoler, programmet: {n} prov godkända")
