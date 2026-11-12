"""Ikonerna – Daniels lerikoner i data/ikoner/*.png (100 st: siffror, räknetecken,
former, kroppar, diagram, vardagsföremål). Används i sidopanelen, på korten,
som räknebilder (figuren `ikoner`) och för rätt/fel."""
from __future__ import annotations

from pathlib import Path

MAPP = Path(__file__).resolve().parent.parent / "data" / "ikoner"

# Områdena i Lgr22 och stadierna
OMRADE_IKON = {
    "tal": "kulram", "algebra": "lika-med", "geometri": "triangel",
    "sannolikhet": "tarning", "samband": "linjediagram", "problem": "gloidlampa",
}
STADIUM_IKON = {
    "forskola": "stapeltorn", "lgr22.1-3": "kuber-gula", "lgr22.4-6": "linjal",
    "lgr22.7-9": "koordinatsystem", "gy25.1": "passare", "gy25.2": "gradskiva",
    "gy25.fortsattning": "raket",
}
RATT, FEL, PROGRAM = "ratt", "fel", "kulram"

# Räknebilder som passar att räkna (figuren `ikoner`)
RAKNEBILDER = ["apple", "banan", "apelsin", "paron", "jordgubbe", "morot", "kaka",
               "mynt", "tarning", "strumpor", "stjarna", "hjarta", "raket"]


def sokvag(namn: str) -> Path:
    p = MAPP / f"{namn}.png"
    return p if p.exists() else MAPP / "fragetecken.png"


def alla() -> list[str]:
    return sorted(p.stem for p in MAPP.glob("*.png"))
