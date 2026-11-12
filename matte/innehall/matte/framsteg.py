"""Framsteg – vad eleven behärskar, och när det är dags att repetera.

Tre lägen per kort:  0 Lär  ·  1 Övar  ·  2 Behärskar.
Behärskar man ett kort repeteras det med växande mellanrum (1, 3, 7, 14, 30
dagar …). Fel svar på en repetition kortar nästa mellanrum – inte tillbaka
till noll. Det är den enkla formen av "hämta ur minnet, glesare med tiden".

Sparas som JSON i GLib.get_user_data_dir()/se.wenuyos.app.matte/<elev>.json
(~/.local/share/se.wenuyos.app.matte) – aldrig i programmappen, som byts ut vid
uppdatering. Skrivs säkert: först till en temporär fil, sedan byte av namn.
"""
from __future__ import annotations

import json
import os
from dataclasses import asdict, dataclass, field
from datetime import date, timedelta
from pathlib import Path

LAR, OVAR, BEHARSKAR = 0, 1, 2
INTERVALL = [1, 3, 7, 14, 30, 60, 120]
KRAV_RATT_I_RAD = 4          # så många rätt i rad flyttar Övar → Behärskar


APP_ID = "se.wenuyos.app.matte"


def _datamapp() -> str:
    """Samma som GLib.get_user_data_dir(), men utan att kräva gi (terminalläget och testerna)."""
    try:
        from gi.repository import GLib
        return GLib.get_user_data_dir()
    except Exception:  # noqa: BLE001
        return os.environ.get("XDG_DATA_HOME") or os.path.expanduser("~/.local/share")


def _mapp() -> Path:
    bas = os.environ.get("XDG_DATA_HOME") or _datamapp()
    m = Path(bas) / APP_ID
    gammal = Path(bas) / "matte"                 # 0.1–0.3.0 sparade här – flytta över en gång
    if not m.exists() and gammal.is_dir() and any(gammal.glob("*.json")):
        try:
            gammal.rename(m)
        except OSError:
            pass
    return m


def filnamn(namn: str) -> str:
    """Ett elevnamn som filnamn: "Ali S." → "ali-s". Bokstäver (även åäö) och siffror behålls."""
    import re
    ren = re.sub(r"[^\wåäöÅÄÖ]+", "-", namn.strip().casefold(), flags=re.UNICODE).strip("-")
    return ren or "elev"


def elever() -> list[str]:
    """Alla elever som har sparat något på den här datorn (filnamnen)."""
    m = _mapp()
    return sorted(p.stem for p in m.glob("*.json")) if m.exists() else []


def senaste_elev() -> str:
    p = _mapp() / "senaste"
    try:
        namn = p.read_text(encoding="utf-8").strip()
        return namn if namn else "elev"
    except OSError:
        return "elev"


def satt_senaste_elev(namn: str) -> None:
    m = _mapp(); m.mkdir(parents=True, exist_ok=True)
    (m / "senaste").write_text(namn, encoding="utf-8")


@dataclass
class Kortstatus:
    lage: int = LAR
    ratt_i_rad: int = 0
    steg: int = 0                # index i INTERVALL
    nasta: str = ""              # ISO-datum för nästa repetition
    antal: int = 0
    antal_ratt: int = 0
    senast: str = ""
    feltankar: list[str] = field(default_factory=list)  # (äldre format, läses men skrivs inte längre)
    historik: list[int] = field(default_factory=list)   # de senaste svaren, 1 = rätt, 0 = fel (högst 10)
    feltanke_antal: dict[str, int] = field(default_factory=dict)   # feltanke-id → hur ofta den kommit nyligen
    feltanke_totalt: dict[str, int] = field(default_factory=dict)  # feltanke-id → hur ofta totalt (för läraren)


