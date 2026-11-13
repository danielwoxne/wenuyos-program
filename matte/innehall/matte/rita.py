"""Ritning av figurer – bråkstaplar, hundrarutor, positionstabeller, tallinjer.

Figurerna beskrivs som små ordböcker (från kortens TOML-filer) och ritas på
en *duk*. Det finns två dukar med samma gränssnitt:

  SvgDuk    – ger en SVG-sträng (förhandsbilder, utskrift av regelboken, tester)
  CairoDuk  – ritar i ett GTK-fönster (används av fonster.py)

Så ser korten exakt likadana ut på skärmen, i tester och på papper.
"""
from __future__ import annotations

from dataclasses import dataclass

from . import tema

# ---------------------------------------------------------------- dukar


class Duk:
    """Gemensamt gränssnitt. Koordinater i pixlar, origo uppe till vänster."""

    def rekt(self, x, y, b, h, fyll=None, kant=None, tjock=1.5): ...
    def linje(self, x1, y1, x2, y2, farg=tema.LINJE, tjock=1.5): ...
    def text(self, x, y, s, storlek=14, fet=False, farg=tema.TEXT,
             ankare="start", lutande=False): ...
    def pil(self, x1, y1, x2, y2, farg=tema.ROD): ...
    def bild(self, x, y, b, h, sokvag): ...
    def ring(self, x, y, b, h, farg=tema.ACCENT, tjock=4): ...
    def bricka(self, cx, cy, r, farg): ...


class SvgDuk(Duk):
    def __init__(self, bredd: int, hojd: int):
        self.b, self.h = bredd, hojd
        self.delar: list[str] = []

    def rekt(self, x, y, b, h, fyll=None, kant=None, tjock=1.5):
        f = fyll or "none"
        k = kant or "none"
        self.delar.append(
            f'<rect x="{x:.1f}" y="{y:.1f}" width="{b:.1f}" height="{h:.1f}" '
            f'fill="{f}" stroke="{k}" stroke-width="{tjock}"/>')

    def linje(self, x1, y1, x2, y2, farg=tema.LINJE, tjock=1.5):
        self.delar.append(
            f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" '
            f'stroke="{farg}" stroke-width="{tjock}" stroke-linecap="round"/>')

    def text(self, x, y, s, storlek=14, fet=False, farg=tema.TEXT,
             ankare="start", lutande=False):
        s = (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))
        vikt = ' font-weight="600"' if fet else ""
        rot = f' transform="rotate(-90 {x:.1f} {y:.1f})"' if lutande else ""
        ank = {"start": "start", "mitt": "middle", "slut": "end"}[ankare]
        self.delar.append(
            f'<text x="{x:.1f}" y="{y:.1f}" font-family="{tema.TYPSNITT}" '
            f'font-size="{storlek}" fill="{farg}" text-anchor="{ank}"{vikt}{rot}>{s}</text>')

    def pil(self, x1, y1, x2, y2, farg=tema.ROD):
        self.linje(x1, y1, x2, y2, farg, 2.5)
        # pilspets
        import math
        v = math.atan2(y2 - y1, x2 - x1)
        for d in (0.5, -0.5):
            self.linje(x2, y2, x2 - 9 * math.cos(v + d), y2 - 9 * math.sin(v + d), farg, 2.5)

    def bild(self, x, y, b, h, sokvag):
        import base64
        with open(sokvag, "rb") as f:
            data = base64.b64encode(f.read()).decode()
        mime = "image/jpeg" if str(sokvag).lower().endswith((".jpg", ".jpeg")) else "image/png"
        self.delar.append(f'<image x="{x:.1f}" y="{y:.1f}" width="{b:.1f}" height="{h:.1f}" '
                          f'preserveAspectRatio="none" href="data:{mime};base64,{data}"/>')

    def bricka(self, cx, cy, r, farg):
        self.delar.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" fill="{farg}" stroke="#00000033" stroke-width="2"/>')

    def ring(self, x, y, b, h, farg=tema.ACCENT, tjock=4):
        self.delar.append(f'<ellipse cx="{x + b / 2:.1f}" cy="{y + h / 2:.1f}" rx="{b / 2:.1f}" ry="{h / 2:.1f}" '
                          f'fill="none" stroke="{farg}" stroke-width="{tjock}"/>')

    def svg(self) -> str:
        inre = "\n".join(self.delar)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.b}" height="{self.h}" '
                f'viewBox="0 0 {self.b} {self.h}">\n{inre}\n</svg>')


