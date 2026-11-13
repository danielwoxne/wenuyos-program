#!/usr/bin/env python3
"""Byggsteg: korten (TOML) → färdiga data som C++-programmet läser.

Innehållet skrivs fortfarande som regelkort i innehall/data/kort/*.toml – med mallar där
{a}, {n} … slumpas. C++-programmet ska inte behöva tolka Python-uttrycken i mallarna, så här
räknas varje mall fram till ett antal färdiga *varianter* (fråga, steg, lösning, figur,
facit, feltankar). Programmet väljer sedan en variant i stället för att slumpa tal.

    python3 verktyg/expandera.py innehall ut/share/se.wenuyos.app.matte [reservikoner-i-byggmapp]

Skriver:
    kort.json              alla kort (rader, exempel, gäller …) och läroplanen
    ordlista.json          matteorden med förklaring och bild (data/ordlista.toml)
    uppgifter/<kort>.json  varianterna för kortets mallar
    bilder/, ikoner/       målningarna och lerikonerna (kopieras)

Facit och feltankar skrivs som *nycklar* – samma form som C++-programmet räknar fram ur
elevens svar (se src/Bedom.cpp):  t:3/4 (tal), p:x² + 2x (polynom), o:blå (ord/namn),
flera fält med ";" emellan. kryssa/para/mala: k:/q:/m:/c: (se ny_nyckel i innehall/matte/uppgifter.py).
"""
from __future__ import annotations

import json
import os
import re
import shutil
import sys
from fractions import Fraction
from pathlib import Path

MAX_VARIANTER = 40      # per mall – nog för att det sällan blir samma uppgift två gånger
FROER = 400             # så många frön provas för att hitta olika varianter


def nyckel(v) -> str:
    """Ett värde (Fraction, polynom-dict, str eller tupel) → jämförbar nyckel."""
    from matte.algebra import skriv
    if isinstance(v, tuple):
        return ";".join(nyckel(x) for x in v)
    if isinstance(v, Fraction):
        return f"t:{v.numerator}/{v.denominator}"
    if isinstance(v, dict):
        if all(m == () for m in v):                      # konstant polynom = tal
            c = v.get((), Fraction(0))
            return f"t:{c.numerator}/{c.denominator}"
        return "p:" + skriv(v)
    return "o:" + str(v)


def textnyckel(s: str) -> str:
    """Som likvardiga_text i Python-versionen: utan mellanslag, komma = punkt, gemener."""
    return "x:" + str(s).replace(" ", "").replace(",", ".").casefold()


def variant(u, kort, mall) -> dict:
    """En färdig uppgift. Feltankarna räknas om här med samma nycklar som C++ använder."""
    from matte.uppgifter import NYA_TYPER, _fyll, _mangd, _varde, ny_felnyckel
    if mall.svarstyp in NYA_TYPER:
        return variant_ny(u, mall)
    fel = []
    for felsvar, forklaring in {**kort.feltankar, **mall.feltankar}.items():
        fylld = str(_fyll(felsvar, u.varden))
        text = str(_fyll(forklaring, u.varden))
        if mall.svarstyp == "markera":
            k, v = "o:" + _mangd(fylld), ""
        elif mall.svarstyp == "valj":
            k, v = "o:" + fylld.strip().casefold(), ""
        elif ";" in fylld:
            k, v = nyckel(tuple(_varde(d) for d in fylld.split(";"))), ""
        else:
            k = nyckel(_varde(fylld))
            v = textnyckel(fylld) if mall.svarstyp == "val" else ""
        e = {"k": k, "x": text, "id": f"{u.mall_index}|{felsvar}", "s": fylld}   # s: felsvaret (för testerna)
        if v:
            e["v"] = v
        fel.append(e)
    d = {"f": u.fraga, "h": u.ledtrad, "facit": [nyckel(f) for f in u.facit], "ft": list(u.facit_text)}
    if u.steg:
        d["s"] = u.steg
    if u.losning:
        d["l"] = u.losning
    if u.figur:
        d["fig"] = u.figur
    if u.alternativ:
        d["alt"] = u.alternativ
    if u.falt:
        d["falt"] = u.falt
    if fel:
        d["fel"] = fel
    return d


def variant_ny(u, mall) -> dict:
    """kryssa, para, mala, rita: facit är redan nycklar; "fs" = facit som elevens svar."""
    from matte.uppgifter import _fyll, ny_felnyckel
    d = {"f": u.fraga, "h": u.ledtrad, "facit": list(u.facit), "ft": list(u.facit_text)}
    if u.facit_svar:
        d["fs"] = u.facit_svar
    for nyckel_, falt in (("s", u.steg), ("l", u.losning), ("alt", u.alternativ), ("falt", u.falt)):
        if falt:
            d[nyckel_] = falt
    if u.figur:
        d["fig"] = u.figur
    fel = []
    for felsvar, forklaring in mall.feltankar.items():
        fylld = str(_fyll(felsvar, u.varden))
        fel.append({"k": ny_felnyckel(mall.svarstyp, fylld), "x": str(_fyll(forklaring, u.varden)),
                    "id": f"{u.mall_index}|{felsvar}", "s": fylld})
    if fel:
        d["fel"] = fel
    return d


