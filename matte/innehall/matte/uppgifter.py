"""Uppgifter ur mallar – och bedömning av svar.

En mall som
    fraga = "Vilket bråk visar bilden?"
    figur = { typ = "stapel", delar = "{n}", fyllda = "{k}" }
    svar  = "{k}/{n}"
    var   = { n = "2..10", k = "1..n-1" }
blir en konkret uppgift när variablerna slumpas. Samma frö ger samma uppgift,
så en uppgift kan visas igen (repetition) eller återskapas ur en felrapport.

Svarstyper (`svarstyp` i mallen):
  text    – eleven skriver ett tal (standard). Alla former av samma tal godtas.
  uttryck – eleven skriver ett uttryck med variabler; likvärdiga uttryck godtas.
  val     – flerval: `alternativ = ["1/8", "1/5"]`, svaret är ett av dem.
  klick   – eleven klickar i figuren (staplar, prickar, hundraruta); svaret är antalet.
  farga   – samma som klick ("färga 3/4 av stapeln"), ordet säger vad eleven ska göra.
  tabell  – flera fält: `falt = ["Procent", "Decimal"]`, `svar = ["{k*10} %", "0,{k}"]`.
  punkt   – eleven prickar in ett tal på tallinjen (`svar = "0,7"`) eller en punkt i
            koordinatsystemet (`svar = "{x};{y}"`). Klicket snäpps till rutnätet.
  linje   – eleven klickar två gitterpunkter; svaret är linjens k och m (`svar = "{k};{m}"`).
  skiss   – en ritruta att kladda i (rättas inte) plus ett vanligt textsvar.
  valj    – klicka på EN sak i en scen (målning) eller ikonrad; svaret är dess namn
            (`svar = "mitten"`, namnen står i figurens `namn`).
  markera – klicka på ALLA rätt saker; rättas på vilka, inte bara hur många
            (`svar = "gul,gul,gul"` – ordningen spelar ingen roll).
  kryssa  – kryssrutor, flera kan vara rätt: `alternativ = [...]`, `svar = ["{a}", "{b}"]`
            (eller "a | b"). Feltankar per fel ruta: `feltankar = { "{a+1}" = "…" }`.
            En sak kan vara en form eller ikon: "form:triangel", "ikon:apple", "prickar:4".
  para    – dra linjer mellan paren: `par = [["3 + 4", "7"], ["2 + 6", "8"]]`. Vänster och
            höger blandas. Feltanke för en fel linje: `feltankar = { "3 + 4 → 8" = "…" }`.
  mala    – måla ytor med färgpennor. Figuren `malrutor` (`objekt = "1|2|3"` eller lista,
            `kolumner`), en `scen` med klickytor, eller en stapel/hundraruta/prickar.
            `svar = "röd|blå||röd"` = färgen för varje yta ("" = omålad), eller
            `svar = "blå:1|röd:2"` = hur många av varje färg (vilka spelar ingen roll).
            Pennorna: figurens `farger`, annars färgerna i facit.
  rita    – fri rityta (rättas inte). Sparas per elev och syns i lärarens översikt.
            En figur (t.ex. `rutpapper` eller ett koordinatsystem) blir bakgrund.

Två fält gör uppgiften till ett löst exempel (Sweller, "faded worked examples"):
  steg    = ["7 + 5 = 7 + 3 + 2", "= 10 + 2", "= ?"]  – stegen visas före frågan, eleven
            fyller i det som saknas. Nivå 1 lämnar ett steg, nivå 2 fler.
  losning = ["…", "…"]  – så här tänker man. Visas när svaret blev fel och i facit.

Variationsteori i praktiken: `var` låter kortet styra exakt vad som varierar.
"""
from __future__ import annotations

import math
import random
import re
from dataclasses import dataclass, field
from fractions import Fraction

from .algebra import Algebrafel, skriv, tolka_uttryck
from .kort import Kort, Mall
from .uttryck import Uttrycksfel, formatera, tolka

_PLATS = re.compile(r"\{([^{}]+)\}")
_BOKSTAV = re.compile(r"[a-zA-Z]")


