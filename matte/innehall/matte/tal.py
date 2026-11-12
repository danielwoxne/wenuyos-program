"""Uppläsning – frågorna läses upp för barn som inte läser själva än.

Använder en fri svensk röst som finns på datorn, i den här ordningen:
  1. Piper (naturlig röst) – `piper` och en svensk röstmodell (sv_SE-*.onnx) i
     ~/.local/share/piper, /usr/share/piper-voices eller /usr/local/share/piper
  2. espeak-ng (finns i Ubuntu: `sudo apt install espeak-ng`)
Finns ingen röst blir uppläsningen tyst – programmet fungerar ändå.

Matematiska tecken skrivs om till ord innan de läses ("3 · 4" → "3 gånger 4"),
så att rösten inte säger "punkt" eller hoppar över dem. Allt körs lokalt – inget
skickas någonstans.
"""
from __future__ import annotations

import glob
import os
import re
import shutil
import subprocess

_pagaende: list[subprocess.Popen] = []

_ORD = [
    (r"(\d)\s*/\s*(\d)", r"\1 delat med \2"),
    (r"·|\*|×", " gånger "),
    (r"(?<=\d)\s*:\s*(?=\d)", " delat med "),
    (r"−|(?<=\s)-(?=\s)", " minus "),
    (r"\+", " plus "),
    (r"≠", " är inte lika med "),
    (r"=", " är lika med "),
    (r"%", " procent"),
    (r"≈", " ungefär "),
    (r"√", " roten ur "),
    (r"²", " i kvadrat"),
    (r"³", " upphöjt till tre"),
    (r"\^", " upphöjt till "),
    (r"<", " är mindre än "),
    (r">", " är större än "),
    (r"▢|\?", " vad "),
]


def som_ord(text: str) -> str:
    """Gör en fråga läsbar för en röst: tecken blir ord, decimalkomma behålls."""
    t = text
    for monster, ers in _ORD:
        t = re.sub(monster, ers, t)
    return re.sub(r"\s+", " ", t).strip()


def _piper() -> tuple[str, str] | None:
    exe = shutil.which("piper")
    if not exe:
        return None
    for mapp in (os.path.expanduser("~/.local/share/piper"), "/usr/share/piper-voices", "/usr/local/share/piper"):
        modeller = sorted(glob.glob(os.path.join(mapp, "**", "sv_SE-*.onnx"), recursive=True))
        if modeller:
            return exe, modeller[0]
    return None


def finns_rost() -> bool:
    return bool(_piper() or shutil.which("espeak-ng") or shutil.which("espeak"))


def tyst() -> None:
    """Avbryter det som läses upp just nu."""
    while _pagaende:
        p = _pagaende.pop()
        if p.poll() is None:
            p.terminate()


def las(text: str) -> bool:
    """Läser upp texten i bakgrunden. Returnerar False om ingen röst finns."""
    tyst()
    ord_ = som_ord(text)
    if not ord_:
        return False
    try:
        pip = _piper()
        if pip and shutil.which("aplay"):
            exe, modell = pip
            p1 = subprocess.Popen([exe, "--model", modell, "--output-raw"], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
            p2 = subprocess.Popen(["aplay", "-q", "-r", "22050", "-f", "S16_LE", "-t", "raw", "-"],
                                  stdin=p1.stdout, stderr=subprocess.DEVNULL)
            p1.stdin.write(ord_.encode()); p1.stdin.close()
            _pagaende.extend([p1, p2])
            return True
        esp = shutil.which("espeak-ng") or shutil.which("espeak")
        if esp:
            _pagaende.append(subprocess.Popen([esp, "-v", "sv", "-s", "140", ord_],
                                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
            return True
    except OSError:
        pass
    return False
