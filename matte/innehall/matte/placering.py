"""Placeringstest – hittar var eleven är, ett område i taget.

Korten i ett område ligger i läroplanens ordning (åk 1–3 → 4–6 → 7–9).
Testet börjar i mitten, ställer en uppgift på nivå 2, och går uppåt vid rätt
och nedåt vid fel – som att slå upp i en telefonkatalog. Efter högst
log₂(antal) + 1 frågor vet vi första kortet eleven inte behärskar. Allt
före det markeras som *Behärskar* (med repetition inplanerad), så att
repetitionen senare fångar upp sådant som ändå inte satt.

Ingen väljer årskurs. Eleven möter bara "Hur mycket kan du redan?" och
börjar sedan precis där det blir nytt.
"""
from __future__ import annotations

import random
from dataclasses import dataclass, field
from datetime import date, timedelta

from .framsteg import BEHARSKAR, INTERVALL, Elev
from .kort import Kort, Mal, OMRADEN
from .uppgifter import Uppgift, gor_uppgift


def kortordning(laroplan: list[Mal], kort: dict[str, Kort]) -> dict[str, list[str]]:
    """Område → kort i läroplanens ordning (första förekomsten räknas)."""
    stadier = ["forskola", "lgr22.1-3", "lgr22.4-6", "lgr22.7-9", "gy25.1", "gy25.2", "gy25.fortsattning"]
    ut: dict[str, list[str]] = {o: [] for o in OMRADEN}
    for st in stadier:
        for m in laroplan:
            if m.stadium != st:
                continue
            for kid in m.kort:
                if kid in kort and kid not in ut.setdefault(m.omrade, []):
                    ut[m.omrade].append(kid)
    return ut


@dataclass
class Placering:
    omrade: str
    kort_ids: list[str]
    korten: dict[str, Kort]
    lo: int = 0                      # första kort som kan vara "inte behärskat"
    hi: int = field(default=-1)      # sista kort som kan vara "inte behärskat"
    fragor: int = 0
    svar: list[tuple[str, bool]] = field(default_factory=list)

    def __post_init__(self):
        if self.hi < 0:
            self.hi = len(self.kort_ids) - 1

    @property
    def klar(self) -> bool:
        return self.lo > self.hi or not self.kort_ids

    @property
    def start(self) -> int:
        """Index för första kortet eleven ska arbeta med."""
        return self.lo

    def nasta(self) -> tuple[Kort, Uppgift] | None:
        if self.klar:
            return None
        mitt = (self.lo + self.hi) // 2
        k = self.korten[self.kort_ids[mitt]]
        mallar = [m for m in k.mallar if m.niva == 2] or k.mallar
        u = gor_uppgift(k, random.choice(mallar))
        self._aktuell = mitt
        return k, u

    def svara(self, ratt: bool) -> None:
        self.fragor += 1
        self.svar.append((self.kort_ids[self._aktuell], ratt))
        if ratt:
            self.lo = self._aktuell + 1
        else:
            self.hi = self._aktuell - 1

    def tillampa(self, elev: Elev, idag: date | None = None) -> None:
        """Markerar allt före startpunkten som behärskat, resten lämnas."""
        idag = idag or date.today()
        for kid in self.kort_ids[: self.start]:
            s = elev.status(kid)
            if s.lage != BEHARSKAR:
                s.lage = BEHARSKAR; s.steg = 1; s.ratt_i_rad = 0
                s.nasta = (idag + timedelta(days=INTERVALL[1])).isoformat()
            if kid not in elev.regelbok:
                elev.regelbok.append(kid)


def placera_alla(elev: Elev, laroplan: list[Mal], korten: dict[str, Kort]) -> list[Placering]:
    """En Placering per område som har kort."""
    ordning = kortordning(laroplan, korten)
    return [Placering(o, ids, korten) for o, ids in ordning.items() if ids]