@dataclass
class Uppgift:
    kort_id: str
    fraga: str
    facit: list                 # ett värde per fält (Fraction, eller str för uttryck/ord)
    facit_text: list[str]
    figur: dict | None
    ledtrad: str
    feltankar: dict             # värde → förklaring (bara första fältet)
    varden: dict[str, int]
    fro: int
    svarstyp: str
    alternativ: list[str]
    falt: list[str]
    mall: Mall = field(repr=False)
    mall_index: int = 0                       # vilken mall på kortet (för anpassningen)
    feltanke_mall: dict = field(default_factory=dict)   # förklaringstext → feltankens mall (id)
    steg: list[str] = field(default_factory=list)       # löst exempel med lucka (före frågan)
    losning: list[str] = field(default_factory=list)    # så här tänker man (efter fel svar, i facit)
    facit_svar: list[str] = field(default_factory=list) # kryssa/para/mala: facit som elevens svar (en sträng per sak)


def feltanke_id(u: "Uppgift", b: "Bedomning") -> str | None:
    """Ett stabilt id för den feltanke eleven gjorde: "<mall>|<felsvar-mall>". Samma
    tankefel får samma id oavsett vilka tal som slumpades – så kan det räknas."""
    if not b.feltanke:
        return None
    mall = u.feltanke_mall.get(b.text)
    return f"{u.mall_index}|{mall}" if mall else None


# ---- slumpning ---------------------------------------------------------------

def _slumpa_varden(var: dict[str, str], rnd: random.Random) -> dict[str, int]:
    """Tolkar "2..10", "1..n-1", "2..10 != a", "[10,100,1000]" i den ordning de står."""
    v: dict[str, int] = {}
    for namn, regel in var.items():
        regel = str(regel).strip()
        if regel.startswith("["):
            val = [int(x) for x in regel.strip("[]").split(",")]
            v[namn] = rnd.choice(val)
            continue
        regel, _, inte = regel.partition("!=")
        lo, hi = regel.split("..")
        lo_v = int(eval(lo, {}, v)); hi_v = int(eval(hi, {}, v))  # bara enkla uttryck i n, k …
        val = list(range(lo_v, hi_v + 1)) or [lo_v]
        if inte.strip():
            bort = int(eval(inte.strip(), {}, v))
            val = [x for x in val if x != bort] or val
        v[namn] = rnd.choice(val)
    return v


def _tecken(n) -> str:
    """t(n) i en mall: "+ 3" eller "− 3" – så att "x + −3" aldrig syns."""
    n = int(n) if float(n).is_integer() else n
    skrivet = str(abs(n)).replace(".", ",")          # svenskt decimaltecken
    return f"+ {skrivet}" if n >= 0 else f"− {skrivet}"


def _tusental(n) -> str:
    """ts(n) i en mall: 56000000 → "56 000 000" (svenskt skrivsätt)."""
    return f"{int(n):,}".replace(",", " ")


MISSLYCKADE: list[str] = []      # platshållare som inte gick att räkna ut (läses av `matte check`)

_MALLHJALP = {"t": _tecken, "ts": _tusental, "max": max, "min": min, "abs": abs, "sorted": sorted,
              "round": round, "int": int, "str": str, "len": len, "range": range,
              # gymnasiet: grader, inte radianer – samma som i svaren (uttryck.py)
              "sqrt": math.sqrt, "lg": math.log10,
              "sin": lambda v: math.sin(math.radians(v)), "cos": lambda v: math.cos(math.radians(v)),
              "tan": lambda v: math.tan(math.radians(v)), "atand": lambda q: math.degrees(math.atan(q)),
              "asind": lambda q: math.degrees(math.asin(q)), "acosd": lambda q: math.degrees(math.acos(q))}


def _rakna(uttryck: str, varden: dict[str, int]):
    """{n}, {k+1}, {a*b}, {max(a,b)}, {t(m)}, {ts(n)} – små uttryck i variablerna."""
    try:   # variablerna som globala namn – då når även listuttryck ("… for i in range(n)") dem
        r = eval(uttryck, {"__builtins__": {}, **_MALLHJALP, **varden})
    except Exception:
        MISSLYCKADE.append(uttryck)
        return uttryck
    if isinstance(r, float) and r.is_integer():
        return int(r)
    if isinstance(r, float):
        return (f"{r:.6f}".rstrip("0").rstrip(".")).replace(".", ",")
    return r


