"""`matte check` – kontrollerar att allt innehåll hänger ihop, utan skärm.

Körs i bygget (pack.sh) och av tests/. Stoppar om något kort inte går att
ladda, om en mall ger ett facit som inte bedöms rätt, om en figur inte går
att rita, om läroplanen pekar på kort som inte finns, eller om ett kort
pekar på en ikon eller förkunskap som saknas.
"""
from __future__ import annotations

import re

from .ikoner import alla as alla_ikoner
from .kort import DATA, OMRADEN, alla_kort, las_laroplan
from .regelbok import bok_html
from .rita import FIGURER, celler, cellnamn, figur
from .uppgifter import FARGER, MALBARA, MISSLYCKADE, NYA_TYPER, bedom, gor_uppgift, norm


def _kontrollera_ny(k, i, fro, m, u) -> str:
    """kryssa, para, mala, rita: facit ska bedömas rätt och gå att svara med."""
    var = f"{k.id} uppgift {i+1} frö {fro}"
    rest = [t for t in [u.fraga, u.ledtrad, *u.steg, *u.losning, *u.alternativ, *u.falt, *u.facit_svar,
                        *map(str, u.feltankar.values())] if re.search(r"\{[^{}]*\}", t)]
    if rest:
        return f"{var}: ofylld platshållare i '{rest[0][:60]}'"
    if u.figur:
        try:
            figur(u.figur)
        except Exception as e:                           # noqa: BLE001
            return f"{var}: figuren går inte att rita: {e}"
    if m.svarstyp == "rita":
        return ""
    if m.svarstyp == "kryssa":
        alt = {norm(a) for a in u.alternativ}
        if not u.facit_svar or any(norm(x) not in alt for x in u.facit_svar):
            return f"{var}: facit {u.facit_svar} finns inte bland alternativen {u.alternativ}"
        if len(alt) == len({norm(x) for x in u.facit_svar}):
            return f"{var}: alla alternativ är rätt – minst ett ska vara fel"
        if len(alt) != len(u.alternativ):
            return f"{var}: två alternativ är lika {u.alternativ}"
    if m.svarstyp == "para":
        if len({norm(x) for x in u.falt}) != len(u.falt):
            return f"{var}: två saker till vänster är lika {u.falt}"
    if m.svarstyp == "mala":
        ytor = len(celler(u.figur))
        if not ytor:
            return f"{var}: figuren har inga ytor att måla"
        if u.facit[0].startswith("m:") and len(u.facit_svar) != ytor:
            return f"{var}: {len(u.facit_svar)} färger i facit men {ytor} ytor"
        if len(u.facit_svar) > ytor:
            return f"{var}: facit målar {len(u.facit_svar)} ytor men figuren har {ytor}"
        okanda = [f for f in u.facit_svar if f and f not in FARGER]
        if okanda:
            return f"{var}: okänd färg '{okanda[0]}' (finns: {', '.join(FARGER)})"
        if not any(u.facit_svar):
            return f"{var}: facit målar ingenting"
    svar = u.facit_svar
    if m.svarstyp == "mala" and u.facit[0].startswith("c:"):
        svar = u.facit_svar + [""] * (len(celler(u.figur)) - len(u.facit_svar))
    b = bedom(u, svar)
    if not b.ratt:
        return f"{var}: facit {svar} bedöms fel ({b.text[:50]})"
    return ""