def mallar(kort) -> list[dict]:
    from matte.uppgifter import gor_uppgift
    ut = []
    for i, m in enumerate(kort.mallar):
        sedda, varianter = set(), []
        antal_fro = FROER if m.var else 1
        for fro in range(antal_fro):
            u = gor_uppgift(kort, m, fro=fro)
            v = variant(u, kort, m)
            ordning = m.svarstyp in ("para", "kryssa")      # här är ordningen en egen variant (blandas)
            alt, falt = v.get("alt", []), v.get("falt", [])
            sign = json.dumps([v["f"], v.get("s"), v.get("fig"), alt if ordning else sorted(alt), falt if ordning else sorted(falt)],
                              ensure_ascii=False, sort_keys=True)
            if sign in sedda:
                continue
            sedda.add(sign); varianter.append(v)
            if len(varianter) >= MAX_VARIANTER:
                break
        ut.append({"niva": m.niva, "typ": m.svarstyp, "tol": m.tolerans, "oordnad": m.oordnad,
                   "form": m.form, "varianter": varianter})
    return ut


def kortdata(k) -> dict:
    return {
        "id": k.id, "titel": k.titel, "omrade": k.omrade, "ikon": k.ikon, "bild": k.bild,
        "bildtext": k.bildtext, "sager": k.sager, "galler": k.galler, "galler_inte": k.galler_inte,
        "rader": [{"sager": r.sager, "figur": r.figur, "skriver": r.skriver, "kommentar": r.kommentar} for r in k.rader],
        "exempel": [{"rubrik": e.rubrik, "text": e.text, "figur": e.figur, "steg": e.steg} for e in k.exempel],
        "forkunskap": k.forkunskap, "ateruppta": k.ateruppta, "kalla": k.kalla,
        "mallar": len(k.mallar),
        # feltankarnas förklaringar utan konkreta tal – för lärarens översikt ("<mall>|<felsvar>" → text)
        "feltankar": {f"{i}|{felsvar}": re.sub(r"\{[^{}]+\}", "…", forklaring)
                      for i, m in enumerate(k.mallar) for felsvar, forklaring in {**k.feltankar, **m.feltankar}.items()},
    }



def kopiera_ikoner(innehall: Path, ut: Path, reserv: Path | None = None) -> None:
    """Kopiera verkliga ikoner först, därefter bara de reservikoner som saknas.

    All utdata ligger under ut (byggmappen); källornas data/ikoner lämnas orörd.
    """
    mal = ut / "ikoner"
    if mal.exists():
        shutil.rmtree(mal)
    original = innehall / "data" / "ikoner"
    if original.is_dir():
        shutil.copytree(original, mal)
    else:
        mal.mkdir(parents=True, exist_ok=True)
    if reserv is not None:
        if not reserv.is_dir():
            raise FileNotFoundError(f"Reservikonmappen saknas: {reserv}")
        for fil in sorted(reserv.glob("*.png")):
            if fil.is_file() and not (mal / fil.name).exists():
                shutil.copy2(fil, mal / fil.name)


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    innehall, ut = Path(sys.argv[1]).resolve(), Path(sys.argv[2])
    reserv = Path(sys.argv[3]).resolve() if len(sys.argv) == 4 else None
    if reserv is not None:
        # Innehållskontrollen slår upp ikoner. Gör byggkopian först och peka
        # kontrollen dit, annars hittar den inte reserven och avbryter bygget.
        kopiera_ikoner(innehall, ut, reserv)
        os.environ["MATTE_IKONER_DIR"] = str((ut / "ikoner").resolve())
    sys.path.insert(0, str(innehall))
    from matte.kontroll import kontrollera
    from matte.kort import OMRADEN, STADIER, alla_kort, las_laroplan
    fel = kontrollera(tyst=True)
    if fel:
        sys.exit("Innehållet har fel – kör `python3 innehall/main.py check`:\n" + "\n".join(fel[:20]))
    korten = alla_kort()
    (ut / "uppgifter").mkdir(parents=True, exist_ok=True)
    allt = {
        "omraden": OMRADEN, "stadier": STADIER,
        "laroplan": [{"id": m.id, "stadium": m.stadium, "omrade": m.omrade, "text": m.text, "kort": m.kort}
                     for m in las_laroplan()],
        "kort": [kortdata(k) for k in korten.values()],
    }
    (ut / "kort.json").write_text(json.dumps(allt, ensure_ascii=False), encoding="utf-8")
    from matte.kontroll import las_ordlista
    ordlista = las_ordlista()
    (ut / "ordlista.json").write_text(json.dumps(ordlista, ensure_ascii=False), encoding="utf-8")
    totalt = 0
    for k in korten.values():
        m = mallar(k)
        totalt += sum(len(x["varianter"]) for x in m)
        (ut / "uppgifter" / f"{k.id}.json").write_text(json.dumps(m, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    bildmapp = ut / "bilder"
    if bildmapp.exists():
        shutil.rmtree(bildmapp)
    shutil.copytree(innehall / "data" / "bilder", bildmapp)
    if reserv is None:
        kopiera_ikoner(innehall, ut)
    print(f"{len(korten)} kort, {sum(len(k.mallar) for k in korten.values())} mallar, {totalt} färdiga uppgifter, "
          f"{len(ordlista)} ord i ordlistan → {ut}")


if __name__ == "__main__":
    main()