def _fyll(text, varden: dict[str, int]):
    if isinstance(text, dict):
        return {k: _fyll(x, varden) for k, x in text.items()}
    if isinstance(text, list):
        return [_fyll(x, varden) for x in text]
    if not isinstance(text, str):
        return text
    ut = _PLATS.sub(lambda m: str(_rakna(m.group(1), varden)), text)
    if ut.lstrip("-").isdigit():
        return int(ut)
    return ut


# ---- värden ------------------------------------------------------------------

def _varde(s: str):
    """Ett svar → jämförbart värde: Fraction, polynom (dict) eller ren text."""
    s = str(s).strip()
    if len(s) >= 2 and s.replace(" ", "").isalpha():
        return s.casefold()                        # ett ord (ja, större, triangel) – inte x·y
    try:
        return tolka(s)
    except Uttrycksfel:
        pass
    try:
        return tolka_uttryck(s)
    except Algebrafel:
        pass
    return s.casefold()


def _mangd(text: str) -> str:
    """"gul, blå,gul" → "blå,gul,gul" – markerade saker utan ordning."""
    delar = [d.strip().casefold() for d in str(text).split(",") if d.strip()]
    return ",".join(sorted(delar))


def _avrundat(v: Fraction, tolerans: float) -> str:
    """Facit för en uppgift med tolerans: avrundat till så många decimaler toleransen kräver."""
    dec = 0
    while dec < 6 and 0.5 * 10 ** -dec > tolerans + 1e-12:
        dec += 1
    return f"{float(v):.{dec}f}".replace(".", ",")


def _facit_text(skrivet: str, facit, form: str) -> str:
    """Form "brak" behåller bråket som mallen skrev det (4/8 visas som 4/8, inte 1/2);
    "brak-enklast" förkortar. Uttryck skrivs på skolans sätt. Ord som de är."""
    skrivet = skrivet.strip()
    if form == "text":
        return skrivet
    if isinstance(facit, dict):
        return skriv(facit)
    if isinstance(facit, str):
        return skrivet
    if form == "brak" and re.fullmatch(r"-?\d+/\d+", skrivet):
        return skrivet
    if form == "auto" and re.fullmatch(r"-?[\d ]+([.,]\d+)?( ?%)?", skrivet):
        return skrivet.replace(".", ",")          # mallen skrev det så – behåll
    return formatera(facit, "brak" if form == "brak-enklast" else form)


