"""Uttrycksmotorn – tolkar och räknar ut matematiska uttryck exakt.

Allt räknas med bråk (fractions.Fraction) så att 1/2, 0,5, 50 % och 2/4
är *samma* tal. Det är grunden för att programmet kan avgöra om ett svar
är likvärdigt med facit i stället för att kräva en viss form.

Stöder: heltal, decimaltal med komma eller punkt, bråk a/b, blandad form
"1 1/2", procent (50 %), + - · * / : ^, parenteser, negativa tal – och för
gymnasiet: √9, sqrt(2), 8^(1/3), 2^-1, sin/cos/tan (grader), lg, ln, π.

Rötter och potenser räknas exakt när svaret är rationellt (√(9/4) = 3/2,
16^(3/4) = 8). Annars, och för sin/lg/π, blir talet ett bråk som ligger
inom 10⁻⁹ från det riktiga – uppgifter med sådana svar har en tolerans.
"""
from __future__ import annotations

from fractions import Fraction
import re

__all__ = ["tolka", "likvardiga", "Uttrycksfel", "formatera", "former"]


class Uttrycksfel(ValueError):
    """Uttrycket gick inte att tolka."""


_TOKEN = re.compile(
    r"\s*(?:(?P<tal>\d+(?:[.,]\d+)?)|(?P<op>[-+*/:^()·×√])|(?P<proc>%)|(?P<namn>π|[a-zA-Z]+))"
)

_FUNKTIONER = {"sin", "cos", "tan", "lg", "log", "ln", "sqrt", "rot"}
_KONSTANTER = {"pi", "π"}


def _tokenisera(text: str) -> list[tuple[str, str]]:
    text = text.strip().replace("−", "-").replace("÷", ":")
    pos, ut = 0, []
    while pos < len(text):
        m = _TOKEN.match(text, pos)
        if not m or m.end() == pos:
            raise Uttrycksfel(f"Förstår inte tecknet '{text[pos]}'")
        pos = m.end()
        if m.group("tal") is not None:
            ut.append(("tal", m.group("tal").replace(",", ".")))
        elif m.group("op") is not None:
            op = m.group("op")
            op = {"·": "*", "×": "*", ":": "/"}.get(op, op)
            ut.append(("op", op))
        elif m.group("namn") is not None:
            namn = m.group("namn").lower()
            if namn not in _FUNKTIONER and namn not in _KONSTANTER:
                raise Uttrycksfel(f"Okänt ord '{m.group('namn')}'")   # x, y … → algebra.py tar över
            ut.append(("namn", namn))
        else:
            ut.append(("proc", "%"))
    return ut


def _heltalsrot(n: int, q: int) -> int | None:
    """Exakt q:te rot ur ett icke-negativt heltal, annars None."""
    if n < 0:
        return None
    r = round(n ** (1.0 / q))
    for k in (r - 1, r, r + 1):
        if k >= 0 and k ** q == n:
            return k
    return None


def _ungefar(x: float) -> Fraction:
    if x != x or x in (float("inf"), float("-inf")):
        raise Uttrycksfel("Talet går inte att räkna ut")
    return Fraction(x).limit_denominator(10 ** 9)


def _potens(bas: Fraction, e: Fraction) -> Fraction:
    if e.denominator == 1:
        if bas == 0 and e < 0:
            raise Uttrycksfel("Division med noll")
        return bas ** int(e)
    q = e.denominator
    if bas < 0:
        if q % 2 == 1:                                  # udda rot ur negativt tal: (-8)^(1/3) = -2
            return -_potens(-bas, e)
        raise Uttrycksfel("Jämn rot ur ett negativt tal")
    t, n = _heltalsrot(bas.numerator, q), _heltalsrot(bas.denominator, q)
    if t is not None and n is not None:
        return Fraction(t, n) ** e.numerator
    return _ungefar(float(bas) ** float(e))


def _funktion(namn: str, v: Fraction) -> Fraction:
    import math
    if namn in ("sqrt", "rot"):
        return _potens(v, Fraction(1, 2))
    if namn in ("sin", "cos", "tan"):
        grader = v % 360
        exakt = {("sin", 0): 0, ("sin", 30): Fraction(1, 2), ("sin", 90): 1, ("sin", 150): Fraction(1, 2),
                 ("sin", 180): 0, ("sin", 270): -1, ("cos", 0): 1, ("cos", 60): Fraction(1, 2), ("cos", 90): 0,
                 ("cos", 180): -1, ("cos", 270): 0, ("cos", 300): Fraction(1, 2), ("tan", 0): 0, ("tan", 45): 1,
                 ("tan", 180): 0, ("tan", 225): 1}
        if (namn, grader) in exakt:
            return Fraction(exakt[(namn, grader)])
        return _ungefar(getattr(math, namn)(math.radians(float(v))))
    if v <= 0:
        raise Uttrycksfel("Logaritmen finns bara för positiva tal")
    if namn in ("lg", "log"):
        heltal = round(math.log10(float(v)))
        if Fraction(10) ** heltal == v:
            return Fraction(heltal)
        return _ungefar(math.log10(float(v)))
    return _ungefar(math.log(float(v)))


