"""Fönstret – ett WenuyOS-program på libwenuy.

Fönstergrunden (titelrader, sidopanel, statusrad, toast, ⋯-knappen) kommer från
libwenuy via skal.py – samma som Filer och Inställningar. Här ligger bara det som
är Mattes eget: sidopanelen (stadium → område → kort med status ○ ◐ ●) och fyra
vyer: *Hem*, *Kortet*, *Öva* och *Placering*. Alla texter finns på engelska och
svenska (tr), kortens innehåll är svenskt eftersom det följer den svenska läroplanen.
"""
from __future__ import annotations

import os

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
from gi.repository import Adw, Gdk, GLib, Gtk  # noqa: E402

from . import ikoner, tal, tema  # noqa: E402
from .framsteg import BEHARSKAR, LAR, OVAR, Elev, elever, filnamn, satt_senaste_elev, senaste_elev  # noqa: E402
from .kort import DATA, Kort, OMRADEN, STADIER, alla_kort, las_laroplan  # noqa: E402
from .placering import Placering, placera_alla  # noqa: E402
from .regelbok import bok_html  # noqa: E402
from .rita import CairoDuk, Rutnat, celler, cellnamn, figur  # noqa: E402
from .skal import APP_ID, Fonstergrund, accentfarg, animationer_pa, i18n_init, mat, tr  # noqa: E402
from .uttryck import formatera  # noqa: E402
from .uppgifter import Bedomning, Uppgift, bedom, feltanke_id, gor_uppgift  # noqa: E402
from . import adaptiv  # noqa: E402

# Bara Mattes egna klasser – fönster, knappar, fält och statusrad stylas av WenuyOS-temat.
CSS = f"""
.stadium {{ font-weight: 600; font-size: 13px; padding: 12px 10px 2px; }}
.omrade {{ opacity: 0.7; font-size: 12px; padding: 8px 10px 2px; }}
.kort {{ color: {tema.TEXT}; background: {tema.KORT}; border: 1px solid {tema.LINJE}; margin: 20px; padding: 0 0 16px; }}
.korttitel {{ font-weight: 600; font-size: 18px; padding: 10px 16px; border-bottom: 1px solid {tema.LINJE}; }}
.sager {{ padding: 12px 16px 4px; font-size: 15px; }}
.rubrik {{ font-weight: 600; }}
.skriver {{ font-size: 16px; font-variant-numeric: tabular-nums; }}
.galler {{ color: {tema.TEXT}; margin: 12px 16px 0; padding: 8px 12px; background: {tema.ACCENT_SVAG}; border-left: 4px solid {accentfarg()}; }}
.galler-inte {{ color: {tema.TEXT}; margin: 8px 16px 0; padding: 8px 12px; background: #fbe9e8; border-left: 4px solid {tema.ROD}; }}
.minns {{ color: {tema.TEXT}; margin: 8px 16px 0; padding: 8px 12px; background: #eef3fb; border-left: 4px solid {tema.FARGER['himmel']}; }}
.svag {{ opacity: 0.75; font-size: 12px; }}
.fraga {{ font-size: 18px; padding: 8px 0; }}
.steg {{ color: {tema.TEXT}; margin: 0 16px; padding: 8px 12px; background: {tema.KORT}; border: 1px dashed {tema.LINJE}; font-size: 17px; font-variant-numeric: tabular-nums; }}
.losning {{ color: {tema.TEXT}; margin: 4px 16px 0; padding: 8px 12px; background: #eef3fb; border-left: 4px solid {tema.FARGER['himmel']}; }}
.ratt {{ color: {tema.GRON}; font-weight: 600; }}
.fel {{ color: {tema.ROD}; }}
.stor {{ font-size: 22px; font-weight: 600; }}
button.val {{ font-size: 18px; padding: 10px 22px; min-width: 90px; }}
button.val.ratt {{ background: #e6f4e8; }}
button.val.fel {{ background: #fbe9e8; }}
entry.fel {{ border-color: {tema.ROD}; }}
.svarsfalt {{ font-size: 18px; }}
/* Firandet när ett kort blir Behärskat: kort, ingen blur, inga lager över innehållet */
@keyframes matte-fira {{ from {{ transform: scale(0.85); opacity: 0.4; }} to {{ transform: scale(1); opacity: 1; }} }}
.fira {{ color: {tema.TEXT}; background: {tema.ACCENT_SVAG}; border: 1px solid {accentfarg()}; padding: 10px 16px; margin: 0 16px; }}
.fira.anim image {{ animation: matte-fira 180ms ease-out; }}
.fira-rubrik {{ font-weight: 700; font-size: 18px; }}
button.ny {{ border: 2px solid {accentfarg()}; }}
/* Lekläget: pekskärm och små fingrar */
.lek .fraga {{ font-size: 30px; font-weight: 600; }}
.lek button {{ font-size: 20px; min-height: 56px; padding: 8px 24px; }}
.lek .svarsfalt {{ font-size: 30px; min-height: 60px; }}
.lek .sager {{ font-size: 20px; }}
.lek .korttitel {{ font-size: 26px; }}
.lek .svag {{ font-size: 15px; }}
.lek .steg {{ font-size: 26px; }}
"""

from .fonster_konstanter import INTERAKTIVA, LEKLAGE_STADIER  # noqa: E402

STATUS_TECKEN = {LAR: "○", OVAR: "◐", BEHARSKAR: "●"}


def ikon(namn: str, px: int = 24) -> Gtk.Image:
    bild = Gtk.Image.new_from_file(str(ikoner.sokvag(namn)))
    bild.set_pixel_size(px)
    return bild