def gor_uppgift(kort: Kort, mall: Mall, fro: int | None = None) -> Uppgift:
    fro = random.randrange(1 << 30) if fro is None else fro
    rnd = random.Random(fro)
    varden = _slumpa_varden(mall.var, rnd)
    if mall.svarstyp in NYA_TYPER:
        return _gor_ny(kort, mall, fro, rnd, varden)
    svar_lista = mall.svar if isinstance(mall.svar, list) else [mall.svar]
    if mall.svarstyp in ("punkt", "linje") and len(svar_lista) == 1 and ";" in svar_lista[0]:
        svar_lista = svar_lista[0].split(";")           # "{x};{y}" → två fält
    facit_s = [str(_fyll(s, varden)) for s in svar_lista]
    if mall.svarstyp == "markera":
        facit_s = [_mangd(facit_s[0])]
    facit = [s.strip().casefold() if mall.svarstyp in ("valj", "markera") else _varde(s) for s in facit_s]
    feltankar = {}
    feltanke_mall = {}
    for felsvar, forklaring in {**kort.feltankar, **mall.feltankar}.items():
        feltanke_mall[str(_fyll(forklaring, varden))] = felsvar
        fylld = str(_fyll(felsvar, varden))
        if mall.svarstyp == "markera":
            feltankar[_mangd(fylld)] = _fyll(forklaring, varden); continue
        if mall.svarstyp == "valj":
            feltankar[fylld.strip().casefold()] = _fyll(forklaring, varden); continue
        if ";" in fylld:                     # flera fält (punkt, linje): hela svaret som tuppel
            v = tuple(_varde(d) for d in fylld.split(";"))
        else:
            v = _varde(fylld)
            if isinstance(v, dict):
                v = skriv(v)                 # polynom är inte hashbara – använd normalformen
        feltankar[v] = _fyll(forklaring, varden)
    alternativ = [str(_fyll(a, varden)) for a in mall.alternativ]
    if alternativ:
        rnd.shuffle(alternativ)
    return Uppgift(
        kort_id=kort.id, fraga=str(_fyll(mall.fraga, varden)), facit=facit,
        facit_text=[s.strip() if mall.svarstyp in ("valj", "markera")
                    else _avrundat(f, mall.tolerans) if (mall.tolerans and isinstance(f, Fraction))
                    else _facit_text(s, f, mall.form)
                    for s, f in zip(facit_s, facit)],
        figur=_fyll(mall.figur, varden) if mall.figur else None,
        ledtrad=str(_fyll(mall.ledtrad, varden)), feltankar=feltankar, varden=varden, fro=fro,
        svarstyp=mall.svarstyp, alternativ=alternativ, falt=[str(_fyll(f, varden)) for f in mall.falt], mall=mall,
        mall_index=kort.mallar.index(mall) if mall in kort.mallar else 0, feltanke_mall=feltanke_mall,
        steg=[str(_fyll(x, varden)) for x in mall.steg], losning=[str(_fyll(x, varden)) for x in mall.losning],
    )


# ---- kryssa, para, måla, rita ----------------------------------------------------
# Svaret är en lista med en sträng per sak: kryssa = de ikryssade alternativen, para = den
# högra saken för varje vänster (i vänsterordning), mala = färgen för varje yta.
# Nycklarna nedan räknas likadant i C++ (src/Bedom.cpp).

NYA_TYPER = ("kryssa", "para", "mala", "rita")
FARGER = ("röd", "blå", "gul", "grön", "orange", "lila")
MALBARA = ("malrutor", "scen", "stapel", "staende", "tiostaplar", "hundraruta", "prickar", "ikoner")


def norm(s) -> str:
    """Som likvardiga_text: utan mellanslag, komma = punkt, gemener."""
    return str(s).replace(" ", "").replace(",", ".").casefold()


def _delar(x, behall_tomma=False) -> list[str]:
    """En lista, eller "a | b | c" → ["a", "b", "c"]."""
    if isinstance(x, list):
        d = [str(a).strip() for a in x]
    else:
        d = [a.strip() for a in str(x).split("|")]
    return d if behall_tomma else [a for a in d if a]


def _rakning(farger) -> str:
    """["röd", "blå", "röd", ""] eller "röd:2|blå:1" → "c:blå:1|röd:2"."""
    n: dict[str, int] = {}
    for f in farger:
        f = f.strip().casefold()
        if not f:
            continue
        if ":" in f:
            namn, _, antal = f.partition(":")
            n[namn.strip()] = n.get(namn.strip(), 0) + int(antal)
        else:
            n[f] = n.get(f, 0) + 1
    return "c:" + "|".join(sorted(f"{k}:{v}" for k, v in n.items() if v > 0))


def ny_nyckel(typ: str, svar: list[str], facit0: str = "") -> str:
    if typ == "kryssa":
        return "k:" + "|".join(sorted({norm(s) for s in svar if str(s).strip()}))
    if typ == "para":
        return "q:" + "|".join(norm(s) for s in svar)
    if typ == "mala":
        if facit0.startswith("c:"):
            return _rakning(svar)
        return "m:" + "|".join(str(s).strip().casefold() for s in svar)
    return ""


def ny_felnyckel(typ: str, fylld: str) -> str:
    """Feltankens felsvar (ifyllt) → nyckel. kryssa: en fel ruta. para: "vänster → höger"."""
    if typ == "kryssa":
        return "x:" + norm(fylld)
    if typ == "para":
        v, _, h = fylld.partition("→")
        return "x:" + norm(v) + "→" + norm(h)
    if typ == "mala":
        d = _delar(fylld, behall_tomma=True)
        if d and all(":" in x for x in d if x):
            return _rakning(d)
        return "m:" + "|".join(x.casefold() for x in d)
    return ""