class CairoDuk(Duk):
    """Samma anrop, men ritade med cairo i ett GTK-fönster."""

    def __init__(self, cr):
        self.cr = cr

    @staticmethod
    def _rgb(hexfarg):
        h = hexfarg.lstrip("#")
        return tuple(int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))

    def rekt(self, x, y, b, h, fyll=None, kant=None, tjock=1.5):
        cr = self.cr
        cr.rectangle(x, y, b, h)
        if fyll:
            cr.set_source_rgb(*self._rgb(fyll))
            cr.fill_preserve()
        if kant:
            cr.set_source_rgb(*self._rgb(kant))
            cr.set_line_width(tjock)
            cr.stroke()
        else:
            cr.new_path()

    def linje(self, x1, y1, x2, y2, farg=tema.LINJE, tjock=1.5):
        cr = self.cr
        cr.set_source_rgb(*self._rgb(farg))
        cr.set_line_width(tjock)
        cr.move_to(x1, y1); cr.line_to(x2, y2); cr.stroke()

    def text(self, x, y, s, storlek=14, fet=False, farg=tema.TEXT,
             ankare="start", lutande=False):
        import cairo
        cr = self.cr
        cr.save()
        cr.select_font_face(tema.TYPSNITT.split(",")[0].strip(),
                            cairo.FONT_SLANT_NORMAL,
                            cairo.FONT_WEIGHT_BOLD if fet else cairo.FONT_WEIGHT_NORMAL)
        cr.set_font_size(storlek)
        cr.set_source_rgb(*self._rgb(farg))
        ext = cr.text_extents(s)
        dx = {"start": 0, "mitt": -ext.width / 2, "slut": -ext.width}[ankare]
        cr.translate(x, y)
        if lutande:
            cr.rotate(-3.14159265 / 2)
        cr.move_to(dx, 0)
        cr.show_text(s)
        cr.restore()

    def pil(self, x1, y1, x2, y2, farg=tema.ROD):
        import math
        self.linje(x1, y1, x2, y2, farg, 2.5)
        v = math.atan2(y2 - y1, x2 - x1)
        for d in (0.5, -0.5):
            self.linje(x2, y2, x2 - 9 * math.cos(v + d), y2 - 9 * math.sin(v + d), farg, 2.5)


    def bild(self, x, y, b, h, sokvag):
        cr = self.cr
        cr.save()
        if str(sokvag).lower().endswith(".png"):
            import cairo
            yta = _png_cache.get(sokvag)
            if yta is None:
                yta = _png_cache[sokvag] = cairo.ImageSurface.create_from_png(str(sokvag))
            cr.translate(x, y)
            cr.scale(b / yta.get_width(), h / yta.get_height())
            cr.set_source_surface(yta, 0, 0)
        else:                                       # JPEG (målningarna): GdkPixbuf, skalad en gång och cachad
            import gi
            gi.require_version("GdkPixbuf", "2.0"); gi.require_version("Gdk", "4.0")
            from gi.repository import Gdk, GdkPixbuf
            nyckel = (str(sokvag), int(b), int(h))
            pb = _png_cache.get(nyckel)
            if pb is None:
                pb = _png_cache[nyckel] = GdkPixbuf.Pixbuf.new_from_file_at_scale(str(sokvag), int(b), int(h), False)
            Gdk.cairo_set_source_pixbuf(cr, pb, x, y)
            cr.rectangle(x, y, b, h); cr.clip()
        cr.paint()
        cr.restore()

    def bricka(self, cx, cy, r, farg):
        import math
        cr = self.cr
        cr.arc(cx, cy, r, 0, 2 * math.pi)
        cr.set_source_rgb(*self._rgb(farg)); cr.fill_preserve()
        cr.set_source_rgba(0, 0, 0, 0.2); cr.set_line_width(2); cr.stroke()

    def ring(self, x, y, b, h, farg=tema.ACCENT, tjock=4):
        import math
        cr = self.cr
        cr.save()
        cr.translate(x + b / 2, y + h / 2); cr.scale(b / 2, h / 2)
        cr.arc(0, 0, 1, 0, 2 * math.pi)
        cr.restore()
        cr.set_source_rgb(*self._rgb(farg)); cr.set_line_width(tjock); cr.stroke()


_png_cache: dict = {}


# ---------------------------------------------------------------- figurer


@dataclass
class Figur:
    """En ritbar figur med känd storlek."""
    bredd: int
    hojd: int
    rita: object  # funktion (duk, x, y) -> None

    def svg(self) -> str:
        d = SvgDuk(self.bredd, self.hojd)
        self.rita(d, 0, 0)
        return d.svg()


def stapel(delar: int, fyllda: int = 1, farg: str = "gul",
           bredd: int = 420, hojd: int = 40) -> Figur:
    """En hel delad i lika stora delar – som bråksidan i regelboken."""
    delar = max(1, int(delar)); fyllda = max(0, min(int(fyllda), delar))
    f = tema.farg(farg)

    def rita(duk, x, y):
        db = bredd / delar
        for i in range(delar):
            duk.rekt(x + i * db, y, db, hojd, fyll=f if i < fyllda else tema.VIT)
        for i in range(delar):
            duk.rekt(x + i * db, y, db, hojd, kant=tema.LINJE)
    return Figur(bredd, hojd, rita)


def hundraruta(fyllda: int = 1, farg: str = "lila", ruta: int = 22) -> Figur:
    """10 × 10 rutor. Fyller radvis uppifrån vänster."""
    fyllda = max(0, min(int(fyllda), 100)); f = tema.farg(farg)

    def rita(duk, x, y):
        for i in range(100):
            r, k = divmod(i, 10)
            duk.rekt(x + k * ruta, y + r * ruta, ruta, ruta,
                     fyll=f if i < fyllda else tema.VIT, kant=tema.LINJE, tjock=1.2)
    return Figur(ruta * 10, ruta * 10, rita)


def tiostaplar(fyllda: int = 1, farg: str = "orange", bredd: int = 110, hojd: int = 64) -> Figur:
    """Tio stående staplar – bilden i tabellen bråk/procent/decimal."""
    return _staende(10, fyllda, farg, bredd, hojd)


def _staende(delar, fyllda, farg, bredd, hojd):
    delar = max(1, int(delar)); fyllda = max(0, min(int(fyllda), delar)); f = tema.farg(farg)

    def rita(duk, x, y):
        db = bredd / delar
        for i in range(delar):
            duk.rekt(x + i * db, y, db, hojd, fyll=f if i < fyllda else tema.VIT, kant=tema.LINJE)
    return Figur(bredd, hojd, rita)


def staende(delar: int, fyllda: int = 1, farg: str = "rod", bredd: int = 110, hojd: int = 64) -> Figur:
    return _staende(delar, fyllda, farg, bredd, hojd)


_POSITIONER = ["tusental", "hundratal", "tiotal", "ental", "tiondelar", "hundradelar", "tusendelar"]