class Figurvy(Gtk.DrawingArea):
    """Ritar en figur-beskrivning med CairoDuk. Med `klickbar` kan eleven markera
    rutor (staplar, prickar, äpplen) – antalet markerade är hennes svar."""

    # Lekläget gör alla figurer större (pekskärm, små fingrar). Sätts av fönstret.
    skala_standard = 1.0

    def __init__(self, beskrivning: dict | None, klickbar: bool = False, vid_klick=None,
                 lage: str = "", skala: float | None = None):
        """`lage`: "" (bara visa), "klick" (markera rutor), "punkt" (pricka in ett värde),
        "linje" (två punkter blir en linje)."""
        super().__init__()
        self.besk = beskrivning
        self.lage = lage or ("klick" if klickbar else "")
        self.fig = figur(beskrivning) if beskrivning else None
        self.celler = celler(beskrivning) if (beskrivning and self.lage in ("klick", "valj", "markera")) else []
        self.namn = cellnamn(beskrivning) if (beskrivning and self.lage in ("valj", "markera")) else []
        self.rutnat = Rutnat(beskrivning) if (beskrivning and self.lage in ("punkt", "linje")) else None
        self.markerade: set[int] = set()
        self.punkter: list = []            # valda värden (tal, eller (x, y))
        self.vid_klick = vid_klick
        self.skala = skala or Figurvy.skala_standard
        if self.fig and self.besk.get("typ") == "scen":
            self.skala = min(self.skala, 1.25)       # målningarna är redan stora
        if self.fig:
            self.set_content_width(int(self.fig.bredd * self.skala))
            self.set_content_height(int(self.fig.hojd * self.skala))
        self.set_draw_func(self._rita)
        if self.celler or self.rutnat:
            g = Gtk.GestureClick()
            g.connect("pressed", self._klick)
            self.add_controller(g)

    def _rita(self, _omr, cr, _b, _h):
        if not self.fig:
            return
        cr.scale(self.skala, self.skala)
        duk = CairoDuk(cr)
        self.fig.rita(duk, 0, 0)
        for i in self.markerade:
            x, y, b, h = self.celler[i]
            if self.besk.get("typ") == "scen":
                duk.ring(x, y, b, h, accentfarg(), 4)        # ring runt det man klickat på i målningen
                continue
            if self.besk.get("typ") in ("ikonval", "grupper"):
                duk.rekt(x, y, b, h, kant=accentfarg(), tjock=4)
                continue
            fyll = tema.farg(self.besk.get("farg", "gul")) if self.besk.get("typ") in ("stapel", "staende", "tiostaplar", "hundraruta") else None
            duk.rekt(x + 1.5, y + 1.5, b - 3, h - 3, fyll=fyll, kant=tema.ACCENT, tjock=3)
        if self.rutnat:
            pix = [self.rutnat.pixel(v) for v in self.punkter]
            if self.lage == "linje" and len(pix) == 2:
                (x1, y1), (x2, y2) = pix
                # förläng linjen över hela rutan
                if x1 != x2:
                    k = (y2 - y1) / (x2 - x1)
                    xa, xb = self.rutnat.marg, self.rutnat.marg + (self.rutnat.xmax - self.rutnat.xmin) * self.rutnat.ruta
                    duk.linje(xa, y1 + k * (xa - x1), xb, y1 + k * (xb - x1), tema.ACCENT, 2.5)
                else:
                    duk.linje(x1, 6, x1, 6 + (self.rutnat.ymax - self.rutnat.ymin) * self.rutnat.ruta, tema.ACCENT, 2.5)
            for x, y in pix:
                duk.rekt(x - 6, y - 6, 12, 12, fyll=tema.ACCENT, kant=tema.VIT, tjock=1.5)

    def _klick(self, _g, _n, x, y):
        x, y = x / self.skala, y / self.skala
        if self.rutnat:
            v = self.rutnat.snapp(x, y)
            if self.lage == "punkt":
                self.punkter = [v]
            else:                                   # linje: två punkter, tredje klicket börjar om
                self.punkter = self.punkter + [v] if len(self.punkter) < 2 else [v]
                if len(self.punkter) == 2 and self.punkter[0] == self.punkter[1]:
                    self.punkter = [v]
            self.queue_draw()
            if self.vid_klick:
                self.vid_klick(self.punkter)
            return
        for i, (cx, cy, b, h) in enumerate(self.celler):
            if cx <= x <= cx + b and cy <= y <= cy + h:
                if self.lage == "valj":
                    self.markerade = {i}                 # bara en sak åt gången
                else:
                    self.markerade ^= {i}
                self.queue_draw()
                if self.vid_klick:
                    self.vid_klick(len(self.markerade))
                return

    def svar(self) -> list[str] | str | None:
        """Elevens svar i den form bedom() vill ha."""
        if self.lage == "klick":
            return str(len(self.markerade))
        if self.lage == "valj":
            return self.namn[next(iter(self.markerade))] if self.markerade else ""
        if self.lage == "markera":
            return ",".join(self.namn[i] for i in sorted(self.markerade))
        if self.lage == "punkt":
            if not self.punkter:
                return None
            v = self.punkter[0]
            return [str(v[0]), str(v[1])] if isinstance(v, tuple) else formatera(v, "auto")
        if self.lage == "linje":
            if len(self.punkter) < 2:
                return None
            (x1, y1), (x2, y2) = self.punkter
            if x1 == x2:
                return ["lodrät", "lodrät"]
            from fractions import Fraction
            k = Fraction(y2 - y1, x2 - x1); m = y1 - k * x1
            return [formatera(k, "auto"), formatera(m, "auto")]
        return None


class Dragyta(Gtk.DrawingArea):
    """En tioram och en hög brickor under den. Eleven drar brickor in i ramen (eller
    knackar på dem – det fungerar också med finger), och drar ut dem igen.
    Svaret är hur många brickor eleven har lagt dit själv."""

    def __init__(self, beskrivning: dict, skala: float = 1.0):
        super().__init__()
        self.besk = dict(beskrivning)
        self.fyllda = int(self.besk.get("fyllda", 0))
        self.ruta = int(self.besk.get("ruta", 52))
        self.lagda = 0                                  # brickor eleven har lagt i ramen
        self.hog = 10 - self.fyllda                     # så många ligger i högen
        self.skala = skala
        self.drar: tuple[float, float] | None = None     # brickan som dras just nu (x, y)
        self._fran_ram = False
        self.bredd = 5 * self.ruta + 12
        self.hojd = 2 * self.ruta + 12 + 30 + self.ruta
        self.set_content_width(int(self.bredd * skala)); self.set_content_height(int(self.hojd * skala))
        self.set_draw_func(self._rita)
        d = Gtk.GestureDrag()
        d.connect("drag-begin", self._borja); d.connect("drag-update", self._dra); d.connect("drag-end", self._slut)
        self.add_controller(d)

    def _ram(self):
        return figur({**self.besk, "typ": "tioram", "fyllda": self.fyllda, "extra": self.lagda})

    def _hog_y(self):
        return 2 * self.ruta + 12 + 30

    def _rita(self, _o, cr, _b, _h):
        cr.scale(self.skala, self.skala)
        duk = CairoDuk(cr)
        self._ram().rita(duk, 0, 0)
        r = self.ruta * 0.38
        for i in range(self.hog - self.lagda):            # högen: brickor i en rad under ramen
            duk.bricka(self.ruta / 2 + 6 + (i % 10) * (r * 1.3), self._hog_y() + self.ruta / 2, r, tema.farg(self.besk.get("farg2", "#f2c230")))
        if self.drar:
            duk.bricka(self.drar[0], self.drar[1], r, tema.farg(self.besk.get("farg2", "#f2c230")))

    def _i_ram(self, x, y):
        return 0 <= x <= self.bredd and 0 <= y <= 2 * self.ruta + 12

    def _borja(self, _g, x, y):
        x, y = x / self.skala, y / self.skala
        self._start = (x, y)
        if y >= self._hog_y() - 10 and self.hog - self.lagda > 0:
            self.drar = (x, y); self._fran_ram = False
        elif self._i_ram(x, y) and self.lagda > 0:
            self.drar = (x, y); self._fran_ram = True; self.lagda -= 1
        self.queue_draw()

    def _dra(self, _g, dx, dy):
        if self.drar:
            self.drar = (self._start[0] + dx / self.skala, self._start[1] + dy / self.skala); self.queue_draw()

    def _slut(self, _g, dx, dy):
        if not self.drar:
            return
        x, y = self.drar
        kort_knack = abs(dx) < 6 and abs(dy) < 6
        if self._fran_ram:
            if self._i_ram(x, y) and not kort_knack:
                self.lagda += 1                            # släpptes i ramen igen – ångra
        elif (self._i_ram(x, y) or kort_knack) and self.fyllda + self.lagda < 10:
            self.lagda += 1                                # dragen dit, eller en knackning på högen
        self.drar = None
        self.queue_draw()

    def svar(self) -> str:
        return str(self.lagda)