def kontrollera(fron: int = 40, tyst: bool = False) -> list[str]:
    fel: list[str] = []
    try:
        kort = alla_kort()
    except Exception as e:                           # noqa: BLE001
        return [f"Korten gick inte att läsa: {e}"]
    laroplan = las_laroplan()
    ikoner = set(alla_ikoner())

    for k in kort.values():
        if k.omrade not in OMRADEN:
            fel.append(f"{k.id}: okänt område '{k.omrade}'")
        if k.bild and not (DATA / "bilder" / k.bild).exists():
            fel.append(f"{k.id}: bilden '{k.bild}' finns inte i data/bilder")
        if k.ikon and k.ikon not in ikoner:
            fel.append(f"{k.id}: ikonen '{k.ikon}' finns inte")
        for m in k.mallar:
            if m.figur and m.figur.get("typ") in ("ikonval", "ikoner", "grupper"):
                for n in list(m.figur.get("ikoner") or []) + list(m.figur.get("fore") or []) + [m.figur.get("namn"), m.figur.get("fore_namn")]:
                    if isinstance(n, str) and "{" not in n and n not in ikoner:
                        fel.append(f"{k.id}: ikonen '{n}' i en uppgift finns inte")
        for f in k.forkunskap + k.ateruppta:
            if f not in kort:
                fel.append(f"{k.id}: pekar på kortet '{f}' som inte finns")
        if not k.mallar:
            fel.append(f"{k.id}: inga uppgifter")
        if not any(m.niva == 1 for m in k.mallar):
            fel.append(f"{k.id}: saknar nivå 1 (uttunnat exempel)")
        for r in k.rader:
            if r.figur:
                try:
                    figur(r.figur)
                except Exception as e:                   # noqa: BLE001
                    fel.append(f"{k.id}: rad-figur går inte att rita: {e}")
        for e in k.exempel:
            if e.figur:
                try:
                    figur(e.figur)
                except Exception as ex:                  # noqa: BLE001
                    fel.append(f"{k.id}: exempel-figur går inte att rita: {ex}")
        for i, m in enumerate(k.mallar):
            if m.svarstyp == "val" and not m.alternativ:
                fel.append(f"{k.id} uppgift {i+1}: flerval utan alternativ")
            if m.svarstyp == "tabell" and (not m.falt or not isinstance(m.svar, list) or len(m.falt) != len(m.svar)):
                fel.append(f"{k.id} uppgift {i+1}: tabell där fält och svar inte matchar")
            if m.figur and m.figur.get("typ") not in FIGURER:
                fel.append(f"{k.id} uppgift {i+1}: okänd figur '{m.figur.get('typ')}'")
            if m.svarstyp in ("valj", "markera") and (not m.figur or m.figur.get("typ") not in ("scen", "ikonval", "grupper")):
                fel.append(f"{k.id} uppgift {i+1}: {m.svarstyp} kräver en scen eller ikonval")
            if m.svarstyp == "dra" and (not m.figur or m.figur.get("typ") != "tioram"):
                fel.append(f"{k.id} uppgift {i+1}: dra kräver en tioram")
            if m.svarstyp == "punkt" and (not m.figur or m.figur.get("typ") not in ("tallinje", "koordinatsystem")):
                fel.append(f"{k.id} uppgift {i+1}: punkt kräver tallinje eller koordinatsystem")
            if m.svarstyp == "linje" and (not m.figur or m.figur.get("typ") != "koordinatsystem"):
                fel.append(f"{k.id} uppgift {i+1}: linje kräver koordinatsystem")
            if m.svarstyp in ("klick", "farga") and (not m.figur or m.figur.get("typ") not in
                                                       ("stapel", "staende", "tiostaplar", "hundraruta", "prickar", "ikoner")):
                fel.append(f"{k.id} uppgift {i+1}: {m.svarstyp} kräver en figur med rutor")
            if m.svarstyp == "kryssa" and len(m.alternativ) < 3:
                fel.append(f"{k.id} uppgift {i+1}: kryssa behöver minst tre alternativ")
            if m.svarstyp == "para" and len(m.par) < 2:
                fel.append(f"{k.id} uppgift {i+1}: para behöver minst två par")
            if m.svarstyp == "mala" and (not m.figur or m.figur.get("typ") not in MALBARA):
                fel.append(f"{k.id} uppgift {i+1}: mala kräver en figur med ytor ({', '.join(MALBARA)})")
            if m.svarstyp == "rita" and m.figur and m.figur.get("typ") in ("scen",):
                fel.append(f"{k.id} uppgift {i+1}: rita på en målning stöds inte")
            for fro in range(fron):
                MISSLYCKADE.clear()
                try:
                    u = gor_uppgift(k, m, fro=fro)
                except Exception as e:                   # noqa: BLE001
                    fel.append(f"{k.id} uppgift {i+1} frö {fro}: går inte att skapa: {e}"); break
                if m.svarstyp in NYA_TYPER:
                    f = _kontrollera_ny(k, i, fro, m, u)
                    if f:
                        fel.append(f); break
                    if MISSLYCKADE:
                        fel.append(f"{k.id} uppgift {i+1} frö {fro}: platshållaren {{{MISSLYCKADE[0][:50]}}} gick inte att räkna ut"); break
                    continue
                svar = u.facit_text if m.svarstyp in ("tabell", "punkt", "linje") and len(u.facit) > 1 else u.facit_text[0]
                b = bedom(u, svar)
                if not b.ratt:
                    fel.append(f"{k.id} uppgift {i+1} frö {fro}: facit '{svar}' bedöms fel ({b.text[:50]})"); break
                if m.svarstyp in ("valj", "markera") and u.figur:
                    namn = {n.casefold() for n in cellnamn(u.figur)}
                    if any(d not in namn for d in u.facit[0].split(",")):
                        fel.append(f"{k.id} uppgift {i+1}: facit '{u.facit_text[0]}' finns inte bland klickytornas namn"); break
                if m.svarstyp == "val" and u.facit_text[0] not in u.alternativ:
                    fel.append(f"{k.id} uppgift {i+1} frö {fro}: facit '{u.facit_text[0]}' finns inte bland alternativen {u.alternativ}"); break
                if u.figur:
                    try:
                        figur(u.figur)
                        if m.svarstyp in ("klick", "farga") and not celler(u.figur):
                            raise ValueError("inga klickbara rutor")
                    except Exception as e:               # noqa: BLE001
                        fel.append(f"{k.id} uppgift {i+1} frö {fro}: figuren går inte att rita: {e}"); break
                rest = [t for t in [u.fraga, u.ledtrad, *u.steg, *u.losning, *map(str, u.feltankar.values())]
                        if re.search(r"\{[^{}]*\}", t)]
                if rest:
                    fel.append(f"{k.id} uppgift {i+1} frö {fro}: ofylld platshållare i '{rest[0][:60]}'"); break
                if MISSLYCKADE:
                    fel.append(f"{k.id} uppgift {i+1} frö {fro}: platshållaren {{{MISSLYCKADE[0][:50]}}} gick inte att räkna ut"); break
                for felsvar in u.feltankar:
                    if felsvar == u.facit[0] and m.svarstyp != "val":
                        pass  # samma som facit för detta frö – ofarligt, facit vinner

    for m in laroplan:
        if m.omrade not in OMRADEN:
            fel.append(f"{m.id}: okänt område '{m.omrade}'")
        for kid in m.kort:
            if kid not in kort:
                fel.append(f"{m.id}: pekar på kortet '{kid}' som inte finns")
    oanvanda = set(kort) - {kid for m in laroplan for kid in m.kort}
    for kid in sorted(oanvanda):
        fel.append(f"{kid}: ingår inte i någon läroplan")

    try:
        bok_html(list(kort.values()))
    except Exception as e:                               # noqa: BLE001
        fel.append(f"Regelboken går inte att skriva: {e}")

    fel += kontrollera_ordlista()
    obs = innehall(kort)
    if not tyst:
        print(f"{len(kort)} kort, {len(laroplan)} mål, {len(ikoner)} ikoner, {len(FIGURER)} figurtyper, "
              f"data i {DATA}")
        tackta = sum(1 for m in laroplan if m.kort)
        print(f"{tackta} av {len(laroplan)} mål har kort")
        print(f"{sum(len(k.mallar) for k in kort.values())} uppgiftsmallar, "
              f"{sum(len(k.exempel) for k in kort.values())} lösta exempel")
        for o in obs:
            print("OBS:", o)
        for f in fel:
            print("FEL:", f)
        print("OK" if not fel else f"{len(fel)} fel")
    return fel


