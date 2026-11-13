#!/usr/bin/env python3
"""Testfall för C++-tolkningen: samma texter genom Python-versionens tolka() och tolka_uttryck().

    python3 verktyg/testfall.py innehall tester/fall.json

C++-testerna (src/Test.cpp) läser filen och kontrollerar att C++ räknar fram exakt samma
värden – så att elevens svar bedöms likadant som i Python-versionen.
"""
import json
import sys
from pathlib import Path

EXTRA = ["1/2", "0,5", "0.5", "50 %", "50%", "2/4", "1 1/2", "-1 1/2", "√9", "√(9/4)", "sqrt(2)", "2^-1", "2^3^2",
         "8^(1/3)", "(-8)^(1/3)", "16^(3/4)", "sin(30)", "cos 60", "tan(45)", "sin(20)", "lg 1000", "lg(2)", "ln(5)",
         "π", "2π", "3 · 4", "3×4", "12:4", "−5", "5 − −3", "(2+3)*4", "10 %", "1/0", "abc", "", "5 kr", "3,5,6",
         "2x+3x", "x·5", "(x+1)(x+1)", "x²+2x+1", "3(x+2)", "x^2 - 9", "(x+3)(x-3)", "2x/4", "x/0", "x^-1",
         "4x² + 12x + 9", "-(x - 3)", "a+b+a", "x1 + x1", "0,25x", "1/2x", "y = 2x"]


def main():
    innehall, ut = Path(sys.argv[1]).resolve(), Path(sys.argv[2])
    sys.path.insert(0, str(innehall))
    from matte.algebra import Algebrafel, skriv, tolka_uttryck
    from matte.kort import alla_kort
    from matte.uppgifter import NYA_TYPER, gor_uppgift
    from matte.uttryck import Uttrycksfel, tolka
    texter = set(EXTRA)
    for k in alla_kort().values():
        for m in k.mallar:
            if m.svarstyp in NYA_TYPER:          # kryssa/para/mala/rita: inga skrivna svar att tolka
                continue
            for fro in range(6):
                u = gor_uppgift(k, m, fro=fro)
                texter.update(u.facit_text)
                texter.update(str(a) for a in u.alternativ)
    fall = []
    for t in sorted(texter):
        f = {"in": t}
        try:
            v = tolka(t); f["tal"] = f"{v.numerator}/{v.denominator}"
        except (Uttrycksfel, ZeroDivisionError, ValueError, OverflowError):
            f["tal"] = None
        try:
            f["poly"] = skriv(tolka_uttryck(t))
        except (Algebrafel, ZeroDivisionError, ValueError):
            f["poly"] = None
        fall.append(f)
    ut.parent.mkdir(parents=True, exist_ok=True)
    ut.write_text(json.dumps(fall, ensure_ascii=False, indent=0), encoding="utf-8")
    print(len(fall), "testfall →", ut)


if __name__ == "__main__":
    main()
