"""Algebra – uttryck med variabler, för åk 7–9 och gymnasiet.

Ett uttryck tolkas till ett *polynom*: en ordbok från monom (sorterade
variabel-exponent-par) till koefficient (Fraction). Två uttryck är
likvärdiga om de ger samma polynom – så 2x + 3x, 5x och x·5 bedöms lika,
och (x+1)(x+1) = x² + 2x + 1.

Stöder: tal (komma eller punkt), variabler (en bokstav, ev. med index: x, y, a, x1),
+ - · * / ^, parenteser, implicit multiplikation (2x, 3(x+1), xy).
Division bara med tal. Ekvationer: `los_linjar("2x + 3 = 11")` → 4.
"""
from __future__ import annotations

import re
from fractions import Fraction

__all__ = ["Polynom", "tolka_uttryck", "likvardiga_uttryck", "los_linjar", "skriv", "Algebrafel"]


class Algebrafel(ValueError):
    pass


Monom = tuple[tuple[str, int], ...]
Polynom = dict[Monom, Fraction]

_TOKEN = re.compile(r"\s*(?:(?P<tal>\d+(?:[.,]\d+)?)|(?P<var>[a-zA-Z]\d?)|(?P<op>[-+*/^()·×²³]))")


def _tokenisera(text: str) -> list[tuple[str, str]]:
    text = text.strip().replace("−", "-")
    pos, ut = 0, []
    while pos < len(text):
        m = _TOKEN.match(text, pos)
        if not m or m.end() == pos:
            raise Algebrafel(f"Förstår inte tecknet '{text[pos]}'")
        pos = m.end()
        if m.group("tal") is not None:
            ut.append(("tal", m.group("tal").replace(",", ".")))
        elif m.group("var") is not None:
            ut.append(("var", m.group("var")))
        else:
            op = m.group("op")
            if op in "²³":            # x² → x ^ 2
                ut.append(("op", "^")); ut.append(("tal", "2" if op == "²" else "3"))
            else:
                ut.append(("op", {"·": "*", "×": "*"}.get(op, op)))
    return ut


# ---- polynomräkning ---------------------------------------------------------

def _konst(c) -> Polynom:
    return {(): Fraction(c)} if c != 0 else {}


def _var(namn: str) -> Polynom:
    return {((namn, 1),): Fraction(1)}


def _add(a: Polynom, b: Polynom, tecken=1) -> Polynom:
    r = dict(a)
    for m, c in b.items():
        r[m] = r.get(m, Fraction(0)) + tecken * c
        if r[m] == 0:
            del r[m]
    return r


def _mul_monom(m1: Monom, m2: Monom) -> Monom:
    d = dict(m1)
    for v, e in m2:
        d[v] = d.get(v, 0) + e
    return tuple(sorted((v, e) for v, e in d.items() if e))


def _mul(a: Polynom, b: Polynom) -> Polynom:
    r: Polynom = {}
    for m1, c1 in a.items():
        for m2, c2 in b.items():
            m = _mul_monom(m1, m2)
            r[m] = r.get(m, Fraction(0)) + c1 * c2
    return {m: c for m, c in r.items() if c != 0}


def _pow(a: Polynom, n: int) -> Polynom:
    r = _konst(1)
    for _ in range(n):
        r = _mul(r, a)
    return r


def _ar_konst(a: Polynom) -> bool:
    return all(m == () for m in a)


