"""Kommandoraden:  matte            – öppnar fönstret
                 matte kort       – listar korten och läroplanens mål
                 matte regelbok [fil.html] – skriver regelboken (alla kort, eller
                                     elevens egen med --elev)
                 matte ova <kort> – övar i terminalen (bra utan skärm)
                 matte check      – kontrollerar kort, läroplan, figurer och ikoner
                 matte arbetsblad <kort> [<kort> …] [--antal 6] [--blad 4711] [-o fil.html]
                                  – utskrivbart arbetsblad med facit
                 matte oversikt [fil.html] – elevernas framsteg och vanligaste feltankar
                 matte elever     – vilka elever som finns på datorn
                 matte omraden <bild> – sida för att dra klickytor på en målning
"""
from __future__ import annotations

import sys


def _kort():
    from .kort import alla_kort, las_laroplan
    kort = alla_kort(); mal = las_laroplan()
    print(f"{len(kort)} kort, {len(mal)} mål\n")
    for m in mal:
        tackt = ", ".join(m.kort) if m.kort else "– kommer –"
        print(f"[{m.stadium}] {m.omrade:12} {m.text[:60]:60} → {tackt}")


def _regelbok(args):
    from .framsteg import Elev, senaste_elev
    from .kort import alla_kort
    from .regelbok import bok_html
    kort = alla_kort()
    fil = next((a for a in args if not a.startswith("--")), "regelbok.html")
    if "--elev" in args:
        e = Elev.ladda(senaste_elev())
        urval = [kort[k] for k in e.regelbok if k in kort]
        titel = "Min regelbok"
    else:
        urval = list(kort.values()); titel = "Regelboken"
    with open(fil, "w", encoding="utf-8") as f:
        f.write(bok_html(urval, titel))
    print(f"Skrev {fil} ({len(urval)} kort)")


def _ova(args):
    from .framsteg import Elev, senaste_elev
    from .kort import alla_kort
    from . import adaptiv
    from .uppgifter import bedom, feltanke_id, gor_uppgift
    kort = alla_kort()
    if not args or args[0] not in kort:
        print("Välj ett kort:", ", ".join(kort)); return 1
    k = kort[args[0]]; e = Elev.ladda(senaste_elev()); e.sett_kort(k.id)
    print(f"— {k.titel} —\n{k.sager}\n(Enter utan svar avslutar)\n")
    while True:
        s = e.status(k.id)
        mall, tranar = adaptiv.valj_mall(k, s)
        niva = mall.niva
        u = gor_uppgift(k, mall)
        if tranar and u.ledtrad:
            print("  Tänk på:", u.ledtrad)
        for rad in u.steg:
            print("    " + rad)
        if u.figur:
            fig = u.figur
            if fig.get("typ") in ("stapel", "tiostaplar", "staende"):
                d, f = int(fig.get("delar", 10)), int(fig.get("fyllda", 0))
                print("  [" + "■" * f + "□" * (d - f) + "]")
        if u.svarstyp == "val":
            print("  Alternativ:", " · ".join(u.alternativ))
        if u.svarstyp == "tabell":
            print(f"Nivå {niva}: {u.fraga}")
            svar = [input(f"  {f}: ").strip() for f in u.falt]
            if not any(svar):
                break
        elif u.svarstyp in ("punkt", "linje") and len(u.facit) > 1:
            print(f"Nivå {niva}: {u.fraga}")
            svar = input("  (skriv x;y eller k;m) > ").strip()
            if not svar:
                break
            svar = svar.split(";")
        else:
            svar = input(f"Nivå {niva}: {u.fraga}\n> ").strip()
            if not svar:
                break
        b = bedom(u, svar)
        if not b.otolkbart:
            e.svarade(k.id, b.ratt, feltanke_id(u, b), mall_index=u.mall_index); e.spara()
            if not b.ratt and (fore := adaptiv.ga_tillbaka(k, e, kort)):
                print(f"  Det här bygger på något tidigare – prova: matte ova {fore.id}")
        print(("✓ " if b.ratt else "✗ ") + b.text + ("" if b.ratt else f"  (facit: {', '.join(u.facit_text)})"))
        if not b.ratt and u.losning:
            print("  Så här kan man tänka:")
            for i, rad in enumerate(u.losning, 1):
                print(f"    {i}. {rad}")
        print()
    print("Status:", e.sammanfattning())
    return 0


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if not argv:
        from .fonster import kor
        return kor()
    cmd, args = argv[0], argv[1:]
    if cmd == "kort":
        return _kort()
    if cmd == "regelbok":
        return _regelbok(args)
    if cmd == "ova":
        return _ova(args)
    if cmd == "arbetsblad":
        from .arbetsblad import arbetsblad
        from .kort import alla_kort
        kort = alla_kort(); valda, antal, fro, ut = [], 6, None, None
        i = 0
        while i < len(args):
            a = args[i]
            if a == "--antal": antal = int(args[i + 1]); i += 2; continue
            if a == "--blad": fro = int(args[i + 1]); i += 2; continue
            if a == "-o": ut = args[i + 1]; i += 2; continue
            if a not in kort:
                print("Okänt kort:", a, "– se `matte kort`"); return 1
            valda.append(kort[a]); i += 1
        if not valda:
            print("Ange minst ett kort, t.ex. matte arbetsblad brak-delar"); return 1
        text = arbetsblad(valda, antal, fro)
        ut = ut or f"arbetsblad-{valda[0].id}.html"
        with open(ut, "w", encoding="utf-8") as f:
            f.write(text)
        print("Skrev", ut); return 0
    if cmd == "oversikt":
        from .oversikt import html_rapport
        ut = args[0] if args else "oversikt.html"
        with open(ut, "w", encoding="utf-8") as f:
            f.write(html_rapport())
        print("Skrev", ut); return 0
    if cmd == "omraden":
        from .omraden import hitta, sida
        if not args or not (bild := hitta(args[0])):
            print("Ange en bild i data/bilder, t.ex. matte omraden pizza-attondelar.jpg"); return 1
        ut = f"omraden-{bild.stem}.html"
        with open(ut, "w", encoding="utf-8") as f:
            f.write(sida(bild))
        print("Skrev", ut, "– öppna den i webbläsaren"); return 0
    if cmd == "elever":
        from .framsteg import elever
        print("\n".join(elever()) or "(inga elever än)"); return 0
    if cmd == "check":
        from .kontroll import kontrollera
        return 1 if kontrollera() else 0
    print(__doc__); return 1


if __name__ == "__main__":
    sys.exit(main() or 0)