class Skissruta(Gtk.DrawingArea):
    """En ritruta att kladda i med mus eller finger. Rättas inte – det är elevens
    eget papper under problemlösningen. Streck sparas i minnet tills nästa uppgift."""

    def __init__(self, bredd: int = 420, hojd: int = 220):
        super().__init__()
        self.set_content_width(bredd); self.set_content_height(hojd)
        self.streck: list[list[tuple[float, float]]] = []
        self.set_draw_func(self._rita)
        d = Gtk.GestureDrag()
        d.connect("drag-begin", self._borja); d.connect("drag-update", self._dra)
        self.add_controller(d)

    def _rita(self, _o, cr, b, h):
        duk = CairoDuk(cr)
        duk.rekt(0, 0, b, h, fyll=tema.VIT, kant=tema.LINJE)
        for yy in range(20, h, 20):
            duk.linje(0, yy, b, yy, "#e4e4e4", 1)
        for st in self.streck:
            for (x1, y1), (x2, y2) in zip(st, st[1:]):
                duk.linje(x1, y1, x2, y2, tema.TEXT, 2.5)

    def _borja(self, _g, x, y):
        self._start = (x, y); self.streck.append([(x, y)])

    def _dra(self, _g, dx, dy):
        self.streck[-1].append((self._start[0] + dx, self._start[1] + dy)); self.queue_draw()

    def rensa(self):
        self.streck = []; self.queue_draw()


def tom(box: Gtk.Box):
    while (c := box.get_first_child()) is not None:
        box.remove(c)


