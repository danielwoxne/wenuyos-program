"""Arbetsblad – nya slumpade uppgifter att skriva ut, med facit på en egen sida.

Samma mallar och figurer som i programmet, så bladet ser ut som skärmen. Varje blad
har ett nummer (fröet); samma nummer ger samma uppgifter igen, så läraren kan dela
ut blad 4711 och rätta mot facit 4711.

Uppgifter som på skärmen är klick eller dra blir pappersuppgifter: "ringa in",
"rita brickorna", "pricka in" – figuren är densamma.
"""
from __future__ import annotations

import html
import random

from .kort import Kort
from .regelbok import STIL
from .rita import figur
from .uppgifter import gor_uppgift

_PAPPER = {
    "valj": "Ringa in svaret i bilden.",
    "markera": "Ringa in allt som passar.",
    "klick": "Ringa in och räkna.",
    "farga": "Färglägg.",
    "dra": "Rita brickorna i ramen.",
    "punkt": "Pricka in svaret.",
    "linje": "Rita linjen.",
    "skiss": "Rita gärna först.",
}


def facit_visning(u) -> str:
    """Facit som en människa läser det: "6 st: fat" i stället för "fat,fat,fat,fat,fat,fat"."""
    if u.svarstyp == "markera":
        delar = u.facit_text[0].split(",")
        return f"{len(delar)} st: " + ", ".join(sorted(set(delar)))
    if u.svarstyp == "valj":
        return u.facit_text[0].replace("siffra-", "")
    if u.svarstyp in ("punkt",) and len(u.facit_text) == 2:
        return f"({u.facit_text[0]}, {u.facit_text[1]})"
    if u.svarstyp == "linje":
        return f"k = {u.facit_text[0]}, m = {u.facit_text[1]}"
    return ", ".join(u.facit_text)


def arbetsblad(korten: list[Kort], antal: int = 6, fro: int | None = None, niva: int | None = None,
               titel: str = "Arbetsblad") -> str:
    """HTML för ett arbetsblad med `antal` uppgifter per kort, och facit sist."""
    fro = random.randrange(1000, 9999) if fro is None else fro
    rnd = random.Random(fro)
    uppgifter = []
    for k in korten:
        mallar = [m for m in k.mallar if niva is None or m.niva == niva] or k.mallar
        for i in range(antal):
            m = mallar[i % len(mallar)] if len(mallar) >= antal else rnd.choice(mallar)
            uppgifter.append((k, gor_uppgift(k, m, fro=rnd.randrange(1 << 30))))
    d = [f'<!doctype html><html lang="sv"><head><meta charset="utf-8"><title>{html.escape(titel)} {fro}</title>',
         f"<style>{STIL}"
         " .blad { max-width: 760px; margin: 0 auto; background: white; padding: 20px 28px; border: 1px solid #3a3f44; }"
         " .huvud { display: flex; justify-content: space-between; border-bottom: 1px solid #3a3f44; padding-bottom: 8px; }"
         " .uppg { page-break-inside: avoid; margin: 18px 0; }"
         " .uppg .nr { font-weight: 600; margin-right: 6px; }"
         " .svarsrad { border-bottom: 1px solid #999; height: 26px; width: 60%; margin-top: 8px; }"
         " .papper { color: #5c6670; font-size: 13px; font-style: italic; }"
         " .facit { page-break-before: always; } .facit li { margin: 3px 0; }"
         " .steg { margin: 6px 0 0 18px; padding: 6px 10px; border: 1px dashed #999; font-size: 16px; display: inline-block; }"
         " .losning { color: #5c6670; font-size: 13px; }"
         " @media print { .blad { border: none; } }</style></head><body>",
         '<div class="blad">',
         f'<div class="huvud"><b>{html.escape(titel)}</b><span>Namn: ____________________ &nbsp; Datum: ________</span></div>',
         f'<div class="papper">Blad {fro} · {", ".join(html.escape(k.titel) for k in korten)}</div>']
    for nr, (k, u) in enumerate(uppgifter, 1):
        d.append('<div class="uppg">')
        d.append(f'<div><span class="nr">{nr}.</span>{html.escape(u.fraga)}</div>')
        if u.steg:
            d.append('<div class="steg">' + "<br>".join(html.escape(x) for x in u.steg) + "</div>")
        if u.svarstyp == "val" and u.alternativ:
            d.append('<div class="papper">Ringa in: ' + " &nbsp; ".join(f"☐ {html.escape(a)}" for a in u.alternativ) + "</div>")
        elif u.svarstyp in _PAPPER:
            d.append(f'<div class="papper">{_PAPPER[u.svarstyp]}</div>')
        if u.figur:
            besk = dict(u.figur)
            if besk.get("typ") == "tioram":
                besk["fyllda"] = besk.get("fyllda", 0)
            try:
                d.append(figur(besk).svg())
            except Exception:                               # noqa: BLE001 – en trasig figur ska inte stoppa bladet
                pass
        if u.svarstyp == "tabell" or (u.svarstyp in ("punkt", "linje") and len(u.facit) > 1 and u.falt):
            d.append("<div>" + " &nbsp; ".join(f"{html.escape(f)} = ______" for f in u.falt) + "</div>")
        elif u.svarstyp not in ("valj", "markera", "klick", "farga", "dra", "punkt", "linje", "val"):
            d.append('<div class="svarsrad"></div>')
        d.append("</div>")
    d.append('<div class="facit"><b>Facit – blad ' + str(fro) + "</b><ol>")
    for k, u in uppgifter:
        los = f'<div class="losning">{html.escape(" · ".join(u.losning))}</div>' if u.losning else ""
        d.append(f"<li>{html.escape(facit_visning(u))}{los}</li>")
    d.append("</ol></div></div></body></html>")
    return "\n".join(d)