@dataclass
class Elev:
    namn: str = "elev"                                  # filnamnet (se filnamn())
    kort: dict[str, Kortstatus] = field(default_factory=dict)
    regelbok: list[str] = field(default_factory=list)   # korten i den ordning de mötts
    placerad: bool = False                              # har gjort placeringstestet
    lekläge: bool = False                               # förskola/lågstadiet: stort, uppläst, utan text
    visningsnamn: str = ""                              # som eleven skrev det: "Ali S."

    # -- läsa/spara --------------------------------------------------------
    @classmethod
    def ladda(cls, namn: str = "elev") -> "Elev":
        p = _mapp() / f"{namn}.json"
        if not p.exists():
            return cls(namn)
        with open(p, encoding="utf-8") as f:
            d = json.load(f)
        e = cls(namn, {k: Kortstatus(**v) for k, v in d.get("kort", {}).items()},
                list(d.get("regelbok", [])), bool(d.get("placerad", False)), bool(d.get("lekläge", False)),
                d.get("visningsnamn", "") or namn)
        return e

    def spara(self) -> None:
        m = _mapp(); m.mkdir(parents=True, exist_ok=True)
        p = m / f"{self.namn}.json"
        tmp = p.with_suffix(".json.tmp")
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump({"namn": self.namn, "kort": {k: asdict(v) for k, v in self.kort.items()},
                       "regelbok": self.regelbok, "placerad": self.placerad,
                       "lekläge": self.lekläge, "visningsnamn": self.visningsnamn or self.namn}, f, ensure_ascii=False, indent=1)
        os.replace(tmp, p)

    # -- händelser ---------------------------------------------------------
    def status(self, kort_id: str) -> Kortstatus:
        return self.kort.setdefault(kort_id, Kortstatus())

    def sett_kort(self, kort_id: str) -> None:
        """Kortet visades – det hamnar i elevens regelbok."""
        if kort_id not in self.regelbok:
            self.regelbok.append(kort_id)
        s = self.status(kort_id)
        if s.lage == LAR:
            s.lage = OVAR

    def svarade(self, kort_id: str, ratt: bool, feltanke: str | None = None,
                idag: date | None = None, mall_index: int | None = None) -> None:
        """`feltanke` = feltanke-id från uppgifter.feltanke_id(). Ett rätt svar på samma
        uppgiftsmall räknar ned dess feltankar – de "släpper" när eleven klarar dem."""
        idag = idag or date.today()
        s = self.status(kort_id)
        s.antal += 1; s.senast = idag.isoformat()
        s.historik = (s.historik + [1 if ratt else 0])[-10:]
        if feltanke and not ratt:
            s.feltanke_antal[feltanke] = s.feltanke_antal.get(feltanke, 0) + 1
            s.feltanke_totalt[feltanke] = s.feltanke_totalt.get(feltanke, 0) + 1
        if ratt and mall_index is not None:
            for fid in list(s.feltanke_antal):
                if fid.startswith(f"{mall_index}|"):
                    s.feltanke_antal[fid] -= 1
                    if s.feltanke_antal[fid] <= 0:
                        del s.feltanke_antal[fid]
        if ratt:
            s.antal_ratt += 1; s.ratt_i_rad += 1
            if s.lage == BEHARSKAR:
                s.steg = min(s.steg + 1, len(INTERVALL) - 1)
            elif s.ratt_i_rad >= KRAV_RATT_I_RAD:
                s.lage = BEHARSKAR; s.steg = 0
            s.nasta = (idag + timedelta(days=INTERVALL[s.steg])).isoformat() if s.lage == BEHARSKAR else ""
        else:
            s.ratt_i_rad = 0
            if s.lage == BEHARSKAR:
                s.steg = max(s.steg - 2, 0)
                s.nasta = (idag + timedelta(days=INTERVALL[s.steg])).isoformat()

    # -- urval -------------------------------------------------------------
    def att_repetera(self, idag: date | None = None) -> list[str]:
        idag = idag or date.today()
        return [k for k, s in self.kort.items()
                if s.lage == BEHARSKAR and s.nasta and date.fromisoformat(s.nasta) <= idag]

    def nasta_kort(self, ordning: list[str], idag: date | None = None) -> str | None:
        """Repetitioner först, annars nästa kort i ordningen som inte behärskas."""
        rep = self.att_repetera(idag)
        if rep:
            return rep[0]
        for k in ordning:
            if self.status(k).lage != BEHARSKAR:
                return k
        return None

    def sammanfattning(self) -> dict[str, int]:
        n = {"lar": 0, "ovar": 0, "beharskar": 0}
        for s in self.kort.values():
            n[("lar", "ovar", "beharskar")[s.lage]] += 1
        return n