def _gor_ny(kort: Kort, mall: Mall, fro: int, rnd: random.Random, varden: dict) -> Uppgift:
    fyll = lambda x: str(_fyll(x, varden))
    figur = _fyll(mall.figur, varden) if mall.figur else None
    if figur and isinstance(figur.get("objekt"), str):
        figur["objekt"] = _delar(figur["objekt"], behall_tomma=True)
    alternativ, falt, facit_svar, facit, facit_text = [], [], [], [], []
    if mall.svarstyp == "kryssa":
        alternativ = [fyll(a) for a in mall.alternativ]
        svar = [fyll(x) for x in mall.svar] if isinstance(mall.svar, list) else _delar(fyll(mall.svar))
        facit_svar = svar
        facit = [ny_nyckel("kryssa", svar)]
        facit_text = [" · ".join(svar)]
        rnd.shuffle(alternativ)
    elif mall.svarstyp == "para":
        par = [(fyll(a), fyll(b)) for a, b in mall.par]
        rnd.shuffle(par)
        falt = [a for a, _ in par]
        facit_svar = [b for _, b in par]
        alternativ = list(facit_svar)
        while len(alternativ) > 1 and alternativ == facit_svar:   # höger får aldrig stå i samma ordning
            rnd.shuffle(alternativ)
        facit = [ny_nyckel("para", facit_svar)]
        facit_text = [" · ".join(f"{a} – {b}" for a, b in par)]
    elif mall.svarstyp == "mala":
        d = _delar([fyll(x) for x in mall.svar] if isinstance(mall.svar, list) else fyll(mall.svar), behall_tomma=True)
        if d and all(":" in x for x in d if x):        # hur många av varje färg
            facit = [_rakning(d)]
            for x in d:
                if x:
                    namn, _, antal = x.partition(":")
                    facit_svar += [namn.strip().casefold()] * int(antal)
            facit_text = [", ".join(f"{x.split(':')[1].strip()} {x.split(':')[0].strip()}" for x in d if x)]
        else:
            facit_svar = [x.casefold() for x in d]
            facit = [ny_nyckel("mala", facit_svar)]
            namn = figur.get("objekt") or figur.get("namn") or [] if figur else []
            facit_text = [", ".join(f"{namn[i] if i < len(namn) else i + 1}: {f}" for i, f in enumerate(facit_svar) if f)]
        if figur is not None and not figur.get("farger"):
            figur["farger"] = [f for f in FARGER if f in {x.split(":")[0].strip().casefold() for x in d if x}]
    feltankar, feltanke_mall = {}, {}
    for felsvar, forklaring in mall.feltankar.items():   # bara mallens egna – kortets gäller tal
        text = fyll(forklaring)
        feltanke_mall[text] = felsvar
        feltankar[ny_felnyckel(mall.svarstyp, fyll(felsvar))] = text
    return Uppgift(
        kort_id=kort.id, fraga=fyll(mall.fraga), facit=facit, facit_text=facit_text, figur=figur,
        ledtrad=fyll(mall.ledtrad), feltankar=feltankar, varden=varden, fro=fro, svarstyp=mall.svarstyp,
        alternativ=alternativ, falt=falt, mall=mall,
        mall_index=kort.mallar.index(mall) if mall in kort.mallar else 0, feltanke_mall=feltanke_mall,
        steg=[fyll(x) for x in mall.steg], losning=[fyll(x) for x in mall.losning], facit_svar=facit_svar)


