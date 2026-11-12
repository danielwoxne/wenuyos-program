"""Anpassning – nästa uppgift väljs efter vad eleven faktiskt tänker fel på.

Tre regler, i den här ordningen:

1. **Träna feltanken.** Har samma feltanke (samma felsvar på samma uppgiftsmall)
   kommit två gånger nyligen, kommer den uppgiftsmallen oftare – med ledtråden synlig
   från början ("Tänk på: …"). Varje rätt svar på mallen räknar ned feltanken, så den
   släpper när eleven har förstått. (Diagnostisk undervisning: Swan; Bell.)
2. **Nivå efter rätt i rad.** Annars: nivå 1 (nästan lösta exempel) tills två rätt i
   rad, sedan nivå 2, sedan nivå 3 – de lösta exemplen tunnas ut (Sweller).
3. **Gå tillbaka ett steg.** Är minst 4 av de 6 senaste svaren fel, och kortet har ett
   förkunskapskort som inte sitter, föreslås det kortet först. Det är ett förslag –
   eleven väljer själv.

Inget här kräver nätet eller ett konto; allt bygger på elevens egen fil.
"""
from __future__ import annotations

import random

from .framsteg import BEHARSKAR, Elev, Kortstatus
from .kort import Kort, Mall

TRANA_FRAN = 2          # så många gånger en feltanke måste ha kommit för att tränas
TRANA_ANDEL = 0.6       # så ofta den mallen väljs när en feltanke tränas
BAKAT_FEL = 4           # så många fel …
BAKAT_AV = 6            # … av så många senaste svar ger förslaget att gå tillbaka


def niva_for(s: Kortstatus) -> int:
    return 1 if s.ratt_i_rad < 2 else (2 if s.ratt_i_rad < 4 else 3)


def feltanke_att_trana(s: Kortstatus) -> tuple[int, str] | None:
    """(mallindex, felsvar-mall) för den feltanke som kommit oftast, om den kommit nog ofta."""
    if not s.feltanke_antal:
        return None
    fid, antal = max(s.feltanke_antal.items(), key=lambda kv: kv[1])
    if antal < TRANA_FRAN:
        return None
    idx, _, mall = fid.partition("|")
    try:
        return int(idx), mall
    except ValueError:
        return None


def valj_mall(kort: Kort, s: Kortstatus, tillatna: list[Mall] | None = None,
              rnd: random.Random | None = None) -> tuple[Mall, bool]:
    """Väljer nästa uppgiftsmall. Returnerar (mall, tranar_feltanke)."""
    rnd = rnd or random
    tillatna = tillatna or kort.mallar
    tr = feltanke_att_trana(s)
    if tr is not None and 0 <= tr[0] < len(kort.mallar):
        m = kort.mallar[tr[0]]
        if m in tillatna and rnd.random() < TRANA_ANDEL:
            return m, True
    niva = niva_for(s)
    val = [m for m in tillatna if m.niva == niva] or tillatna
    return rnd.choice(val), False


def ga_tillbaka(kort: Kort, elev: Elev, korten: dict[str, Kort]) -> Kort | None:
    """Ett förkunskapskort att öva först, om eleven fastnat på det här kortet."""
    s = elev.status(kort.id)
    senaste = s.historik[-BAKAT_AV:]
    if len(senaste) < BAKAT_AV or senaste.count(0) < BAKAT_FEL:
        return None
    for fid in kort.forkunskap:
        if fid in korten and elev.status(fid).lage != BEHARSKAR:
            return korten[fid]
    for fid in kort.forkunskap:                 # alla sitter – föreslå det senast övade att repetera
        if fid in korten:
            return korten[fid]
    return None
