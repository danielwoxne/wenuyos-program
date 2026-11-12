"""Regelkort – programmets minsta byggsten.

Ett kort är en TOML-fil i data/kort/ och motsvarar ett inklistrat kort i en
regelbok: rubrik, "man säger / bild / man skriver"-rader, exempel, *när regeln
gäller* och uppgiftsmallar. Korten laddas här; fonster.py visar dem,
uppgifter.py gör uppgifter av mallarna.

Läroplanen (data/laroplan/) pekar på korten per stadium och område – kortet
självt vet inte vilken årskurs det hör till. Samma kort kan återanvändas
i flera stadier (bråk återkommer i åk 1–3, 4–6 och 7–9 med ökande krav).
"""
from __future__ import annotations

import tomllib
from dataclasses import dataclass, field
from pathlib import Path

DATA = Path(__file__).resolve().parent.parent / "data"

OMRADEN = {
    # Lgr22:s sex områden – samma indelning används på alla stadier
    "tal": "Taluppfattning och tals användning",
    "algebra": "Algebra",
    "geometri": "Geometri",
    "sannolikhet": "Sannolikhet och statistik",
    "samband": "Samband och förändring",
    "problem": "Problemlösning",
}

STADIER = [
    ("forskola", "Förskola"),
    ("lgr22.1-3", "Årskurs 1–3"),
    ("lgr22.4-6", "Årskurs 4–6"),
    ("lgr22.7-9", "Årskurs 7–9"),
    ("gy25.1", "Gymnasiet nivå 1"),
    ("gy25.2", "Gymnasiet nivå 2"),
    ("gy25.fortsattning", "Gymnasiet – fortsättning"),
]


@dataclass
class Rad:
    """En rad på kortet: man säger – bild – man skriver."""
    sager: str = ""
    figur: dict | None = None
    skriver: list[str] = field(default_factory=list)
    kommentar: str = ""


@dataclass
class Exempel:
    rubrik: str = ""
    text: str = ""
    figur: dict | None = None
    steg: list[str] = field(default_factory=list)


@dataclass
class Mall:
    """En uppgiftsmall. {a}, {n} … byts mot slumpade värden i uppgifter.py."""
    fraga: str
    svar: str | list[str]
    var: dict[str, str] = field(default_factory=dict)
    figur: dict | None = None
    form: str = "auto"              # vilken form svaret helst visas i
    feltankar: dict[str, str] = field(default_factory=dict)  # fel svar → förklaring
    ledtrad: str = ""
    niva: int = 1                   # 1 = uttunnat exempel, 2 = vanlig, 3 = svårare
    svarstyp: str = "text"          # text · uttryck · val · klick · tabell · kryssa · para · mala · rita
    alternativ: list[str] = field(default_factory=list)   # för val
    falt: list[str] = field(default_factory=list)         # för tabell
    tolerans: float = 0.0           # godta svar så här nära facit (t.ex. 0.05 = "svara med en decimal")
    oordnad: bool = False           # flera fält där ordningen inte spelar roll (x₁ och x₂)
    steg: list[str] = field(default_factory=list)     # löst exempel med lucka: visas före frågan ("… = ?")
    losning: list[str] = field(default_factory=list)  # så här tänker man: visas efter ett fel svar och i facit
    par: list[list[str]] = field(default_factory=list)  # para: [["3 + 4", "7"], …] – dra linjer mellan paren


@dataclass
class Kort:
    id: str
    titel: str
    omrade: str
    ikon: str = ""                  # en av Daniels lerikoner (data/ikoner)
    bild: str = ""                  # scenbild i data/bilder – verkligheten före staplarna (CRA)
    bildtext: str = ""              # vad man ska se i bilden
    sager: str = ""                 # kortets kärnmening
    galler: str = ""                # när regeln gäller
    galler_inte: str = ""           # när den slutar gälla ("regler som slutar gälla")
    rader: list[Rad] = field(default_factory=list)
    exempel: list[Exempel] = field(default_factory=list)
    mallar: list[Mall] = field(default_factory=list)
    feltankar: dict[str, str] = field(default_factory=dict)
    forkunskap: list[str] = field(default_factory=list)  # kort som bör sitta först
    ateruppta: list[str] = field(default_factory=list)   # kort vars "slutar gälla" tas upp igen här
    kalla: str = ""                 # vilken forskningsprincip kortet bygger på

    @property
    def omrade_namn(self) -> str:
        return OMRADEN.get(self.omrade, self.omrade)


def _rader(lista):
    return [Rad(r.get("sager", ""), r.get("figur"), list(r.get("skriver", [])), r.get("kommentar", ""))
            for r in lista]


def _exempel(lista):
    return [Exempel(e.get("rubrik", ""), e.get("text", ""), e.get("figur"), list(e.get("steg", [])))
            for e in lista]


def _mallar(lista):
    ut = []
    for m in lista:
        svar = m.get("svar", "")          # rita och para har inget svar
        svar = [str(x) for x in svar] if isinstance(svar, list) else str(svar)
        typ = m.get("svarstyp", "val" if m.get("alternativ") else "tabell" if m.get("falt") else "text")
        ut.append(Mall(m["fraga"], svar, dict(m.get("var", {})), m.get("figur"),
                       m.get("form", "auto"), dict(m.get("feltankar", {})), m.get("ledtrad", ""),
                       int(m.get("niva", 2)), typ, [str(a) for a in m.get("alternativ", [])],
                       [str(f) for f in m.get("falt", [])], float(m.get("tolerans", 0)),
                       bool(m.get("oordnad", False)), [str(x) for x in m.get("steg", [])],
                       [str(x) for x in m.get("losning", [])],
                       [[str(a), str(b)] for a, b in m.get("par", [])]))
    return ut


def las_kort(sokvag: Path) -> Kort:
    with open(sokvag, "rb") as f:
        d = tomllib.load(f)
    return Kort(
        id=d.get("id", sokvag.stem), titel=d["titel"], omrade=d.get("omrade", "tal"),
        ikon=d.get("ikon", ""), bild=d.get("bild", ""), bildtext=d.get("bildtext", ""), sager=d.get("sager", ""), galler=d.get("galler", ""), galler_inte=d.get("galler_inte", ""),
        rader=_rader(d.get("rad", [])), exempel=_exempel(d.get("exempel", [])),
        mallar=_mallar(d.get("uppgift", [])), feltankar=dict(d.get("feltankar", {})),
        forkunskap=list(d.get("forkunskap", [])), ateruppta=list(d.get("ateruppta", [])),
        kalla=d.get("kalla", ""),
    )


def alla_kort(mapp: Path = DATA / "kort") -> dict[str, Kort]:
    kort = {}
    for p in sorted(mapp.glob("*.toml")):
        k = las_kort(p)
        kort[k.id] = k
    return kort


@dataclass
class Mal:
    """Ett mål i läroplanen: ett centralt innehåll och de kort som täcker det."""
    id: str
    stadium: str
    omrade: str
    text: str
    kort: list[str]


def las_laroplan(mapp: Path = DATA / "laroplan") -> list[Mal]:
    mal = []
    for p in sorted(mapp.rglob("*.toml")):
        with open(p, "rb") as f:
            d = tomllib.load(f)
        stadium = d["stadium"]
        for m in d.get("mal", []):
            mal.append(Mal(m["id"], stadium, m["omrade"], m["text"], list(m.get("kort", []))))
    return mal