class MatteFonster:
    def __init__(self, app, elev: Elev):
        self.grund = Fonstergrund(app, "Matte")
        self.win = self.grund.win
        self.elev = elev
        self.korten = alla_kort()
        self.laroplan = las_laroplan()
        self.ordning: list[str] = []          # alla kort i läroplanens ordning
        self.aktuellt: Kort | None = None
        self.uppgift: Uppgift | None = None
        self.niva = 1
        self.placeringar: list[Placering] = []
        self.nya_beharskade: set[str] = set()          # kort som blev Behärskade den här gången
        self.plac_i = 0

        # ---- sidopanel (libwenuy ritar titelraden och ramen)
        self.lista = Gtk.ListBox(css_classes=["navigation-sidebar"])
        self.lista.set_selection_mode(Gtk.SelectionMode.SINGLE)
        self.lista.connect("row-selected", self._valde_rad)
        self.grund.satt_sidopanel(Gtk.ScrolledWindow(child=self.lista, hscrollbar_policy=Gtk.PolicyType.NEVER))

        # ---- innehåll
        self.stack = Gtk.Stack(transition_type=Gtk.StackTransitionType.CROSSFADE, transition_duration=150)
        self.grund.satt_innehall(self.stack)
        self.kortrull = Gtk.ScrolledWindow(hscrollbar_policy=Gtk.PolicyType.NEVER)
        self.stack.add_named(self.kortrull, "kort")
        self.stack.add_named(self._bygg_ovavy(), "ova")
        self.stack.add_named(self._bygg_placeringsvy(), "placering")
        self.hemrull = Gtk.ScrolledWindow(hscrollbar_policy=Gtk.PolicyType.NEVER)
        self.stack.add_named(self.hemrull, "hem")
        self.oversiktrull = Gtk.ScrolledWindow(hscrollbar_policy=Gtk.PolicyType.NEVER)
        self.stack.add_named(self.oversiktrull, "oversikt")

        self._tillampa_lekläge()
        self._fyll_lista()
        self._uppdatera_status()
        self.visa_hem()

        tang = Gtk.EventControllerKey()
        tang.connect("key-pressed", self._tangent)
        self.win.add_controller(tang)

    def present(self):
        self.win.present()

    # ------------------------------------------------------------ lekläge
    def _tillampa_lekläge(self):
        lek = self.elev.lekläge
        Figurvy.skala_standard = 1.5 if lek else 1.0
        (self.win.add_css_class if lek else self.win.remove_css_class)("lek")

    def satt_lekläge(self, pa: bool):
        self.elev.lekläge = pa; self.elev.spara()
        self._tillampa_lekläge(); self._fyll_lista(); self.visa_hem()
        self.grund.toast(tr("Play mode on – big buttons and read aloud", "Lekläge på – stora knappar och uppläsning") if pa
                         else tr("Play mode off", "Lekläge av"))

    def las_upp(self, text: str):
        if not tal.las(text):
            self.grund.toast(tr("No voice installed: sudo apt install espeak-ng", "Ingen röst installerad: sudo apt install espeak-ng"))

    # ------------------------------------------------------------ sidopanel
    def _fyll_lista(self):
        while (rad := self.lista.get_row_at_index(0)) is not None:
            self.lista.remove(rad)
        self.ordning = []
        hem = Gtk.ListBoxRow(); hem.vy = "hem"
        rad_box = Gtk.Box(spacing=8); rad_box.append(ikon(ikoner.PROGRAM, 22)); rad_box.append(Gtk.Label(label=tr("Home", "Hem"), xalign=0))
        hem.set_child(rad_box); self.lista.append(hem)
        for st_id, st_namn in STADIER:
            if self.elev.lekläge and st_id not in LEKLAGE_STADIER:
                continue
            mal = [m for m in self.laroplan if m.stadium == st_id]
            if not any(m.kort for m in mal):
                continue
            rubrik = Gtk.ListBoxRow(selectable=False, activatable=False)
            rb = Gtk.Box(spacing=8, css_classes=["stadium"]); rb.append(ikon(ikoner.STADIUM_IKON.get(st_id, "bok"), 20)); rb.append(Gtk.Label(label=st_namn, xalign=0))
            rubrik.set_child(rb); self.lista.append(rubrik)
            for omr, namn in OMRADEN.items():
                ids = [k for m in mal if m.omrade == omr for k in m.kort if k in self.korten]
                ids = list(dict.fromkeys(ids))
                if not ids:
                    continue
                orad = Gtk.ListBoxRow(selectable=False, activatable=False)
                ob = Gtk.Box(spacing=6, css_classes=["omrade"]); ob.append(ikon(ikoner.OMRADE_IKON.get(omr, "fragetecken"), 16)); ob.append(Gtk.Label(label=namn, xalign=0, ellipsize=3))
                orad.set_child(ob); self.lista.append(orad)
                for kid in ids:
                    if kid not in self.ordning:
                        self.ordning.append(kid)
                    rad = Gtk.ListBoxRow(); rad.kort_id = kid
                    s = self.elev.status(kid)
                    kb = Gtk.Box(spacing=8, margin_start=18)
                    kb.append(Gtk.Label(label=STATUS_TECKEN[s.lage], css_classes=["svag"]))
                    if self.korten[kid].ikon:
                        kb.append(ikon(self.korten[kid].ikon, 18))
                    kb.append(Gtk.Label(label=self.korten[kid].titel, xalign=0, ellipsize=3))
                    rad.set_child(kb); self.lista.append(rad)

    def _valde_rad(self, _lista, rad):
        if rad is None:
            return
        if getattr(rad, "vy", None) == "hem":
            self.visa_hem()
        elif hasattr(rad, "kort_id"):
            self.visa_kort(self.korten[rad.kort_id])

    # ------------------------------------------------------------ hem
    def visa_hem(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12, css_classes=["kort"])
        topp = Gtk.Box(spacing=12, margin_start=16, margin_top=14)
        topp.append(ikon(ikoner.PROGRAM, 48))
        topp.append(Gtk.Label(label="Matte", css_classes=["stor"], xalign=0, hexpand=True))
        # Elev: välj bland eleverna på datorn, eller lägg till en ny
        namn = elever() or [self.elev.namn]
        if self.elev.namn not in namn:
            namn.append(self.elev.namn)
        visning = [Elev.ladda(n).visningsnamn or n for n in namn]
        val = Gtk.DropDown.new_from_strings(visning)
        val.set_selected(namn.index(self.elev.namn))
        val.connect("notify::selected", lambda dd, _p: namn[dd.get_selected()] != self.elev.namn
                    and self.byt_elev(namn[dd.get_selected()]))
        topp.append(Gtk.Label(label=tr("Student", "Elev"), css_classes=["svag"]))
        topp.append(val)
        ny = Gtk.MenuButton(label=tr("New", "Ny"))
        pop = Gtk.Popover(); pb = Gtk.Box(spacing=6, margin_start=8, margin_end=8, margin_top=8, margin_bottom=8)
        falt = Gtk.Entry(placeholder_text=tr("Name", "Namn")); ok = Gtk.Button(label=tr("Add", "Lägg till"), css_classes=["suggested-action"])
        def lagg_till(*_):
            t = falt.get_text().strip()
            if t:
                e = Elev(filnamn(t)); e.visningsnamn = t; e.spara(); pop.popdown(); self.byt_elev(e.namn)
        ok.connect("clicked", lagg_till); falt.connect("activate", lagg_till)
        pb.append(falt); pb.append(ok); pop.set_child(pb); ny.set_popover(pop)
        topp.append(ny)
        box.append(topp)
        n = self.elev.sammanfattning(); rep = self.elev.att_repetera()
        box.append(Gtk.Label(label=tr(f"You have mastered {n['beharskar']} cards and are practising {n['ovar']}.", f"Du behärskar {n['beharskar']} kort och övar på {n['ovar']}."), xalign=0, margin_start=16))
        knappar = Gtk.Box(spacing=8, margin_start=16)
        if not self.elev.placerad:
            b = Gtk.Button(label=tr("How much do I already know?", "Hur mycket kan jag redan?"), css_classes=["suggested-action"])
            b.connect("clicked", lambda *_: self.borja_placering()); knappar.append(b)
            box.append(Gtk.Label(label=tr("A few questions per area find where you should start. You can skip it and start from the beginning.", "Några frågor per område hittar var du ska börja. Du kan hoppa över det och börja från början."),
                                 css_classes=["svag"], xalign=0, margin_start=16, wrap=True))
        if rep:
            b = Gtk.Button(label=tr(f"Review ({len(rep)})", f"Repetera ({len(rep)})"), css_classes=["suggested-action"] if self.elev.placerad else [])
            b.connect("clicked", lambda *_: (self.visa_kort(self.korten[rep[0]]), self.borja_ova())); knappar.append(b)
        nasta = self.elev.nasta_kort(self.ordning)
        if nasta:
            b = Gtk.Button(label=tr("Continue: ", "Fortsätt: ") + self.korten[nasta].titel)
            b.connect("clicked", lambda *_: self.visa_kort(self.korten[nasta])); knappar.append(b)
        b = Gtk.Button(label=tr("Print my rule book", "Skriv ut min regelbok"))
        b.connect("clicked", lambda *_: self.spara_regelbok()); knappar.append(b)
        b = Gtk.Button(label=tr("Overview (teacher/parent)", "Översikt (lärare/förälder)"))
        b.connect("clicked", lambda *_: self.visa_oversikt()); knappar.append(b)
        box.append(knappar)
        lekrad = Gtk.Box(spacing=10, margin_start=16, margin_top=6)
        brytare = Gtk.Switch(active=self.elev.lekläge, valign=Gtk.Align.CENTER)
        brytare.connect("notify::active", lambda sw, _p: sw.get_active() != self.elev.lekläge and self.satt_lekläge(sw.get_active()))
        lekrad.append(brytare)
        lekrad.append(Gtk.Label(label=tr("Play mode – for preschool and years 1–3: big buttons, pictures, questions read aloud",
                                         "Lekläge – för förskola och åk 1–3: stora knappar, bilder, frågorna läses upp"),
                                xalign=0, wrap=True, hexpand=True))
        box.append(lekrad)
        self.hem_medd = Gtk.Label(xalign=0, margin_start=16, css_classes=["svag"], wrap=True)
        box.append(self.hem_medd)
        if self.elev.regelbok:
            box.append(Gtk.Label(label=tr("Your rule book", "Din regelbok"), css_classes=["rubrik"], xalign=0, margin_start=16, margin_top=10))
            flode = Gtk.FlowBox(margin_start=16, margin_end=16, selection_mode=Gtk.SelectionMode.NONE, max_children_per_line=4)
            for kid in self.elev.regelbok:
                if kid not in self.korten:
                    continue
                k = self.korten[kid]
                b = Gtk.Button()
                bb = Gtk.Box(spacing=8); bb.append(ikon(k.ikon or ikoner.OMRADE_IKON.get(k.omrade, "bok"), 22))
                bb.append(Gtk.Label(label=f"{STATUS_TECKEN[self.elev.status(kid).lage]} {k.titel}", ellipsize=3))
                b.set_child(bb); b.connect("clicked", lambda _b, k=k: self.visa_kort(k))
                if kid in self.nya_beharskade:
                    b.add_css_class("ny"); b.set_tooltip_text(tr("New in your rule book", "Nytt i din regelbok"))
                flode.append(b)
            box.append(flode)
        self.hemrull.set_child(box)
        self.grund.satt_titel(tr("Home", "Hem"))
        self.stack.set_visible_child_name("hem")

    def byt_elev(self, namn: str):
        self.elev = Elev.ladda(namn)
        satt_senaste_elev(namn)
        self.aktuellt = None
        self._tillampa_lekläge(); self._fyll_lista(); self._uppdatera_status(); self.visa_hem()
        self.grund.toast(tr("Hello ", "Hej ") + (self.elev.visningsnamn or namn) + "!")

    def _dokumentmapp(self) -> str:
        return GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_DOCUMENTS) or os.path.expanduser("~")

    def visa_oversikt(self):
        from .oversikt import html_rapport, sammanstall
        rader, grupp = sammanstall()
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10, css_classes=["kort"])
        box.append(Gtk.Label(label=tr("Overview", "Översikt"), css_classes=["korttitel"]))
        rutnat = Gtk.Grid(column_spacing=18, row_spacing=6, margin_start=16, margin_end=16)
        for c, t in enumerate([tr("Student", "Elev"), "● " + tr("Mastered", "Behärskar"), "◐ " + tr("Practising", "Övar"),
                               tr("Review today", "Repetera i dag"), tr("Answers", "Svar"), tr("Correct", "Rätt")]):
            rutnat.attach(Gtk.Label(label=t, css_classes=["rubrik"], xalign=0), c, 0, 1, 1)
        for r, rad in enumerate(rader, 1):
            for c, t in enumerate([rad.namn, str(rad.beharskar), str(rad.ovar), str(rad.repetera), str(rad.svar),
                                   f"{rad.andel_ratt:.0%}"]):
                rutnat.attach(Gtk.Label(label=t, xalign=0), c, r, 1, 1)
        box.append(rutnat)
        if grupp:
            box.append(Gtk.Label(label=tr("Most common misconceptions in the group", "Vanligaste feltankarna i gruppen"),
                                 css_classes=["rubrik"], xalign=0, margin_start=16, margin_top=10))
            for f in grupp:
                box.append(Gtk.Label(label=f"{f.kort_titel} – {len(f.elever)} " + tr("student(s)", "elev(er)") + f", {f.antal}×\n{f.text}",
                                     xalign=0, wrap=True, margin_start=16, margin_end=16, css_classes=["galler"]))
        else:
            box.append(Gtk.Label(label=tr("No misconceptions recorded yet.", "Inga feltankar registrerade än."),
                                 css_classes=["svag"], xalign=0, margin_start=16))
        def spara(*_):
            p = os.path.join(self._dokumentmapp(), "Matte – översikt.html")
            with open(p, "w", encoding="utf-8") as fil:
                fil.write(html_rapport())
            self.grund.toast(tr("Saved: ", "Sparad: ") + p)
        knapp = Gtk.Button(label=tr("Save for printing", "Spara för utskrift"), margin_start=16, halign=Gtk.Align.START)
        knapp.connect("clicked", spara); box.append(knapp)
        box.append(Gtk.Label(label=tr("Everything comes from the files on this computer. Nothing is sent anywhere.",
                                      "Allt kommer från filerna på den här datorn. Inget skickas någonstans."),
                             css_classes=["svag"], xalign=0, margin_start=16))
        self.oversiktrull.set_child(box)
        self.grund.satt_titel(tr("Overview", "Översikt"))
        self.stack.set_visible_child_name("oversikt")

    def spara_arbetsblad(self, k: Kort):
        from .arbetsblad import arbetsblad
        p = os.path.join(self._dokumentmapp(), f"Arbetsblad – {k.titel}.html")
        with open(p, "w", encoding="utf-8") as fil:
            fil.write(arbetsblad([k], 8, titel=k.titel))
        self.grund.toast(tr("Worksheet saved: ", "Arbetsblad sparat: ") + p)

    def spara_regelbok(self):
        mapp = GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_DOCUMENTS) or os.path.expanduser("~")
        p = os.path.join(mapp, "Min regelbok.html")
        urval = [self.korten[k] for k in self.elev.regelbok if k in self.korten]
        with open(p, "w", encoding="utf-8") as f:
            f.write(bok_html(urval, "Min regelbok"))
        self.hem_medd.set_label(tr(f"Saved as {p} – open it in the browser and print.", f"Sparad som {p} – öppna i webbläsaren och skriv ut."))
        self.grund.toast(tr("Rule book saved", "Regelboken sparad"))

    # ------------------------------------------------------------ kortvyn
    def visa_kort(self, k: Kort):
        self.aktuellt = k
        self.elev.sett_kort(k.id); self.elev.spara()
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, css_classes=["kort"])
        titel = Gtk.Box(spacing=10, css_classes=["korttitel"], halign=Gtk.Align.CENTER)
        if k.ikon:
            titel.append(ikon(k.ikon, 28))
        titel.append(Gtk.Label(label=k.titel))
        box.append(titel)
        box.append(Gtk.Label(label=k.omrade_namn, css_classes=["svag"], xalign=0, margin_start=16, margin_top=4))
        if k.bild:
            bp = DATA / "bilder" / k.bild
            if bp.exists():
                bild = Gtk.Picture.new_for_filename(str(bp))
                bild.set_content_fit(Gtk.ContentFit.COVER); bild.set_size_request(-1, 260)
                bild.set_margin_top(8)
                box.append(bild)
                if k.bildtext:
                    box.append(Gtk.Label(label=k.bildtext, css_classes=["svag"], xalign=0, wrap=True, margin_start=16, margin_top=4))
        if k.sager:
            box.append(Gtk.Label(label=k.sager, css_classes=["sager"], xalign=0, wrap=True))

        # "Minns du?" – regler som slutade gälla
        for aid in k.ateruppta:
            a = self.korten.get(aid)
            if a and a.galler_inte and aid in self.elev.regelbok:
                box.append(Gtk.Label(label=tr(f"Remember {a.titel}? ", f"Minns du {a.titel}? ") + a.galler_inte, css_classes=["minns"], xalign=0, wrap=True))

        if k.rader:
            tre = all(len(r.skriver) == 3 for r in k.rader)
            rutnat = Gtk.Grid(row_spacing=8, column_spacing=18, margin_start=16, margin_end=16, margin_top=10)
            rubriker = ["", tr("Fraction", "Bråkform"), tr("Percent", "Procentform"), tr("Decimal", "Decimalform")] if tre else [tr("We say", "Man säger"), "", tr("We write", "Man skriver")]
            for i, t in enumerate(rubriker):
                rutnat.attach(Gtk.Label(label=t, css_classes=["rubrik"], xalign=0), i, 0, 1, 1)
            rad_i = 1
            for r in k.rader:
                if not tre:
                    rutnat.attach(Gtk.Label(label=r.sager, xalign=0, wrap=True, max_width_chars=22), 0, rad_i, 1, 1)
                fig = Figurvy(r.figur) if r.figur else Gtk.Label()
                rutnat.attach(fig, 0 if tre else 1, rad_i, 1, 1)
                if tre:
                    for c, s in enumerate(r.skriver, start=1):
                        rutnat.attach(Gtk.Label(label=s, css_classes=["skriver"], xalign=0.5), c, rad_i, 1, 1)
                else:
                    rutnat.attach(Gtk.Label(label="\n".join(r.skriver), css_classes=["skriver"], xalign=0,
                                           wrap=True, max_width_chars=30), 2, rad_i, 1, 1)
                rad_i += 1
                if r.kommentar:
                    rutnat.attach(Gtk.Label(label=r.kommentar, css_classes=["svag"], xalign=0, wrap=True), 0, rad_i, 4, 1)
                    rad_i += 1
            box.append(rutnat)

        for e in k.exempel:
            ebox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4, margin_start=16, margin_end=16, margin_top=14)
            if e.rubrik:
                ebox.append(Gtk.Label(label=e.rubrik, css_classes=["rubrik"], xalign=0))
            if e.text:
                ebox.append(Gtk.Label(label=e.text, xalign=0, wrap=True))
            if e.figur:
                ebox.append(Figurvy(e.figur))
            for i, s in enumerate(e.steg, start=1):
                ebox.append(Gtk.Label(label=f"{i}. {s}", xalign=0, wrap=True))
            box.append(ebox)

        if k.galler:
            box.append(Gtk.Label(label=tr("Applies: ", "Gäller: ") + k.galler, css_classes=["galler"], xalign=0, wrap=True))
        if k.galler_inte:
            box.append(Gtk.Label(label=tr("Stops applying: ", "Slutar gälla: ") + k.galler_inte, css_classes=["galler-inte"], xalign=0, wrap=True))

        knappar = Gtk.Box(spacing=8, margin_start=16, margin_top=16)
        ova = Gtk.Button(label=tr("Practise", "Öva"), css_classes=["suggested-action"]); ova.connect("clicked", lambda *_: self.borja_ova())
        knappar.append(ova)
        lyssna = Gtk.Button(label="🔊 " + tr("Read aloud", "Läs upp"))
        lyssna.connect("clicked", lambda *_: self.las_upp(". ".join(x for x in (k.titel, k.sager, k.bildtext) if x)))
        knappar.append(lyssna)
        blad = Gtk.Button(label=tr("Worksheet", "Arbetsblad"))
        blad.connect("clicked", lambda *_: self.spara_arbetsblad(k))
        knappar.append(blad)
        if k.kalla and not self.elev.lekläge:
            knappar.append(Gtk.Label(label=tr("Based on: ", "Bygger på: ") + k.kalla, css_classes=["svag"], xalign=0, wrap=True, hexpand=True))
        box.append(knappar)
        self.kortrull.set_child(box)
        self.grund.satt_titel(k.titel)
        self.stack.set_visible_child_name("kort")
        self.grund.visa_innehall()

    # ------------------------------------------------------------ svarswidgetar (delas av Öva och Placering)
    def _bygg_svar(self, plats: Gtk.Box, u: Uppgift, vid_svar):
        """Bygger rätt inmatning för uppgiftens svarstyp i `plats`. `vid_svar(svar)` anropas vid Enter/klick."""
        tom(plats)
        self.svarsfalt: list[Gtk.Entry] = []
        self.valknappar: list[Gtk.Button] = []
        self.klickfigur: Figurvy | None = None
        if u.svarstyp == "val":
            rad = Gtk.Box(spacing=8)
            for alt in u.alternativ:
                b = Gtk.Button(label=alt, css_classes=["val"])
                b.connect("clicked", lambda _b, alt=alt: vid_svar(alt))
                rad.append(b); self.valknappar.append(b)
            plats.append(rad)
        elif u.svarstyp == "tabell":
            rutnat = Gtk.Grid(column_spacing=12, row_spacing=4)
            for i, namn in enumerate(u.falt):
                rutnat.attach(Gtk.Label(label=namn, css_classes=["rubrik"], xalign=0), i, 0, 1, 1)
                e = Gtk.Entry(width_chars=10); e.connect("activate", lambda *_: vid_svar([f.get_text() for f in self.svarsfalt]))
                rutnat.attach(e, i, 1, 1, 1); self.svarsfalt.append(e)
            plats.append(rutnat)
            b = Gtk.Button(label=tr("Check", "Kolla"), css_classes=["suggested-action"]); b.connect("clicked", lambda *_: vid_svar([f.get_text() for f in self.svarsfalt]))
            plats.append(b)
        elif u.svarstyp == "dra" and u.figur:
            self.klickfigur = Dragyta(u.figur, Figurvy.skala_standard)
            plats.append(self.klickfigur)
            plats.append(Gtk.Label(label=tr("Drag the counters into the frame. Drag one out to undo.",
                                            "Dra brickorna till ramen. Dra ut en för att ångra."), css_classes=["svag"], xalign=0))
            b = Gtk.Button(label=tr("Done", "Klar"), css_classes=["suggested-action"])
            b.connect("clicked", lambda *_: vid_svar(self.klickfigur.svar()))
            plats.append(b)
        elif u.svarstyp in ("valj", "markera") and u.figur:
            self.klickfigur = Figurvy(u.figur, lage=u.svarstyp)
            plats.append(self.klickfigur)
            tips = tr("Click on your answer in the picture.", "Klicka på svaret i bilden.") if u.svarstyp == "valj" \
                else tr("Click on everything that fits. Click again to undo.", "Klicka på allt som passar. Klicka igen för att ångra.")
            plats.append(Gtk.Label(label=tips, css_classes=["svag"], xalign=0))
            b = Gtk.Button(label=tr("Check", "Kolla"), css_classes=["suggested-action"])
            b.connect("clicked", lambda *_: vid_svar(self.klickfigur.svar()))
            plats.append(b)
        elif u.svarstyp in ("klick", "farga") and u.figur:
            self.klickfigur = Figurvy(u.figur, lage="klick")
            plats.append(self.klickfigur)
            b = Gtk.Button(label=tr("Check", "Kolla"), css_classes=["suggested-action"]); b.connect("clicked", lambda *_: vid_svar(self.klickfigur.svar()))
            plats.append(b)
        elif u.svarstyp in ("punkt", "linje") and u.figur:
            self.klickfigur = Figurvy(u.figur, lage=u.svarstyp)
            plats.append(self.klickfigur)
            tips = tr("Click where the answer should be.", "Klicka där svaret ska vara.") if u.svarstyp == "punkt" else tr("Click two points the line passes through.", "Klicka på två punkter som linjen går genom.")
            plats.append(Gtk.Label(label=tips, css_classes=["svag"], xalign=0))
            b = Gtk.Button(label=tr("Check", "Kolla"), css_classes=["suggested-action"])
            b.connect("clicked", lambda *_: vid_svar(self.klickfigur.svar()) if self.klickfigur.svar() is not None else None)
            plats.append(b)
        elif u.svarstyp == "skiss":
            self.skiss = Skissruta()
            plats.append(Gtk.Label(label=tr("Feel free to draw here – it helps to see the problem. It is not graded.", "Rita gärna här – det hjälper att se problemet. Det rättas inte."), css_classes=["svag"], xalign=0))
            plats.append(self.skiss)
            rad = Gtk.Box(spacing=8)
            e = Gtk.Entry(placeholder_text=tr("Answer …", "Svar …"), hexpand=True, css_classes=["svarsfalt"]); e.connect("activate", lambda *_: vid_svar(e.get_text()))
            b = Gtk.Button(label=tr("Check", "Kolla"), css_classes=["suggested-action"]); b.connect("clicked", lambda *_: vid_svar(e.get_text()))
            r = Gtk.Button(label=tr("Clear", "Sudda")); r.connect("clicked", lambda *_: self.skiss.rensa())
            rad.append(e); rad.append(b); rad.append(r); plats.append(rad); self.svarsfalt.append(e)
        else:
            rad = Gtk.Box(spacing=8)
            e = Gtk.Entry(placeholder_text=tr("Type the answer …", "Skriv svaret …") if u.svarstyp != "uttryck" else tr("e.g. 5x + 3", "t.ex. 5x + 3"), hexpand=True, css_classes=["svarsfalt"])
            e.connect("activate", lambda *_: vid_svar(e.get_text()))
            b = Gtk.Button(label=tr("Check", "Kolla"), css_classes=["suggested-action"]); b.connect("clicked", lambda *_: vid_svar(e.get_text()))
            rad.append(e); rad.append(b); plats.append(rad); self.svarsfalt.append(e)
        if self.svarsfalt:
            self.svarsfalt[0].grab_focus()

    def _markera_svar(self, b: Bedomning, svar):
        for e in self.svarsfalt:
            e.remove_css_class("fel")
        for i in b.fel_falt:
            if i < len(self.svarsfalt):
                self.svarsfalt[i].add_css_class("fel")
        for kn in self.valknappar:
            if kn.get_label() == svar:
                kn.add_css_class("ratt" if b.ratt else "fel")

    # ------------------------------------------------------------ övavyn
    def _bygg_ovavy(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10, css_classes=["kort"])
        self.ova_titel = Gtk.Box(spacing=10, css_classes=["korttitel"], halign=Gtk.Align.CENTER)
        self.ova_fraga = Gtk.Label(css_classes=["fraga"], xalign=0, wrap=True, margin_start=16, margin_end=16)
        self.ova_fokus = Gtk.Label(css_classes=["galler"], xalign=0, wrap=True, visible=False)
        self.ova_steg = Gtk.Label(css_classes=["steg"], xalign=0, wrap=True, visible=False)
        self.ova_losning = Gtk.Label(css_classes=["losning"], xalign=0, wrap=True, visible=False)
        self.ova_tillbaka = Gtk.Box(spacing=8, margin_start=16)
        self.ova_fira = Gtk.Revealer(transition_type=Gtk.RevealerTransitionType.CROSSFADE, transition_duration=180)
        fb = Gtk.Box(spacing=12, css_classes=["fira"])
        fb.append(ikon("pokal", 48))
        ft = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        ft.append(Gtk.Label(label=tr("Mastered!", "Behärskar!"), css_classes=["fira-rubrik"], xalign=0))
        self.ova_fira_text = Gtk.Label(xalign=0, wrap=True)
        ft.append(self.ova_fira_text); fb.append(ft)
        self.ova_fira.set_child(fb); self._fira_box = fb
        self.ova_figur_plats = Gtk.Box(margin_start=16)
        self.ova_svar_plats = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8, margin_start=16, margin_end=16)
        svar_rad = Gtk.Box(spacing=10, margin_start=16, margin_end=16)
        self.ova_ikon = Gtk.Box(); svar_rad.append(self.ova_ikon)
        self.ova_svar_text = Gtk.Label(xalign=0, wrap=True, hexpand=True); svar_rad.append(self.ova_svar_text)
        knappar = Gtk.Box(spacing=8, margin_start=16, margin_bottom=8)
        self.ova_nasta = Gtk.Button(label=tr("Next", "Nästa"), sensitive=False); self.ova_nasta.connect("clicked", lambda *_: self.ny_uppgift())
        visa = Gtk.Button(label=tr("Show the card", "Visa kortet")); visa.connect("clicked", lambda *_: self.stack.set_visible_child_name("kort"))
        ledtrad = Gtk.Button(label=tr("Hint", "Ledtråd")); ledtrad.connect("clicked", lambda *_: self._ledtrad())
        lyssna = Gtk.Button(label="🔊 " + tr("Read aloud", "Läs upp"))
        lyssna.connect("clicked", lambda *_: self.uppgift and self.las_upp(self.uppgift.fraga))
        for b in (self.ova_nasta, ledtrad, lyssna, visa):
            knappar.append(b)
        self.ova_niva = Gtk.Label(css_classes=["svag"], xalign=0, margin_start=16)
        for w in (self.ova_titel, self.ova_fira, self.ova_fraga, self.ova_steg, self.ova_fokus, self.ova_figur_plats,
                  self.ova_svar_plats, svar_rad, self.ova_losning, self.ova_tillbaka, knappar, self.ova_niva):
            box.append(w)
        return Gtk.ScrolledWindow(child=box, hscrollbar_policy=Gtk.PolicyType.NEVER)

    def _niva_for(self, kort_id: str) -> int:
        return adaptiv.niva_for(self.elev.status(kort_id))

    def borja_ova(self):
        if not self.aktuellt:
            return
        self.niva = self._niva_for(self.aktuellt.id)
        tom(self.ova_titel)
        if self.aktuellt.ikon:
            self.ova_titel.append(ikon(self.aktuellt.ikon, 28))
        self.ova_titel.append(Gtk.Label(label=self.aktuellt.titel))
        self.ny_uppgift()
        self.grund.satt_titel(tr("Practise", "Öva") + " · " + self.aktuellt.titel)
        self.stack.set_visible_child_name("ova")

    def ny_uppgift(self):
        k = self.aktuellt
        s = self.elev.status(k.id)
        tillatna = k.mallar
        if self.elev.lekläge:                         # små barn: klicka, dra och välj hellre än skriva
            tillatna = [m for m in k.mallar if m.svarstyp in INTERAKTIVA] or k.mallar
        mall, tranar = adaptiv.valj_mall(k, s, tillatna)
        self.niva = mall.niva
        self.uppgift = gor_uppgift(k, mall)
        self.forsok = 0                               # bara första svaret räknas mot Behärskar
        self.losning_visad = False
        # Tränar vi en feltanke står ledtråden framme från början
        self.ova_fokus.set_visible(tranar and bool(self.uppgift.ledtrad))
        if tranar:
            self.ova_fokus.set_label(tr("Think about: ", "Tänk på: ") + self.uppgift.ledtrad)
        tom(self.ova_tillbaka)
        self.ova_fira.set_reveal_child(False)
        if self.elev.lekläge:
            tal.las(" ".join([self.uppgift.fraga, *self.uppgift.steg]))
        u = self.uppgift
        self.ova_fraga.set_label(u.fraga)
        self.ova_steg.set_label("\n".join(u.steg)); self.ova_steg.set_visible(bool(u.steg))
        self.ova_losning.set_visible(False)
        tom(self.ova_figur_plats)
        if u.figur and u.svarstyp not in ("klick", "farga", "punkt", "linje", "valj", "markera", "dra"):
            self.ova_figur_plats.append(Figurvy(u.figur))
        self._bygg_svar(self.ova_svar_plats, u, self.kolla)
        tom(self.ova_ikon)
        self.ova_svar_text.set_label(""); self.ova_svar_text.set_css_classes([])
        self.ova_nasta.set_sensitive(False)
        self.ova_niva.set_label(tr(f"Level {self.niva} of 3 · {STATUS_TECKEN[s.lage]} {s.ratt_i_rad} correct in a row", f"Nivå {self.niva} av 3 · {STATUS_TECKEN[s.lage]} {s.ratt_i_rad} rätt i rad"))

    def kolla(self, svar):
        if not self.uppgift:
            return
        with mat("ova.kolla"):
            b = bedom(self.uppgift, svar)
        self._markera_svar(b, svar)
        tom(self.ova_ikon)
        if b.otolkbart:
            self.ova_svar_text.set_label(b.text); self.ova_svar_text.set_css_classes(["fel"])
            return
        self.forsok += 1
        if self.forsok == 1:                          # första svaret räknas – nya försök är för att lära sig
            fore = self.elev.status(self.aktuellt.id).lage
            self.elev.svarade(self.aktuellt.id, b.ratt, feltanke_id(self.uppgift, b),
                              mall_index=self.uppgift.mall_index)
            self.elev.spara()
            if fore != BEHARSKAR and self.elev.status(self.aktuellt.id).lage == BEHARSKAR:
                self.fira(self.aktuellt)
        if not b.ratt:                                # fastnat? föreslå ett förkunskapskort
            fore = adaptiv.ga_tillbaka(self.aktuellt, self.elev, self.korten)
            tom(self.ova_tillbaka)
            if fore:
                knapp = Gtk.Button(label=tr("Practise first: ", "Öva först: ") + fore.titel)
                knapp.connect("clicked", lambda *_: (self.visa_kort(fore), self.borja_ova()))
                self.ova_tillbaka.append(Gtk.Label(label=tr("This builds on something earlier.", "Det här bygger på något tidigare."),
                                                   css_classes=["svag"]))
                self.ova_tillbaka.append(knapp)
        # Första felet: förklaringen av feltanken eller ledtråden – eleven får försöka själv.
        # Andra felet: hela lösningen, så att ingen fastnar. Visa hur man tänker, inte bara att det blev fel.
        visa_losning = not b.ratt and self.forsok >= 2 and bool(self.uppgift.losning)
        if visa_losning:
            self.ova_losning.set_label(tr("One way to think:", "Så här kan man tänka:") + "\n"
                                       + "\n".join(f"{i}. {x}" for i, x in enumerate(self.uppgift.losning, 1)))
            self.ova_losning.set_visible(True)
            self.losning_visad = True
        self.ova_ikon.append(ikon(ikoner.RATT if b.ratt else ikoner.FEL, 28))
        text = b.text
        if b.ratt and self.forsok > 1:
            text = tr("Right – now you know how. The next one counts.", "Rätt – nu vet du hur man gör. Nästa uppgift räknas.")
        self.ova_svar_text.set_label(text)
        self.ova_svar_text.set_css_classes(["ratt" if b.ratt else "fel"])
        if self.elev.lekläge:
            tal.las(tr("Right!", "Rätt!") if b.ratt else " ".join([b.text, *(self.uppgift.losning if visa_losning else [])]))
        if b.ratt:
            self.ova_nasta.set_sensitive(True); self.ova_nasta.grab_focus()
        self._fyll_lista(); self._uppdatera_status()

    def fira(self, k: Kort):
        """Kortet sitter: en kort bekräftelse, pokalen, och kortet markeras nytt i regelboken."""
        self.nya_beharskade.add(k.id)
        anim = animationer_pa()
        self.ova_fira.set_transition_duration(180 if anim else 0)
        (self._fira_box.add_css_class if anim else self._fira_box.remove_css_class)("anim")
        self.ova_fira_text.set_label(tr(f"“{k.titel}” is now in your rule book. It will come back for review in a few days.",
                                        f"”{k.titel}” sitter nu och finns i din regelbok. Det kommer tillbaka för repetition om några dagar."))
        self.ova_fira.set_reveal_child(True)
        self.grund.toast(tr("Mastered: ", "Behärskar: ") + k.titel)
        if self.elev.lekläge:
            tal.las(tr("Well done! You know it now.", "Bra jobbat! Nu kan du det."))
        GLib.timeout_add(4000, lambda: (self.ova_fira.set_reveal_child(False), False)[1])

    def _ledtrad(self):
        if self.uppgift and self.uppgift.ledtrad:
            self.ova_svar_text.set_label(self.uppgift.ledtrad); self.ova_svar_text.set_css_classes(["svag"])

    # ------------------------------------------------------------ placering
    def _bygg_placeringsvy(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10, css_classes=["kort"])
        box.append(Gtk.Label(label=tr("How much do you already know?", "Hur mycket kan du redan?"), css_classes=["korttitel"]))
        self.plac_omrade = Gtk.Label(css_classes=["svag"], xalign=0, margin_start=16)
        self.plac_fraga = Gtk.Label(css_classes=["fraga"], xalign=0, wrap=True, margin_start=16, margin_end=16)
        self.plac_figur_plats = Gtk.Box(margin_start=16)
        self.plac_svar_plats = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8, margin_start=16, margin_end=16)
        self.plac_text = Gtk.Label(xalign=0, wrap=True, margin_start=16, css_classes=["svag"])
        knappar = Gtk.Box(spacing=8, margin_start=16)
        vet = Gtk.Button(label=tr("Don't know", "Vet inte")); vet.connect("clicked", lambda *_: self._plac_svar(None))
        hoppa = Gtk.Button(label=tr("Skip the test", "Hoppa över testet")); hoppa.connect("clicked", lambda *_: self._plac_avsluta(hoppade=True))
        knappar.append(vet); knappar.append(hoppa)
        box.append(Gtk.Label(label=tr("Answer as well as you can. Wrong answers are fine – they only show where to start.", "Svara så gott du kan. Fel svar är inte farliga – de visar bara var vi ska börja."),
                             xalign=0, wrap=True, margin_start=16, css_classes=["svag"]))
        for w in (self.plac_omrade, self.plac_fraga, self.plac_figur_plats, self.plac_svar_plats, self.plac_text, knappar):
            box.append(w)
        return Gtk.ScrolledWindow(child=box, hscrollbar_policy=Gtk.PolicyType.NEVER)

    def borja_placering(self):
        self.placeringar = placera_alla(self.elev, self.laroplan, self.korten)
        self.plac_i = 0
        self.grund.satt_titel(tr("How much do you already know?", "Hur mycket kan du redan?"))
        self.stack.set_visible_child_name("placering")
        self._plac_nasta()

    def _plac_nasta(self):
        while self.plac_i < len(self.placeringar) and self.placeringar[self.plac_i].klar:
            self.plac_i += 1
        if self.plac_i >= len(self.placeringar):
            self._plac_avsluta(); return
        p = self.placeringar[self.plac_i]
        k, u = p.nasta()
        self.plac_u = u
        self.plac_omrade.set_label(f"{OMRADEN[p.omrade]} · " + tr(f"question {p.fragor + 1}", f"fråga {p.fragor + 1}"))
        self.plac_fraga.set_label("\n".join([u.fraga, *u.steg]))
        tom(self.plac_figur_plats)
        if u.figur and u.svarstyp not in ("klick", "farga", "punkt", "linje", "valj", "markera", "dra"):
            self.plac_figur_plats.append(Figurvy(u.figur))
        self._bygg_svar(self.plac_svar_plats, u, self._plac_svar)
        self.plac_text.set_label("")

    def _plac_svar(self, svar):
        p = self.placeringar[self.plac_i]
        if svar is None:
            ratt = False
        else:
            b = bedom(self.plac_u, svar)
            if b.otolkbart:
                self.plac_text.set_label(b.text); return
            ratt = b.ratt
        p.svara(ratt)
        self._plac_nasta()

    def _plac_avsluta(self, hoppade: bool = False):
        if not hoppade:
            for p in self.placeringar:
                p.tillampa(self.elev)
        self.elev.placerad = True
        self.elev.spara()
        self._fyll_lista(); self._uppdatera_status()
        self.visa_hem()
        if not hoppade:
            n = self.elev.sammanfattning()
            self.hem_medd.set_label(tr(f"Done. You already seem to know {n['beharskar']} cards – they come back for review in a few days. Start with 'Continue'.",
                                       f"Klart. Du verkar redan kunna {n['beharskar']} kort – de repeteras om några dagar. Börja med 'Fortsätt'."))

    # ------------------------------------------------------------ övrigt
    def _uppdatera_status(self):
        n = self.elev.sammanfattning(); rep = len(self.elev.att_repetera())
        self.grund.satt_status(
            f"○ {tr('Learning', 'Lär')} {n['lar']}   ◐ {tr('Practising', 'Övar')} {n['ovar']}   ● {tr('Mastered', 'Behärskar')} {n['beharskar']}",
            tr(f"{rep} to review today", f"{rep} att repetera i dag") if rep else None)

    def _tangent(self, _c, keyval, _code, state):
        ctrl = state & Gdk.ModifierType.CONTROL_MASK
        if keyval == Gdk.KEY_Escape and self.stack.get_visible_child_name() in ("ova", "placering"):
            self.stack.set_visible_child_name("kort" if self.aktuellt else "hem"); return True
        if ctrl and keyval in (Gdk.KEY_odiaeresis, Gdk.KEY_Odiaeresis):
            self.borja_ova(); return True
        return False


class MatteApp(Adw.Application):
    def __init__(self):
        super().__init__(application_id=APP_ID)
        self.fonster: MatteFonster | None = None

    def do_startup(self):
        Adw.Application.do_startup(self)
        i18n_init()
        css = Gtk.CssProvider()
        try:
            css.load_from_string(CSS)
        except AttributeError:                       # GTK < 4.12
            css.load_from_data(CSS.encode())
        Gtk.StyleContext.add_provider_for_display(Gdk.Display.get_default(), css,
                                                  Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

    def do_activate(self):
        if self.fonster is None:
            with mat("start"):
                self.fonster = MatteFonster(self, Elev.ladda(senaste_elev()))
        self.fonster.present()


def kor(argv=None):
    return MatteApp().run(argv or [])
