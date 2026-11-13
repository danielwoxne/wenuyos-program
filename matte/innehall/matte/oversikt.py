"""Översikt för läraren eller föräldern – vad sitter, och vilka feltankar återkommer?

Läser elevernas filer på den här datorn (inget nät, inga konton) och sammanställer:
  * per elev: hur många kort som behärskas/övas, vad som ska repeteras i dag,
    och de vanligaste feltankarna med programmets egen förklaring;
  * för gruppen: vilka feltankar som är vanligast – det är dem en genomgång
    på tavlan gör mest nytta för.

`matte oversikt [fil.html]` skriver samma sak som en utskrivbar sida.
"""
from __future__ import annotations

import html
import re
from dataclasses import dataclass, field

from .framsteg import Elev, elever
from .kort import Kort, alla_kort

_PLATS = re.compile(r"\{[^{}]+\}")


@dataclass
class Feltanke:
    kort_id: str
    kort_titel: str
    text: str            # förklaringen, med platshållarna ersatta av "…"
    antal: int
    elever: list[str] = field(default_factory=list)


@dataclass
class Elevrad:
    namn: str
    beharskar: int
    ovar: int
    repetera: int
    svar: int
    andel_ratt: float
    feltankar: list[Feltanke]


def _feltanke_text(kort: Kort | None, fid: str) -> str:
    if kort is None:
        return fid
    idx, _, mall = fid.partition("|")
    try:
        m = kort.mallar[int(idx)]
    except (ValueError, IndexError):
        return mall
    text = m.feltankar.get(mall) or kort.feltankar.get(mall) or mall
    return _PLATS.sub("…", text)


def elevrad(e: Elev, korten: dict[str, Kort]) -> Elevrad:
    n = e.sammanfattning()
    svar = sum(s.antal for s in e.kort.values())
    ratt = sum(s.antal_ratt for s in e.kort.values())
    ft: list[Feltanke] = []
    for kid, s in e.kort.items():
        for fid, antal in s.feltanke_totalt.items():
            k = korten.get(kid)
            ft.append(Feltanke(kid, k.titel if k else kid, _feltanke_text(k, fid), antal, [e.visningsnamn or e.namn]))
    ft.sort(key=lambda f: -f.antal)
    return Elevrad(e.visningsnamn or e.namn, n["beharskar"], n["ovar"], len(e.att_repetera()), svar,
                   (ratt / svar) if svar else 0.0, ft[:5])


def sammanstall(namn: list[str] | None = None) -> tuple[list[Elevrad], list[Feltanke]]:
    """(en rad per elev, gruppens vanligaste feltankar)."""
    korten = alla_kort()
    rader = []
    grupp: dict[tuple[str, str], Feltanke] = {}
    for n in (namn if namn is not None else elever()):
        e = Elev.ladda(n)
        rad = elevrad(e, korten)
        rader.append(rad)
        for kid, s in e.kort.items():
            for fid, antal in s.feltanke_totalt.items():
                nyckel = (kid, fid)
                k = korten.get(kid)
                f = grupp.setdefault(nyckel, Feltanke(kid, k.titel if k else kid, _feltanke_text(k, fid), 0))
                f.antal += antal
                if rad.namn not in f.elever:
                    f.elever.append(rad.namn)
    vanligast = sorted(grupp.values(), key=lambda f: (-len(f.elever), -f.antal))[:10]
    return rader, vanligast


def html_rapport(namn: list[str] | None = None) -> str:
    from .regelbok import STIL
    rader, grupp = sammanstall(namn)
    d = ['<!doctype html><html lang="sv"><head><meta charset="utf-8"><title>Översikt</title>',
         f"<style>{STIL} table.ov td, table.ov th {{ border: 1px solid #3a3f44; padding: 4px 8px; }}"
         " table.ov { border-collapse: collapse; margin: 12px 20px; } .ov th { background: #fdeedc; }</style>",
         '</head><body><section class="kort"><h1>Översikt</h1>',
         '<table class="ov"><tr><th>Elev</th><th>● Behärskar</th><th>◐ Övar</th><th>Repetera i dag</th><th>Svar</th><th>Rätt</th></tr>']
    for r in rader:
        d.append(f"<tr><td>{html.escape(r.namn)}</td><td>{r.beharskar}</td><td>{r.ovar}</td><td>{r.repetera}</td>"
                 f"<td>{r.svar}</td><td>{r.andel_ratt:.0%}</td></tr>")
    d.append("</table>")
    if grupp:
        d.append('<div class="exempel"><h2>Vanligaste feltankarna i gruppen</h2><ol>')
        for f in grupp:
            d.append(f"<li><b>{html.escape(f.kort_titel)}</b> – {len(f.elever)} elev(er), {f.antal} gånger:"
                     f"<br>{html.escape(f.text)}</li>")
        d.append("</ol></div>")
    for r in rader:
        if r.feltankar:
            d.append(f'<div class="exempel"><h2>{html.escape(r.namn)}</h2><ul>')
            d.extend(f"<li><b>{html.escape(f.kort_titel)}</b> ({f.antal}): {html.escape(f.text)}</li>" for f in r.feltankar)
            d.append("</ul></div>")
    d.append('<div class="kalla">Allt i översikten kommer från filerna på den här datorn. Inget skickas någonstans.</div>')
    d.append("</section></body></html>")
    return "\n".join(d)