def positionstabell(tal: str, skift: int = 0, ruta: int = 52) -> Figur:
    """Positionssystemet: talet i rutor, och – om skift ≠ 0 – samma tal flyttat
    `skift` steg (åt vänster = större) med röda pilar. Som sidan om · 10, · 100."""
    tal = str(tal).replace(".", ",")
    heltal, _, dec = tal.partition(",")
    siffror = list(heltal) + list(dec)
    pos0 = 4 - len(heltal)            # kolumn för första siffran (ental = kolumn 3)
    topp, mellan = 96, 34             # plats för lutande rubriker; mellanrum för pilar
    rader = 2 if skift else 1
    hojd = topp + rader * ruta + (mellan if skift else 0) + 8

    def satt(duk, x, yy, start, komma_efter, siffror=siffror):
        for i, s in enumerate(siffror):
            kol = start + i
            if 0 <= kol < 7:
                duk.text(x + kol * ruta + ruta / 2, yy + ruta * 0.72, s, storlek=30, fet=True, ankare="mitt")
                if i + 1 == komma_efter and i + 1 < len(siffror):
                    duk.text(x + (kol + 1) * ruta - 3, yy + ruta * 0.72, ",", storlek=30, fet=True, ankare="mitt")

    def rita(duk, x, y):
        for k, namn in enumerate(_POSITIONER):
            duk.text(x + k * ruta + ruta / 2 + 5, y + topp - 8, namn, storlek=12, ankare="start", lutande=True)
        y1 = y + topp
        y2 = y1 + ruta + mellan
        for yy in ([y1, y2] if skift else [y1]):
            for k in range(7):
                duk.rekt(x + k * ruta, yy, ruta, ruta, fyll=tema.VIT, kant=tema.LINJE)
            duk.linje(x + 4 * ruta, yy, x + 4 * ruta, yy + ruta, tema.LINJE, 3)
        satt(duk, x, y1, pos0, len(heltal))
        if skift:
            start2 = pos0 - skift
            # ett heltal som flyttas får nollor i de tomma platserna: 7 · 10 = 70
            siffror2 = siffror + list("0" * max(0, skift - len(dec)))
            satt(duk, x, y2, start2, len(heltal) + skift, siffror2)
            for i in range(len(siffror)):
                k1, k2 = pos0 + i, start2 + i
                if 0 <= k1 < 7 and 0 <= k2 < 7:
                    duk.pil(x + k1 * ruta + ruta / 2, y1 + ruta + 4,
                            x + k2 * ruta + ruta / 2, y2 - 4)
    return Figur(ruta * 7, hojd, rita)


def tallinje(fran=0, till=10, markera: list | None = None, steg=1,
             bredd: int = 420, etikett=1) -> Figur:
    """En tallinje. `fran`/`till` kan vara negativa eller decimaltal, `steg` är
    avståndet mellan strecken, `etikett` hur ofta siffror skrivs (varje n:e streck).
    `markera` = tal att pricka in (röda rutor)."""
    from fractions import Fraction as F
    from .uttryck import tolka
    fran, till, steg = tolka(str(fran)), tolka(str(till)), tolka(str(steg))
    markera = [tolka(str(m)) for m in (markera or [])]
    hojd = 50
    n = int((till - fran) / steg)

    def txt(v: F) -> str:
        return (str(v.numerator) if v.denominator == 1 else f"{float(v):g}").replace(".", ",")

    def rita(duk, x, y):
        y0 = y + 24
        x = x + 18; b = bredd - 36
        duk.linje(x, y0, x + b, y0, tema.LINJE, 2)
        for i in range(n + 1):
            xx = x + i * b / n
            stor = i % etikett == 0
            duk.linje(xx, y0 - (7 if stor else 4), xx, y0 + (7 if stor else 4), tema.LINJE, 2 if stor else 1.2)
            if stor:
                duk.text(xx, y0 + 22, txt(fran + i * steg), storlek=12, ankare="mitt")
        for v in markera:
            xx = x + float((v - fran) / (till - fran)) * b
            duk.rekt(xx - 5, y0 - 5, 10, 10, fyll=tema.ROD)
    return Figur(bredd, hojd, rita)


def brak(taljare, namnare, storlek: int = 26) -> Figur:
    """Ett bråk skrivet med bråkstreck."""
    t, n = str(taljare), str(namnare)
    b = max(len(t), len(n)) * storlek * 0.62 + 10
    h = storlek * 2.4

    def rita(duk, x, y):
        duk.text(x + b / 2, y + storlek * 0.95, t, storlek=storlek, ankare="mitt")
        duk.linje(x + 2, y + storlek * 1.15, x + b - 2, y + storlek * 1.15, tema.TEXT, 2)
        duk.text(x + b / 2, y + storlek * 2.15, n, storlek=storlek, ankare="mitt")
    return Figur(int(b), int(h), rita)


def rektangel(bredd: int = 5, hojd: int = 3, ruta: int = 30, farg: str = "gron",
              rutnat: bool = True, matt: bool = True) -> Figur:
    """En rektangel på rutnät – för area och omkrets. bredd/hojd i rutor."""
    b, h = int(bredd), int(hojd); f = tema.farg(farg)
    marg = 46 if matt else 2

    def rita(duk, x, y):
        duk.rekt(x + marg, y + 4, b * ruta, h * ruta, fyll=f)
        if rutnat:
            for i in range(b + 1):
                duk.linje(x + marg + i * ruta, y + 4, x + marg + i * ruta, y + 4 + h * ruta, tema.LINJE, 1)
            for j in range(h + 1):
                duk.linje(x + marg, y + 4 + j * ruta, x + marg + b * ruta, y + 4 + j * ruta, tema.LINJE, 1)
        duk.rekt(x + marg, y + 4, b * ruta, h * ruta, kant=tema.LINJE, tjock=2)
        if matt:
            duk.text(x + marg + b * ruta / 2, y + 4 + h * ruta + 18, f"{b} cm", storlek=13, ankare="mitt")
            duk.text(x + marg - 8, y + 4 + h * ruta / 2 + 5, f"{h} cm", storlek=13, ankare="slut")
    return Figur(marg + b * ruta + 6, h * ruta + (28 if matt else 10), rita)