def _bedom_ny(u: Uppgift, svar: list[str]) -> Bedomning:
    """Samma regler och texter som src/Bedom.cpp."""
    if u.svarstyp == "rita":
        return Bedomning(True, "Sparad.")
    fore = (u.ledtrad + " ") if u.ledtrad else ""
    if u.svarstyp == "kryssa":
        valda = [s for s in svar if str(s).strip()]
        if not valda:
            return Bedomning(False, "Kryssa i minst en ruta.", otolkbart=True)
        if ny_nyckel("kryssa", valda) == u.facit[0]:
            return Bedomning(True, "Rätt.")
        ratt = {norm(x) for x in u.facit_svar}
        fel = [i for i, a in enumerate(u.alternativ) if a in valda and norm(a) not in ratt]
        for i in fel:
            k = "x:" + norm(u.alternativ[i])
            if k in u.feltankar:
                return Bedomning(False, u.feltankar[k], feltanke=True, fel_falt=fel)
        if fel:
            return Bedomning(False, fore + ("Ett kryss är fel." if len(fel) == 1 else f"{len(fel)} kryss är fel."), fel_falt=fel)
        hittade = len({norm(s) for s in valda} & ratt)
        return Bedomning(False, fore + f"Det finns fler som stämmer – du har hittat {hittade} av {len(ratt)}.")
    if u.svarstyp == "para":
        if len(svar) != len(u.falt) or any(not str(s).strip() for s in svar):
            return Bedomning(False, "Dra en linje från varje sak till vänster.", otolkbart=True)
        if ny_nyckel("para", svar) == u.facit[0]:
            return Bedomning(True, "Rätt.")
        fel = [i for i, (s, f) in enumerate(zip(svar, u.facit_svar)) if norm(s) != norm(f)]
        for i in fel:
            k = "x:" + norm(u.falt[i]) + "→" + norm(svar[i])
            if k in u.feltankar:
                return Bedomning(False, u.feltankar[k], feltanke=True, fel_falt=fel)
        return Bedomning(False, fore + ("En linje är fel." if len(fel) == 1 else f"{len(fel)} linjer är fel."), fel_falt=fel)
    # mala
    if not any(str(s).strip() for s in svar):
        return Bedomning(False, "Måla något först.", otolkbart=True)
    k = ny_nyckel("mala", svar, u.facit[0])
    if k == u.facit[0]:
        return Bedomning(True, "Rätt.")
    if k in u.feltankar:
        return Bedomning(False, u.feltankar[k], feltanke=True)
    if u.facit[0].startswith("c:"):
        return Bedomning(False, fore + "Räkna hur många delar varje färg ska ha.")
    fel = [i for i, f in enumerate(u.facit_svar) if i >= len(svar) or str(svar[i]).strip().casefold() != f]
    return Bedomning(False, fore + ("En del har fel färg." if len(fel) == 1 else f"{len(fel)} delar har fel färg."), fel_falt=fel)


# ---- bedömning ---------------------------------------------------------------

@dataclass
class Bedomning:
    ratt: bool
    text: str          # vad eleven får läsa
    feltanke: bool = False
    otolkbart: bool = False
    fel_falt: list[int] = field(default_factory=list)


def _lika(svar, facit) -> bool:
    if isinstance(facit, dict):
        return isinstance(svar, dict) and svar == facit
    if isinstance(facit, Fraction):
        return isinstance(svar, Fraction) and svar == facit
    return isinstance(svar, str) and svar == facit


