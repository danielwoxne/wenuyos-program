"""Skalet – WenuyOS fönstergrund via libwenuy (GObject Introspection).

Matte är ett WenuyOS-program: titelrad med ikon och fet titel till vänster,
sidopanel med egen titelrad, statusrad, toast, ⋯-knappen med fönsterfunktioner,
temat och ikonerna kommer från libwenuy – samma som Filer och Inställningar.

Saknas libwenuy (t.ex. på en vanlig Ubuntu-dator under utveckling) faller
skalet tillbaka på libadwaita med samma funktionsnamn, så koden i fonster.py
har en enda väg. `HAR_WENUY` säger vilket det blev.
"""
from __future__ import annotations

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
from gi.repository import Adw, Gtk  # noqa: E402

try:
    gi.require_version("Wenuy", "0")
    from gi.repository import Wenuy  # noqa: E402
    HAR_WENUY = True
except (ValueError, ImportError):
    Wenuy = None
    HAR_WENUY = False

APP_ID = "se.wenuyos.app.matte"


def i18n_init():
    if HAR_WENUY:
        Wenuy.i18n_init(APP_ID)


def tr(en: str, sv: str) -> str:
    """Engelska + svenska alltid, som i alla WenuyOS-program."""
    if HAR_WENUY:
        return Wenuy.tr(en, sv)
    import os
    return sv if (os.environ.get("LANGUAGE") or os.environ.get("LANG", "")).startswith("sv") else en


class Fonstergrund:
    """Ett WenuyOS-fönster med sidopanel. Samma anrop oavsett libwenuy eller reserv."""

    def __init__(self, app, titel: str):
        if HAR_WENUY:
            self.win = Wenuy.app_window_new_with_sidebar(app, titel, Wenuy.SidebarMode.PAGES)
            header = Wenuy.app_window_get_header_bar(self.win)
            header.pack_end(Wenuy.window_actions_button())
        else:
            self.win = Adw.ApplicationWindow(application=app, title=titel)
            self._split = Adw.OverlaySplitView(min_sidebar_width=200, max_sidebar_width=260)
            sb = Adw.ToolbarView(); sb.add_top_bar(Adw.HeaderBar(title_widget=Gtk.Label(label=titel, css_classes=["heading"])))
            self._sb = sb; self._split.set_sidebar(sb)
            self._cv = Adw.ToolbarView(); self._header = Adw.HeaderBar(); self._cv.add_top_bar(self._header)
            self._status = Gtk.Box(css_classes=["wenuy-statusbar"], visible=False)
            self._status_start = Gtk.Label(xalign=0, hexpand=True, css_classes=["dim-label", "caption"])
            self._status_end = Gtk.Label(xalign=1, css_classes=["dim-label", "caption"])
            for w in (self._status_start, self._status_end):
                w.set_margin_start(10); w.set_margin_end(10); w.set_margin_top(3); w.set_margin_bottom(3)
                self._status.append(w)
            self._cv.add_bottom_bar(self._status)
            self._split.set_content(self._cv)
            self._toasts = Adw.ToastOverlay(child=self._split)
            self.win.set_content(self._toasts)
        self.win.set_default_size(1080, 740)

    def satt_sidopanel(self, w: Gtk.Widget):
        if HAR_WENUY:
            Wenuy.app_window_set_sidebar(self.win, w)
        else:
            self._sb.set_content(w)

    def satt_innehall(self, w: Gtk.Widget):
        if HAR_WENUY:
            Wenuy.app_window_set_content(self.win, w)
        else:
            self._cv.set_content(w)

    def satt_titel(self, titel: str | None):
        if HAR_WENUY:
            Wenuy.app_window_set_content_title(self.win, titel)
        else:
            self._header.set_title_widget(Gtk.Label(label=titel or "", css_classes=["heading"]))

    def visa_innehall(self):
        """Efter ett val i sidopanelen: i ett smalt fönster visas innehållet."""
        if HAR_WENUY:
            Wenuy.app_window_show_content(self.win)
        elif self._split.get_collapsed():
            self._split.set_show_sidebar(False)

    def satt_status(self, start: str | None, slut: str | None = None):
        if HAR_WENUY:
            Wenuy.app_window_set_status(self.win, start, slut)
        else:
            self._status_start.set_label(start or ""); self._status_end.set_label(slut or "")
            self._status.set_visible(bool(start or slut))

    def toast(self, text: str):
        if HAR_WENUY:
            Wenuy.app_window_toast(self.win, text)
        else:
            self._toasts.add_toast(Adw.Toast(title=text))

    def titelrad(self) -> Gtk.Widget:
        return Wenuy.app_window_get_header_bar(self.win) if HAR_WENUY else self._header


def accentfarg() -> str:
    """Temats accentfärg som #rrggbb (WenuyOS orange om libwenuy saknas)."""
    if HAR_WENUY:
        try:
            from gi.repository import Gdk
            rgba = Gdk.RGBA(); Wenuy.Theme.get_default().get_accent(rgba)
            return "#%02x%02x%02x" % (int(rgba.red * 255), int(rgba.green * 255), int(rgba.blue * 255))
        except Exception:  # noqa: BLE001
            pass
    return "#e8821c"


def prestandaprofil() -> str:
    """WenuyOS prestandaprofil (persist.wenuyos.perf): low / normal / high. Egenskaperna är
    filer i /var/lib/wenuyos/props (beständiga) och /run/wenuyos/props (tillfälliga)."""
    import os
    for mapp in ("/run/wenuyos/props", "/var/lib/wenuyos/props"):
        try:
            with open(os.path.join(mapp, "persist.wenuyos.perf"), encoding="utf-8") as f:
                v = f.read().strip()
                if v:
                    return v
        except OSError:
            continue
    return "normal"


def animationer_pa() -> bool:
    """Animationer bara om profilen inte är low och användaren inte stängt av dem."""
    if prestandaprofil() == "low":
        return False
    try:
        installning = Gtk.Settings.get_default()
        return bool(installning.get_property("gtk-enable-animations")) if installning else True
    except Exception:  # noqa: BLE001
        return True


def mat(namn: str):
    """Mätning till programmets diag-rapport (wenuy_diag) – ingenting utan libwenuy."""
    class _M:
        def __enter__(self):
            self.t = Wenuy.diag_measure_begin(namn) if HAR_WENUY else None; return self

        def __exit__(self, typ, *_):
            if HAR_WENUY:
                Wenuy.diag_measure_end(self.t, typ is None)
    return _M()