def las_ordlista() -> list[dict]:
    """data/ordlista.toml – matteorden med förklaring och bild (används av C++-programmet)."""
    import tomllib
    p = DATA / "ordlista.toml"
    if not p.exists():
        return []
    return tomllib.loads(p.read_text(encoding="utf-8")).get("ord", [])


def kontrollera_ordlista() -> list[str]:
    fel, sett = [], {}
    ord_ = las_ordlista()
    namn = {o.get("ord", "") for o in ord_}
    for o in ord_:
        rubrik = o.get("ord", "?")
        if not o.get("forklaring"):
            fel.append(f"ordlistan: '{rubrik}' saknar förklaring")
        for f in o.get("former", []):
            if f != f.lower() or " " in f:
                fel.append(f"ordlistan: formen '{f}' ska vara ett ord med små bokstäver")
            if f in sett:
                fel.append(f"ordlistan: formen '{f}' finns i både '{sett[f]}' och '{rubrik}'")
            sett[f] = rubrik
        if not o.get("former"):
            fel.append(f"ordlistan: '{rubrik}' saknar former")
        for s in o.get("se", []):
            if s not in namn:
                fel.append(f"ordlistan: '{rubrik}' hänvisar till '{s}' som inte finns")
        if o.get("figur"):
            try:
                figur(o["figur"])
            except Exception as e:                       # noqa: BLE001
                fel.append(f"ordlistan: figuren till '{rubrik}' går inte att rita: {e}")
    return fel


MIN_MALLAR, MIN_EXEMPEL = 8, 2


def innehall(kort) -> list[str]:
    """Kort som är tunnare än resten: få uppgifter, inga lösta exempel, ingen lösning
    att visa vid fel. Stoppar inte bygget – men visar var nästa arbete behövs."""
    obs = []
    for k in kort.values():
        brist = []
        if len(k.mallar) < MIN_MALLAR:
            brist.append(f"{len(k.mallar)} uppgifter (minst {MIN_MALLAR})")
        if len(k.exempel) < MIN_EXEMPEL:
            brist.append(f"{len(k.exempel)} lösta exempel (minst {MIN_EXEMPEL})")
        if not any(m.steg for m in k.mallar):
            brist.append("inget exempel med lucka")
        utan = sum(1 for m in k.mallar if not m.losning and m.svarstyp not in ("valj", "markera", "klick", "farga", "dra"))
        if utan > len(k.mallar) // 2:
            brist.append(f"{utan} uppgifter utan lösning")
        if brist:
            obs.append(f"{k.id}: " + ", ".join(brist))
    return obs