def triangel(bas: int = 6, hojd: int = 4, ruta: int = 28, farg: str = "rosa") -> Figur:
    """En triangel med bas och höjd utsatta (höjden streckad)."""
    b, h = int(bas), int(hojd); f = tema.farg(farg)
    marg = 4; topp_x = b * ruta * 0.35

    def rita(duk, x, y):
        x0, y0 = x + marg, y + 6 + h * ruta
        # fylld triangel ritas som tunna lodräta remsor (duken har bara rektanglar)
        for i in range(b * ruta):
            xx = i
            if xx <= topp_x:
                hh = h * ruta * xx / topp_x
            else:
                hh = h * ruta * (b * ruta - xx) / (b * ruta - topp_x)
            duk.rekt(x0 + xx, y0 - hh, 1.2, hh, fyll=f)
        duk.linje(x0, y0, x0 + b * ruta, y0, tema.LINJE, 2)
        duk.linje(x0, y0, x0 + topp_x, y0 - h * ruta, tema.LINJE, 2)
        duk.linje(x0 + b * ruta, y0, x0 + topp_x, y0 - h * ruta, tema.LINJE, 2)
        for yy in range(0, h * ruta, 8):
            duk.linje(x0 + topp_x, y0 - yy, x0 + topp_x, y0 - min(yy + 4, h * ruta), tema.ROD, 1.5)
        duk.text(x0 + b * ruta / 2, y0 + 18, f"bas {b} cm", storlek=13, ankare="mitt")
        duk.text(x0 + topp_x + 6, y0 - h * ruta / 2, f"höjd {h} cm", storlek=13, farg=tema.ROD)
    return Figur(b * ruta + marg * 2 + 80, h * ruta + 30, rita)