def bedom(u: Uppgift, svar) -> Bedomning:
    """Rätt → kort bekräftelse. Känd feltanke → dess förklaring. Annars ledtråd.
    `svar` är en sträng, eller en lista med en sträng per fält för tabeller."""
    svar_lista = list(svar) if isinstance(svar, (list, tuple)) else [svar]
    if u.svarstyp in NYA_TYPER:
        return _bedom_ny(u, [str(x) for x in svar_lista])
    if u.svarstyp == "linje" and "lodrät" in svar_lista:
        return Bedomning(False, "Den linjen är lodrät – den har inget k. Linjen y = kx + m lutar, välj två punkter med olika x.", otolkbart=True)
    if u.svarstyp in ("valj", "markera"):            # klick i scen: jämför namnen
        if u.svarstyp == "markera":
            nyckel = _mangd(svar_lista[0] if svar_lista else "")
            if not nyckel:
                return Bedomning(False, "Klicka på det du vill markera först.", otolkbart=True)
        else:
            nyckel = str(svar_lista[0] or "").strip().casefold()
            if not nyckel:
                return Bedomning(False, "Klicka på något i bilden först.", otolkbart=True)
        if nyckel == u.facit[0]:
            return Bedomning(True, "Rätt.")
        if nyckel in u.feltankar:
            return Bedomning(False, u.feltankar[nyckel], feltanke=True, fel_falt=[0])
        if u.svarstyp == "markera":
            antal_ratt = len(u.facit[0].split(",")); antal = len(nyckel.split(","))
            if antal != antal_ratt:
                return Bedomning(False, (u.ledtrad + " " if u.ledtrad else "") +
                                 f"Du markerade {antal} – det ska vara {antal_ratt}.", fel_falt=[0])
        return Bedomning(False, u.ledtrad or "Inte riktigt. Titta på bilden igen.", fel_falt=[0])
    if u.svarstyp == "val":                       # flerval: jämför med alternativet som det står
        ratt = likvardiga_text(str(svar_lista[0]), u.facit_text[0])
        if ratt:
            return Bedomning(True, "Rätt.")
        nyckel = str(svar_lista[0]).strip()
        for fel, text in u.feltankar.items():
            if isinstance(fel, str) and likvardiga_text(fel, nyckel) or _lika(_varde(nyckel), fel):
                return Bedomning(False, text, feltanke=True, fel_falt=[0])
        return Bedomning(False, u.ledtrad or "Inte riktigt. Titta på kortet och försök igen.", fel_falt=[0])
    if len(svar_lista) != len(u.facit):
        return Bedomning(False, "Fyll i alla fält.", otolkbart=True)
    varden = []
    for s, f in zip(svar_lista, u.facit):
        s = str(s).strip()
        if not s:
            return Bedomning(False, "Skriv ett svar.", otolkbart=True)
        v = _varde(s)
        if isinstance(f, (Fraction, dict)) and isinstance(v, str):
            exempel = "2x + 3" if isinstance(f, dict) else "3/4, 0,75 eller 75 %"
            return Bedomning(False, f"Jag förstår inte svaret '{s}'. Skriv t.ex. {exempel}.", otolkbart=True)
        varden.append(v)
    tol = u.mall.tolerans
    def lika(v, f):
        if tol and isinstance(v, Fraction) and isinstance(f, Fraction):
            return abs(float(v) - float(f)) <= tol + 1e-12
        return _lika(v, f)
    if u.mall.oordnad and len(varden) > 1:        # x₁ och x₂ i vilken ordning som helst
        kvar = list(u.facit); fel = []
        for i, v in enumerate(varden):
            traff = next((j for j, f in enumerate(kvar) if lika(v, f)), None)
            if traff is None:
                fel.append(i)
            else:
                kvar.pop(traff)
    else:
        fel = [i for i, (v, f) in enumerate(zip(varden, u.facit)) if not lika(v, f)]
    if not fel:
        skrivet = str(svar_lista[0]).strip()
        if len(u.facit) == 1 and isinstance(u.facit[0], Fraction) and skrivet != u.facit_text[0] \
                and not likvardiga_text(skrivet, u.facit_text[0]):
            return Bedomning(True, f"Rätt – {skrivet} är samma tal som {u.facit_text[0]}.")
        return Bedomning(True, "Rätt.")
    v0 = varden[0]
    nyckel = skriv(v0) if isinstance(v0, dict) else v0
    if len(varden) > 1 and tuple(varden) in u.feltankar:
        return Bedomning(False, u.feltankar[tuple(varden)], feltanke=True, fel_falt=fel)
    if 0 in fel and nyckel in u.feltankar:
        return Bedomning(False, u.feltankar[nyckel], feltanke=True, fel_falt=fel)
    if len(u.facit) > 1:
        return Bedomning(False, u.ledtrad or f"Fel i {len(fel)} av {len(u.facit)} fält. Titta på kortet.", fel_falt=fel)
    return Bedomning(False, u.ledtrad or "Inte riktigt. Titta på kortet och försök igen.", fel_falt=fel)


def likvardiga_text(a: str, b: str) -> bool:
    """Samma skrivsätt sånär som på mellanslag och %-tecken."""
    norm = lambda s: s.replace(" ", "").replace(",", ".").casefold()
    return norm(a) == norm(b)