class _Tolk:
    """Rekursiv nedstigning: uttryck → term → faktor → potens → primär."""

    def __init__(self, tokens: list[tuple[str, str]]):
        self.t = tokens
        self.i = 0

    def kika(self):
        return self.t[self.i] if self.i < len(self.t) else (None, None)

    def ta(self):
        tok = self.kika()
        self.i += 1
        return tok

    def uttryck(self) -> Fraction:
        v = self.term()
        while self.kika() == ("op", "+") or self.kika() == ("op", "-"):
            op = self.ta()[1]
            h = self.term()
            v = v + h if op == "+" else v - h
        return v

    def term(self) -> Fraction:
        v = self.potens()
        while self.kika() in (("op", "*"), ("op", "/")):
            op = self.ta()[1]
            h = self.potens()
            if op == "/":
                if h == 0:
                    raise Uttrycksfel("Division med noll")
                v = v / h
            else:
                v = v * h
        return v

    def potens(self) -> Fraction:
        v = self.primar()
        if self.kika() == ("op", "^"):
            self.ta()
            e = self.potens_hoger()
            v = _potens(v, e)
        return v

    def potens_hoger(self) -> Fraction:
        """Exponenten: tillåter tecken direkt (2^-1) och är högerassociativ (2^3^2 = 2^9)."""
        if self.kika() == ("op", "-"):
            self.ta(); return -self.potens_hoger()
        return self.potens()

    def primar(self) -> Fraction:
        slag, v = self.kika()
        if slag == "op" and v == "√":
            self.ta()
            return _potens(self.primar(), Fraction(1, 2))
        if slag == "namn":
            self.ta()
            if v in _KONSTANTER:
                return self._efter(_ungefar(3.141592653589793))
            return self._efter(_funktion(v, self.primar()))
        if slag == "op" and v == "-":
            self.ta()
            return -self.primar()
        if slag == "op" and v == "+":
            self.ta()
            return self.primar()
        if slag == "op" and v == "(":
            self.ta()
            inre = self.uttryck()
            if self.ta() != ("op", ")"):
                raise Uttrycksfel("Saknar )")
            return self._efter(inre)
        if slag == "tal":
            self.ta()
            tal = Fraction(v)
            # blandad form: "1 1/2"
            if (self.kika()[0] == "tal" and self.i + 2 < len(self.t)
                    and self.t[self.i + 1] == ("op", "/")
                    and self.t[self.i + 2][0] == "tal"):
                talj = Fraction(self.ta()[1]); self.ta(); namn = Fraction(self.ta()[1])
                if namn == 0:
                    raise Uttrycksfel("Nämnaren kan inte vara noll")
                tal = tal + talj / namn if tal >= 0 else tal - talj / namn
            return self._efter(tal)
        raise Uttrycksfel("Väntade ett tal")

    def _efter(self, v: Fraction) -> Fraction:
        if self.kika() == ("proc", "%"):
            self.ta()
            return v / 100
        return v


def tolka(text: str) -> Fraction:
    """Räknar ut ett uttryck exakt. Kastar Uttrycksfel om det inte går."""
    if not text or not text.strip():
        raise Uttrycksfel("Inget svar")
    tokens = _tokenisera(text)
    tolk = _Tolk(tokens)
    v = tolk.uttryck()
    if tolk.i != len(tokens):
        raise Uttrycksfel("Något blev över i uttrycket")
    return v


def likvardiga(svar: str, facit: str | Fraction) -> bool:
    """Är elevens svar samma tal som facit, oavsett form?"""
    try:
        a = tolka(svar)
    except Uttrycksfel:
        return False
    b = facit if isinstance(facit, Fraction) else tolka(str(facit))
    return a == b


def formatera(v: Fraction, form: str = "auto") -> str:
    """Skriver ett tal i vald form: brak, decimal, procent, auto."""
    if form == "procent":
        p = v * 100
        return (f"{p.numerator}" if p.denominator == 1 else formatera(p, "decimal")) + " %"
    if form == "brak":
        if v.denominator == 1:
            return str(v.numerator)
        return f"{v.numerator}/{v.denominator}"
    if form == "decimal":
        d = v.denominator
        # Exakt decimal bara om nämnaren är 2^a·5^b
        n = d
        while n % 2 == 0: n //= 2
        while n % 5 == 0: n //= 5
        if n == 1:
            s = f"{float(v):.10f}".rstrip("0").rstrip(".")
        else:
            s = f"{float(v):.3f}".rstrip("0").rstrip(".")
        return s.replace(".", ",")
    if v.denominator == 1:
        return str(v.numerator)
    return formatera(v, "decimal") if _exakt_decimal(v) else formatera(v, "brak")


def _exakt_decimal(v: Fraction) -> bool:
    n = v.denominator
    while n % 2 == 0: n //= 2
    while n % 5 == 0: n //= 5
    return n == 1


def former(v: Fraction) -> dict[str, str]:
    """Samma tal i alla tre formerna – som tabellen 'Bråkform, procentform, decimalform'."""
    return {"brak": formatera(v, "brak"),
            "procent": formatera(v, "procent"),
            "decimal": formatera(v, "decimal")}