def stapeldiagram(varden: list | None = None, namn: list | None = None, farg: str = "himmel",
                  bredd: int = 360, hojd: int = 180, rubrik: str = "") -> Figur:
    """Ett stapeldiagram med y-axel och etiketter."""
    varden = [float(v) for v in (varden or [3, 5, 2])]
    namn = list(namn or [str(i + 1) for i in range(len(varden))])
    f = tema.farg(farg); maxv = max(varden) if varden else 1
    steg = 1 if maxv <= 10 else 2 if maxv <= 20 else 5 if maxv <= 50 else 10
    topp = int(-(-maxv // steg) * steg) or steg
    vm, nm = 36, 30  # marginaler vänster, nedre
    h_topp = 22 if rubrik else 6

    def rita(duk, x, y):
        y0 = y + hojd - nm; yt = y + h_topp
        duk.linje(x + vm, yt, x + vm, y0, tema.LINJE, 1.5)
        duk.linje(x + vm, y0, x + bredd, y0, tema.LINJE, 1.5)
        for v in range(0, topp + 1, steg):
            yy = y0 - (y0 - yt) * v / topp
            duk.linje(x + vm - 4, yy, x + bredd, yy, "#d8d8d8" if v else tema.LINJE, 1)
            duk.text(x + vm - 8, yy + 4, str(v), storlek=11, ankare="slut")
        n = len(varden); bb = (bredd - vm) / n
        for i, v in enumerate(varden):
            hh = (y0 - yt) * v / topp
            duk.rekt(x + vm + i * bb + bb * 0.2, y0 - hh, bb * 0.6, hh, fyll=f, kant=tema.LINJE)
            duk.text(x + vm + i * bb + bb / 2, y0 + 16, namn[i], storlek=12, ankare="mitt")
        if rubrik:
            duk.text(x + vm + (bredd - vm) / 2, y + 14, rubrik, storlek=13, fet=True, ankare="mitt")
    return Figur(bredd + 4, hojd, rita)


def koordinatsystem(xmax: int = 6, ymax: int = 6, punkter: list | None = None,
                    linje: list | None = None, ruta: int = 32, xmin: int = 0, ymin: int = 0,
                    linjer: list | None = None, funktioner: list | None = None) -> Figur:
    """Koordinatsystem med rutnät. `punkter` = [[x,y], …], `linje` = [k, m] ritar y = kx + m,
    `linjer` = flera sådana, `funktioner` = ["x^2 - 2x - 3", …] ritar graferna."""
    xmin, xmax, ymin, ymax = int(xmin), int(xmax), int(ymin), int(ymax)
    kol, rad = xmax - xmin, ymax - ymin
    marg = 24

    def px(vx): return marg + (vx - xmin) * ruta
    def py(vy): return 6 + (ymax - vy) * ruta

    def rita(duk, x, y):
        for i in range(kol + 1):
            duk.linje(x + px(xmin + i), y + py(ymax), x + px(xmin + i), y + py(ymin), "#d8d8d8", 1)
        for j in range(rad + 1):
            duk.linje(x + px(xmin), y + py(ymin + j), x + px(xmax), y + py(ymin + j), "#d8d8d8", 1)
        # axlar genom origo om det syns, annars i kanten
        ax_y = py(0) if ymin <= 0 <= ymax else py(ymin)
        ax_x = px(0) if xmin <= 0 <= xmax else px(xmin)
        duk.linje(x + px(xmin), y + ax_y, x + px(xmax), y + ax_y, tema.LINJE, 2)
        duk.linje(x + ax_x, y + py(ymax), x + ax_x, y + py(ymin), tema.LINJE, 2)
        for i in range(kol + 1):
            v = xmin + i
            if v: duk.text(x + px(v), y + ax_y + 16, str(v), storlek=11, ankare="mitt")
        for j in range(rad + 1):
            v = ymin + j
            if v: duk.text(x + ax_x - 6, y + py(v) + 4, str(v), storlek=11, ankare="slut")
        duk.text(x + px(xmax) - 2, y + ax_y - 6, "x", storlek=12, ankare="slut")
        duk.text(x + ax_x + 8, y + py(ymax) + 10, "y", storlek=12)
        farger = [tema.ACCENT, tema.FARGER["himmel"], tema.FARGER["skog"]]
        for nr, uttr in enumerate(funktioner or []):
            from .algebra import tolka_uttryck, varde
            p = tolka_uttryck(str(uttr))
            steg = 60 * (xmax - xmin)
            punkter_ = []
            for i in range(steg + 1):
                vx = xmin + (xmax - xmin) * i / steg
                vy = varde(p, {"x": vx})
                punkter_.append((vx, vy))
            for (x1, y1), (x2, y2) in zip(punkter_, punkter_[1:]):
                if ymin - 0.5 <= y1 <= ymax + 0.5 and ymin - 0.5 <= y2 <= ymax + 0.5:
                    duk.linje(x + px(x1), y + py(max(ymin, min(ymax, y1))), x + px(x2), y + py(max(ymin, min(ymax, y2))),
                              farger[nr % 3], 2.5)
        for nr, lm in enumerate(([linje] if linje else []) + list(linjer or [])):
            k, m = float(lm[0]), float(lm[1])
            # klipp linjen till rutan (Liang–Barsky i miniatyr: prova kanterna)
            kand = []
            for vx in (xmin, xmax):
                vy = k * vx + m
                if ymin - 1e-9 <= vy <= ymax + 1e-9:
                    kand.append((vx, vy))
            if k != 0:
                for vy in (ymin, ymax):
                    vx = (vy - m) / k
                    if xmin - 1e-9 <= vx <= xmax + 1e-9:
                        kand.append((vx, vy))
            kand = sorted(set((round(a, 6), round(b, 6)) for a, b in kand))
            if len(kand) >= 2:
                (x1, y1), (x2, y2) = kand[0], kand[-1]
                duk.linje(x + px(x1), y + py(y1), x + px(x2), y + py(y2), farger[nr % 3], 2.5)
        for p in (punkter or []):
            duk.rekt(x + px(float(p[0])) - 5, y + py(float(p[1])) - 5, 10, 10, fyll=tema.ROD)
    return Figur(marg + kol * ruta + 16, rad * ruta + 30, rita)


def tiobas(tiotal: int = 2, ental: int = 3, farg: str = "orange", ruta: int = 14) -> Figur:
    """Tiobasmaterial: tiostavar och entalskuber – som på lågstadiet."""
    t, e = int(tiotal), int(ental); f = tema.farg(farg)
    bredd = t * (ruta + 8) + (e * (ruta + 4) if e else 0) + 8

    def rita(duk, x, y):
        xx = x + 4
        for _ in range(t):
            for j in range(10):
                duk.rekt(xx, y + 4 + j * ruta, ruta, ruta, fyll=f, kant=tema.LINJE, tjock=1)
            xx += ruta + 8
        xx += 6
        for i in range(e):
            duk.rekt(xx + (i % 5) * (ruta + 4), y + 4 + 9 * ruta - (i // 5) * (ruta + 4), ruta, ruta, fyll=f, kant=tema.LINJE, tjock=1)
    return Figur(max(bredd, 5 * (ruta + 4) + t * (ruta + 8) + 14), 10 * ruta + 10, rita)


def prickar(antal: int = 7, farg: str = "himmel", per_rad: int = 5, d: int = 26) -> Figur:
    """Prickar att räkna – i rader om fem (så man ser femmor utan att räkna en och en)."""
    n = int(antal); f = tema.farg(farg)
    rader = -(-n // per_rad) or 1

    def rita(duk, x, y):
        for i in range(n):
            r, k = divmod(i, per_rad)
            duk.rekt(x + 4 + k * (d + 6), y + 4 + r * (d + 6), d, d, fyll=f, kant=tema.LINJE, tjock=1)
    return Figur(per_rad * (d + 6) + 8, rader * (d + 6) + 8, rita)


def talkamrater(summa: int = 10, del1: int = 4, farg1: str = "gul", farg2: str = "himmel",
                d: int = 26, visa: bool = True) -> Figur:
    """En rad prickar i två färger: del1 + (summa − del1) = summa. `visa` = skriv ut likheten."""
    n, a = int(summa), int(del1)

    def rita(duk, x, y):
        for i in range(n):
            duk.rekt(x + 4 + i * (d + 4), y + 4, d, d, fyll=tema.farg(farg1 if i < a else farg2), kant=tema.LINJE, tjock=1)
        if visa:
            duk.text(x + 4 + n * (d + 4) + 6, y + 4 + d * 0.75, f"{a} + {n - a} = {n}", storlek=16)
    return Figur(n * (d + 4) + (130 if visa else 8), d + 10, rita)


def klocka(timme: int = 3, minut: int = 0, r: int = 56) -> Figur:
    """En analog klocka."""
    import math
    h, m = int(timme) % 12, int(minut) % 60

    def rita(duk, x, y):
        cx, cy = x + r + 4, y + r + 4
        for i in range(60):
            v = math.radians(i * 6 - 90); L = 8 if i % 5 == 0 else 4
            duk.linje(cx + (r - L) * math.cos(v), cy + (r - L) * math.sin(v), cx + r * math.cos(v), cy + r * math.sin(v), tema.LINJE, 2 if i % 5 == 0 else 1)
        for i in range(1, 13):
            v = math.radians(i * 30 - 90)
            duk.text(cx + (r - 20) * math.cos(v), cy + (r - 20) * math.sin(v) + 5, str(i), storlek=13, ankare="mitt")
        vh = math.radians((h + m / 60) * 30 - 90); vm = math.radians(m * 6 - 90)
        duk.linje(cx, cy, cx + (r * 0.5) * math.cos(vh), cy + (r * 0.5) * math.sin(vh), tema.TEXT, 4)
        duk.linje(cx, cy, cx + (r * 0.78) * math.cos(vm), cy + (r * 0.78) * math.sin(vm), tema.ROD, 2.5)
    return Figur(2 * r + 8, 2 * r + 8, rita)


def tabell(rubriker: list | None = None, rader: list | None = None, kolbredd: int = 90, radhojd: int = 28) -> Figur:
    """En enkel tabell (frekvenstabell, värdetabell)."""
    rub = list(rubriker or []); rr = [list(map(str, r)) for r in (rader or [])]
    kol = len(rub) if rub else (len(rr[0]) if rr else 1)
    alla = ([rub] if rub else []) + rr

    def rita(duk, x, y):
        for i, rad in enumerate(alla):
            for j, cell in enumerate(rad):
                duk.rekt(x + j * kolbredd, y + i * radhojd, kolbredd, radhojd, fyll=tema.ACCENT_SVAG if (rub and i == 0) else tema.VIT, kant=tema.LINJE, tjock=1)
                duk.text(x + j * kolbredd + kolbredd / 2, y + i * radhojd + radhojd * 0.68, str(cell), storlek=13, fet=bool(rub and i == 0), ankare="mitt")
    return Figur(kol * kolbredd + 2, len(alla) * radhojd + 2, rita)


def ikoner(namn: str = "apple", antal: int = 5, per_rad: int = 5, d: int = 48) -> Figur:
    """Daniels lerikoner som räknebilder: fem äpplen, tre tärningar …"""
    from .ikoner import sokvag
    n = int(antal); p = sokvag(namn)
    rader = -(-n // per_rad) or 1

    def rita(duk, x, y):
        for i in range(n):
            r, k = divmod(i, per_rad)
            duk.bild(x + 4 + k * (d + 6), y + 4 + r * (d + 6), d, d, p)
    return Figur(min(n, per_rad) * (d + 6) + 8, rader * (d + 6) + 8, rita)


def bildstorlek(sokvag) -> tuple[int, int]:
    """Bredd och höjd ur PNG- eller JPEG-huvudet – utan bildbibliotek."""
    import struct
    with open(sokvag, "rb") as f:
        data = f.read(64 * 1024)
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        return struct.unpack(">II", data[16:24])
    i = 2
    while i < len(data) - 9:                        # JPEG: leta upp SOF-markören
        if data[i] != 0xFF:
            i += 1; continue
        m = data[i + 1]
        if m in (0xC0, 0xC1, 0xC2):
            h, b = struct.unpack(">HH", data[i + 5:i + 9]); return b, h
        i += 2 + struct.unpack(">H", data[i + 2:i + 4])[0]
    return 16, 9


def scen(bild: str, bredd: int = 560, omraden: list | None = None, namn: list | None = None,
         visa_omraden: bool = False) -> Figur:
    """En av Daniels målningar som uppgiftsbild. `omraden` = klickytor [[x, y, b, h], …] i
    andelar av bilden (0–1), så att de håller i alla storlekar; `namn` = vad varje yta är
    ("blå", "gul", "mitten" …) – det är namnen som rättas. `visa_omraden` ritar ytorna (verktyg)."""
    from .kort import DATA
    p = DATA / "bilder" / bild
    bb, hh = bildstorlek(p)
    hojd = int(bredd * hh / bb)

    def rita(duk, x, y):
        duk.bild(x, y, bredd, hojd, p)
        if visa_omraden:
            for i, (ox, oy, ob, oh) in enumerate(omraden or []):
                duk.ring(x + ox * bredd, y + oy * hojd, ob * bredd, oh * hojd, tema.ROD, 2)
                etikett = (namn or [])[i] if i < len(namn or []) else str(i + 1)
                duk.text(x + (ox + ob / 2) * bredd, y + oy * hojd - 3, etikett, storlek=12, fet=True,
                         farg=tema.ROD, ankare="mitt")
    return Figur(bredd, hojd, rita)


def ikonval(ikoner: list | None = None, d: int = 76, fore: list | None = None, namn: list | None = None,
            fore_d: int = 56, fore_namn: str = "", fore_antal: int = 0) -> Figur:
    """Lerikoner att välja bland (förskola: "Klicka på triangeln", "Klicka på fyran").
    `fore` = en rad ikoner ovanför som inte går att klicka på – t.ex. ett mönster att
    fortsätta, eller äpplen att räkna innan man väljer siffran."""
    from .ikoner import sokvag
    val = list(ikoner or [])
    fore = list(fore or []) + [fore_namn] * int(fore_antal or 0)   # "räkna jordgubbarna": fore_namn × fore_antal
    fragetecken = not fore_antal                                     # bara ett mönster slutar med ?
    topp = (fore_d + 26) if fore else 0
    bredd = max(len(val) * (d + 16), len(fore) * (fore_d + 8) + (60 if fragetecken else 0)) + 8

    def rita(duk, x, y):
        for i, n in enumerate(fore):
            duk.bild(x + 4 + i * (fore_d + 8), y + 4, fore_d, fore_d, sokvag(n))
        if fore and fragetecken:
            duk.bild(x + 4 + len(fore) * (fore_d + 8), y + 4, fore_d, fore_d, sokvag("fragetecken"))
            duk.linje(x, y + fore_d + 14, x + bredd, y + fore_d + 14, "#d8d8d8", 1)
        for i, n in enumerate(val):
            duk.rekt(x + 4 + i * (d + 16), y + topp + 4, d + 8, d + 8, fyll=tema.VIT, kant="#d8d8d8", tjock=1)
            duk.bild(x + 8 + i * (d + 16), y + topp + 8, d, d, sokvag(n))
    return Figur(bredd, topp + d + 20, rita)


def grupper(namn: str = "apple", antal: list | None = None, d: int = 40, per_rad: int = 3) -> Figur:
    """Två eller tre grupper av samma lerikon sida vid sida – "Var finns flest?"."""
    from .ikoner import sokvag
    antal = [int(a) for a in (antal or [3, 5])]
    p = sokvag(namn)
    gb = per_rad * (d + 4) + 16
    rader = max(-(-a // per_rad) for a in antal) if antal else 1
    gh = rader * (d + 4) + 16

    def rita(duk, x, y):
        for g, a in enumerate(antal):
            gx = x + 4 + g * (gb + 20)
            duk.rekt(gx, y + 4, gb, gh, fyll=tema.VIT, kant="#d8d8d8", tjock=1)
            for i in range(a):
                r, k = divmod(i, per_rad)
                duk.bild(gx + 8 + k * (d + 4), y + 12 + r * (d + 4), d, d, p)
    return Figur(len(antal) * (gb + 20) + 8, gh + 8, rita)


def tioram(fyllda: int = 0, farg: str = "#2f5fb3", farg2: str = "#f2c230", extra: int = 0, ruta: int = 52) -> Figur:
    """En tioram (2 × 5). `fyllda` brickor i `farg`; `extra` brickor i `farg2` efter dem."""
    f1, f2 = tema.farg(farg), tema.farg(farg2)
    n1, n2 = int(fyllda), int(extra)

    def rita(duk, x, y):
        duk.rekt(x + 2, y + 2, 5 * ruta + 8, 2 * ruta + 8, fyll="#e9c99a", kant="#9c7444", tjock=2)
        for i in range(10):
            r, k = divmod(i, 5)
            cx, cy = x + 6 + k * ruta, y + 6 + r * ruta
            duk.rekt(cx, cy, ruta, ruta, fyll="#f6e3c4", kant="#9c7444", tjock=1.5)
            if i < n1 + n2:
                duk.bricka(cx + ruta / 2, cy + ruta / 2, ruta * 0.38, f1 if i < n1 else f2)
    return Figur(5 * ruta + 12, 2 * ruta + 12, rita)


def malrutor(objekt=None, kolumner: int = 6, ruta: int = 64, farger=None, malade=None) -> Figur:
    """Rutor att måla: tal, ord, former ("form:triangel") eller ikoner ("ikon:apple").
    Ritas på riktigt i C++ (src/Rita.cpp) – här bara så att `matte check` kan prova figuren."""
    obj = objekt if isinstance(objekt, list) else str(objekt or "").split("|")
    k = max(1, int(kolumner)); rader = -(-len(obj) // k) or 1

    def rita(duk, x, y):
        for i, o in enumerate(obj):
            r, c = divmod(i, k)
            duk.rekt(x + 4 + c * ruta, y + 4 + r * ruta, ruta - 6, ruta - 6, fyll=tema.VIT, kant=tema.LINJE, tjock=1.5)
            duk.text(x + 4 + c * ruta + (ruta - 6) / 2, y + 4 + r * ruta + ruta / 2 + 4, str(o).split(":")[-1],
                     storlek=16, ankare="mitt")
    return Figur(min(len(obj), k) * ruta + 8, rader * ruta + 8, rita)


def rutpapper(bredd: int = 16, hojd: int = 10, ruta: int = 28) -> Figur:
    """Rutat papper – bakgrund i en rityta."""
    def rita(duk, x, y):
        for i in range(int(bredd) + 1):
            duk.linje(x + i * ruta, y, x + i * ruta, y + int(hojd) * ruta, "#d6dde8", 1)
        for j in range(int(hojd) + 1):
            duk.linje(x, y + j * ruta, x + int(bredd) * ruta, y + j * ruta, "#d6dde8", 1)
    return Figur(int(bredd) * ruta + 1, int(hojd) * ruta + 1, rita)


FIGURER = {
    "stapel": stapel, "hundraruta": hundraruta, "tiostaplar": tiostaplar,
    "staende": staende, "positionstabell": positionstabell, "tallinje": tallinje, "brak": brak,
    "rektangel": rektangel, "triangel": triangel, "stapeldiagram": stapeldiagram,
    "koordinatsystem": koordinatsystem, "tiobas": tiobas, "prickar": prickar,
    "talkamrater": talkamrater, "klocka": klocka, "tabell": tabell, "ikoner": ikoner,
    "scen": scen, "ikonval": ikonval, "grupper": grupper, "tioram": tioram,
    "malrutor": malrutor, "rutpapper": rutpapper,
}


def _utan_pennor(beskrivning: dict) -> dict:
    """`farger` (pennorna i en måla-uppgift) hör till svarsytan, inte till figuren."""
    b = dict(beskrivning)
    if b.get("typ") != "malrutor":
        b.pop("farger", None); b.pop("malade", None)
    return b


def celler(beskrivning: dict) -> list[tuple[float, float, float, float]]:
    """Klickbara rutor (x, y, b, h) för figurer som går att klicka i: stapel, staende,
    tiostaplar, hundraruta, prickar, ikoner. Tom lista för andra figurer."""
    b = _utan_pennor(beskrivning); typ = b.pop("typ")
    f = FIGURER[typ](**b)
    ut = []
    if typ in ("stapel", "staende", "tiostaplar"):
        delar = int(b.get("delar", 10 if typ == "tiostaplar" else 1))
        db = f.bredd / delar
        ut = [(i * db, 0, db, f.hojd) for i in range(delar)]
    elif typ == "hundraruta":
        r = int(b.get("ruta", 22))
        ut = [((i % 10) * r, (i // 10) * r, r, r) for i in range(100)]
    elif typ == "prickar":
        n, pr, d = int(b.get("antal", 7)), int(b.get("per_rad", 5)), int(b.get("d", 26))
        ut = [(4 + (i % pr) * (d + 6), 4 + (i // pr) * (d + 6), d, d) for i in range(n)]
    elif typ == "ikoner":
        n, pr, d = int(b.get("antal", 5)), int(b.get("per_rad", 5)), int(b.get("d", 48))
        ut = [(4 + (i % pr) * (d + 6), 4 + (i // pr) * (d + 6), d, d) for i in range(n)]
    elif typ == "scen":
        ut = [(ox * f.bredd, oy * f.hojd, ob * f.bredd, oh * f.hojd) for ox, oy, ob, oh in b.get("omraden", [])]
    elif typ == "ikonval":
        d = int(b.get("d", 76)); fd = int(b.get("fore_d", 56))
        topp = (fd + 26) if (b.get("fore") or b.get("fore_antal")) else 0
        ut = [(4 + i * (d + 16), topp + 4, d + 8, d + 8) for i in range(len(b.get("ikoner") or []))]
    elif typ == "grupper":
        d = int(b.get("d", 40)); pr = int(b.get("per_rad", 3)); gb = pr * (d + 4) + 16
        ut = [(4 + g * (gb + 20), 4, gb, f.hojd - 8) for g in range(len(b.get("antal") or [3, 5]))]
    elif typ == "malrutor":
        obj = b.get("objekt") if isinstance(b.get("objekt"), list) else str(b.get("objekt") or "").split("|")
        k, r = max(1, int(b.get("kolumner", 6))), int(b.get("ruta", 64))
        ut = [(4 + (i % k) * r, 4 + (i // k) * r, r - 6, r - 6) for i in range(len(obj))]
    elif typ == "tioram":
        r = int(b.get("ruta", 52))
        ut = [(6 + (i % 5) * r, 6 + (i // 5) * r, r, r) for i in range(10)]
    return ut


def cellnamn(beskrivning: dict) -> list[str]:
    """Namnen på klickytorna i en scen (för valj/markera). Saknas namn: 1, 2, 3 …"""
    n = len(celler(beskrivning))
    typ = beskrivning.get("typ")
    if typ == "grupper":                       # där är `namn` ikonen – ytorna heter första, andra …
        namn = ["första", "andra", "tredje", "fjärde"]
    else:
        namn = list(beskrivning.get("namn") or [])
        if not namn and typ == "ikonval":
            namn = list(beskrivning.get("ikoner") or [])
    return [namn[i] if i < len(namn) else str(i + 1) for i in range(n)]


class Rutnat:
    """Snäppning mellan pixlar och värden för figurer man kan pricka i:
    tallinje (ett tal) och koordinatsystem (x, y). Används av Figurvy vid klick."""

    def __init__(self, beskrivning: dict):
        from .uttryck import tolka
        b = dict(beskrivning); self.typ = b.pop("typ")
        self.fig = FIGURER[self.typ](**b)
        if self.typ == "tallinje":
            self.fran, self.till = tolka(str(b.get("fran", 0))), tolka(str(b.get("till", 10)))
            self.steg = tolka(str(b.get("steg", 1)))
            self.bredd = int(b.get("bredd", 420)); self.y0 = 24
        elif self.typ == "koordinatsystem":
            self.xmin, self.xmax = int(b.get("xmin", 0)), int(b.get("xmax", 6))
            self.ymin, self.ymax = int(b.get("ymin", 0)), int(b.get("ymax", 6))
            self.ruta = int(b.get("ruta", 32)); self.marg = 24
        else:
            raise ValueError(f"Går inte att pricka i figuren {self.typ}")

    def snapp(self, x, y):
        """Pixel → värde: Fraction (tallinje) eller (int, int) (koordinatsystem)."""
        if self.typ == "tallinje":
            b = self.bredd - 36; n = int((self.till - self.fran) / self.steg)
            i = max(0, min(n, round((x - 18) / b * n)))
            return self.fran + i * self.steg
        vx = round((x - self.marg) / self.ruta) + self.xmin
        vy = self.ymax - round((y - 6) / self.ruta)
        return (max(self.xmin, min(self.xmax, vx)), max(self.ymin, min(self.ymax, vy)))

    def pixel(self, v):
        """Värde → pixel (x, y)."""
        if self.typ == "tallinje":
            b = self.bredd - 36
            return 18 + float((v - self.fran) / (self.till - self.fran)) * b, self.y0
        vx, vy = v
        return self.marg + (vx - self.xmin) * self.ruta, 6 + (self.ymax - vy) * self.ruta


def figur(beskrivning: dict) -> Figur:
    """Gör en figur av en ordbok som {typ = "stapel", delar = 4, fyllda = 1}."""
    b = _utan_pennor(beskrivning)
    typ = b.pop("typ")
    if typ not in FIGURER:
        raise ValueError(f"Okänd figur: {typ}")
    return FIGURER[typ](**b)