class _Tolk:
    def __init__(self, t):
        self.t = t; self.i = 0

    def kika(self):
        return self.t[self.i] if self.i < len(self.t) else (None, None)

    def ta(self):
        tok = self.kika(); self.i += 1; return tok

    def uttryck(self) -> Polynom:
        v = self.term()
        while self.kika() in (("op", "+"), ("op", "-")):
            op = self.ta()[1]
            v = _add(v, self.term(), 1 if op == "+" else -1)
        return v

    def term(self) -> Polynom:
        v = self.potens()
        while True:
            slag, tok = self.kika()
            if (slag, tok) in (("op", "*"), ("op", "/")):
                self.ta()
                h = self.potens()
                if tok == "/":
                    if not _ar_konst(h):
                        raise Algebrafel("Kan bara dela med tal")
                    k = h.get((), Fraction(0))
                    if k == 0:
                        raise Algebrafel("Division med noll")
                    v = _mul(v, _konst(1 / k))
                else:
                    v = _mul(v, h)
            elif slag == "var" or (slag == "op" and tok == "("):
                v = _mul(v, self.potens())          # implicit: 2x, 3(x+1), xy
            else:
                return v

    def potens(self) -> Polynom:
        v = self.primar()
        if self.kika() == ("op", "^"):
            self.ta()
            e = self.primar()
            if not _ar_konst(e) or e.get((), Fraction(0)).denominator != 1 or e.get((), 0) < 0:
                raise Algebrafel("Bara positiva heltalsexponenter")
            v = _pow(v, int(e.get((), 0)))
        return v

    def primar(self) -> Polynom:
        slag, tok = self.kika()
        if slag == "op" and tok == "-":
            self.ta(); return _mul(_konst(-1), self.potens())
        if slag == "op" and tok == "+":
            self.ta(); return self.potens()
        if slag == "op" and tok == "(":
            self.ta(); v = self.uttryck()
            if self.ta() != ("op", ")"):
                raise Algebrafel("Saknar )")
            return v
        if slag == "tal":
            self.ta(); return _konst(Fraction(tok))
        if slag == "var":
            self.ta(); return _var(tok)
        raise Algebrafel("Väntade ett tal eller en variabel")


def tolka_uttryck(text: str) -> Polynom:
    if not text or not text.strip():
        raise Algebrafel("Inget svar")
    t = _tokenisera(text); p = _Tolk(t); v = p.uttryck()
    if p.i != len(t):
        raise Algebrafel("Något blev över i uttrycket")
    return v


def likvardiga_uttryck(a: str, b: str) -> bool:
    try:
        return tolka_uttryck(a) == tolka_uttryck(b)
    except Algebrafel:
        return False


def los_linjar(ekvation: str, variabel: str = "x") -> Fraction:
    """Löser en ekvation av första graden i en variabel: 2x + 3 = 11 → 4."""
    if "=" not in ekvation:
        raise Algebrafel("Ingen likhet (=)")
    v, h = ekvation.split("=", 1)
    p = _add(tolka_uttryck(v), tolka_uttryck(h), -1)     # vänster − höger = 0
    k = p.get(((variabel, 1),), Fraction(0))
    rest = {m: c for m, c in p.items() if m != ((variabel, 1),)}
    if k == 0 or not _ar_konst(rest):
        raise Algebrafel("Inte en ekvation av första graden i " + variabel)
    return -rest.get((), Fraction(0)) / k


def varde(p: Polynom, varden: dict[str, float]) -> float:
    """Räknar ut polynomet för givna värden på variablerna (för grafer)."""
    tot = 0.0
    for m, c in p.items():
        t = float(c)
        for v, e in m:
            t *= float(varden.get(v, 0.0)) ** e
        tot += t
    return tot


def skriv(p: Polynom) -> str:
    """Skriver ett polynom på skolans sätt: 3x² + 2x − 5."""
    if not p:
        return "0"
    def grad(m): return sum(e for _, e in m)
    delar = []
    for m in sorted(p, key=lambda m: (-grad(m), m)):
        c = p[m]
        vars_ = "".join(v + ("" if e == 1 else "²" if e == 2 else "³" if e == 3 else f"^{e}") for v, e in m)
        if c.denominator == 1:
            k = str(abs(c.numerator))
        else:
            k = f"{abs(c.numerator)}/{c.denominator}"
        if vars_ and k == "1":
            k = ""
        delar.append(("−" if c < 0 else "+", (k + vars_) or "0"))
    s = ("−" if delar[0][0] == "−" else "") + delar[0][1]
    for tecken, d in delar[1:]:
        s += f" {tecken} {d}"
    return s
