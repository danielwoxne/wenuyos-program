#!/usr/bin/python3
"""Klockan – lär dig klockan (wenuyos-program/klockan). Analog och digital tid i sju nivåer, från hel timme till hur
lång tid något tar. Logiken ligger i klocka.py (provas utan fönster); här är fönstret:

  Hem        nivåerna med stjärnor, och Utforska klockan
  Övning     tio uppgifter: läs urtavlan, ställ visarna, digital tid, tid på dygnet, räkna tid
  Resultat   rätt av tio och stjärnor
  Utforska   dra i visarna och se tiden i ord och siffror

Inget nätverk, inget som tickar i bakgrunden. Färgerna kommer från temat (textfärgen och accentfärgen)."""
import math
import os
import random
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, HERE)
import klocka as K  # noqa: E402

import gi  # noqa: E402
gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
gi.require_version("Pango", "1.0")
gi.require_version("PangoCairo", "1.0")
from gi.repository import Adw, GLib, Gtk, Pango, PangoCairo  # noqa: E402

ID = "se.wenuyos.app.klockan"
try:   # libwenuy: WenuyOS fönstergrund, språk och mätning (wenuyos prestanda). Utan den: vanliga libadwaita-fönster.
    gi.require_version("Wenuy", "0")
    from gi.repository import Wenuy
    Wenuy.i18n_init(ID)
    Wenuy.diag_init(ID)
    tr = Wenuy.tr
except (ValueError, ImportError):
    Wenuy = None
    _sv = (os.environ.get("LANGUAGE") or os.environ.get("LC_ALL") or os.environ.get("LC_MESSAGES") or os.environ.get("LANG") or "").startswith("sv")

    def tr(en, sv):
        return sv if _sv else en

SV = tr("en", "sv") == "sv"
STAR, NO_STAR = "★", "☆"


def stars_text(n):
    return STAR * n + NO_STAR * (3 - n)


class ClockFace(Gtk.DrawingArea):
    """urtavlan: ritas med temats färger; editable = visarna går att dra (minutvisaren fäster på nivåns steg)"""

    def __init__(self):
        super().__init__()
        self.h, self.m, self.step = 12, 0, 5
        self.editable = False
        self.minute_help = False     # minuterna 5, 10, 15 … utanför siffrorna
        self.on_change = None
        self._grab = None
        self.set_content_width(300)
        self.set_content_height(300)
        self.set_hexpand(True)
        self.set_vexpand(True)
        self.set_draw_func(self._draw)
        drag = Gtk.GestureDrag()
        drag.connect("drag-begin", self._begin)
        drag.connect("drag-update", self._update)
        drag.connect("drag-end", lambda *_: setattr(self, "_grab", None))
        self.add_controller(drag)

    def set_time(self, h, m):
        self.h, self.m = K.hour12(h), m % 60
        self.queue_draw()

    # ---- geometri
    def _center(self):
        w, h = self.get_width(), self.get_height()
        r = min(w, h) / 2 * (0.80 if self.minute_help else 0.94)
        return w / 2, h / 2, max(r, 10)

    def _polar(self, x, y):
        cx, cy, r = self._center()
        dx, dy = x - cx, y - cy
        return math.degrees(math.atan2(dx, -dy)) % 360, math.hypot(dx, dy) / r

    def _begin(self, _g, x, y):
        if not self.editable:
            return
        self._start = (x, y)
        deg, rad = self._polar(x, y)
        self._grab = "hour" if self.step >= 60 else K.nearest_hand(deg, self.h, self.m, rad)
        self._move(deg)

    def _update(self, _g, dx, dy):
        if self._grab:
            self._move(self._polar(self._start[0] + dx, self._start[1] + dy)[0])

    def _move(self, deg):
        old = (self.h, self.m)
        if self._grab == "minute":
            self.h, self.m = K.drag_minute(self.h, self.m, K.minute_from_angle(deg, self.step))
        else:
            self.h = K.hour_from_angle(deg, self.m)
        if (self.h, self.m) != old:
            self.queue_draw()
            if self.on_change:
                self.on_change(self.h, self.m)

    # ---- ritning
    def _text(self, cr, text, x, y, size, bold, color, alpha=1.0):
        layout = self.create_pango_layout(text)
        font = self.get_pango_context().get_font_description().copy()
        font.set_absolute_size(size * Pango.SCALE)
        font.set_weight(Pango.Weight.BOLD if bold else Pango.Weight.NORMAL)
        layout.set_font_description(font)
        _ink, logical = layout.get_pixel_extents()
        cr.set_source_rgba(color.red, color.green, color.blue, color.alpha * alpha)
        cr.move_to(x - logical.width / 2 - logical.x, y - logical.height / 2 - logical.y)
        PangoCairo.show_layout(cr, layout)

    def _draw(self, _area, cr, _w, _h):
        cx, cy, r = self._center()
        fg = self.get_color()                                               # temats textfärg
        accent = Adw.StyleManager.get_default().get_accent_color_rgba()    # temats accentfärg: timvisaren

        def src(c, alpha=1.0):
            cr.set_source_rgba(c.red, c.green, c.blue, c.alpha * alpha)

        def at(deg, dist):
            a = math.radians(deg)
            return cx + math.sin(a) * dist, cy - math.cos(a) * dist

        cr.arc(cx, cy, r, 0, 2 * math.pi)
        src(fg, 0.05)
        cr.fill_preserve()
        src(fg)
        cr.set_line_width(max(2.0, r * 0.03))
        cr.stroke()
        cr.set_line_cap(1)   # runda ändar
        for i in range(60):
            big = i % 5 == 0
            x1, y1 = at(i * 6, r * (0.90 if big else 0.94))
            x2, y2 = at(i * 6, r * 0.975)
            src(fg, 1.0 if big else 0.55)
            cr.set_line_width(max(1.0, r * (0.022 if big else 0.010)))
            cr.move_to(x1, y1)
            cr.line_to(x2, y2)
            cr.stroke()
        for n in range(1, 13):
            x, y = at(n * 30, r * 0.76)
            self._text(cr, str(n), x, y, r * 0.17, True, fg)
        if self.minute_help:
            for n in range(1, 13):
                x, y = at(n * 30, r * 1.13)
                self._text(cr, str(n * 5 % 60).zfill(2), x, y, r * 0.095, False, fg, 0.65)
        ha, ma = K.angles(self.h, self.m)
        for deg, length, width, color in ((ma, 0.80, 0.045, fg), (ha, 0.50, 0.075, accent)):   # lång = minuter, kort = timmar
            x, y = at(deg, r * length)
            src(color)
            cr.set_line_width(max(2.0, r * width))
            cr.move_to(cx, cy)
            cr.line_to(x, y)
            cr.stroke()
        cr.arc(cx, cy, max(3.0, r * 0.05), 0, 2 * math.pi)
        src(fg)
        cr.fill()


class Klockan:
    def __init__(self, app):
        self.app = app
        self.rng = random.Random()
        self.progress = K.Progress(os.path.join(GLib.get_user_data_dir(), ID, "framsteg.json"))
        self.speaker = shutil.which("espeak-ng")
        self.voice = None
        self.tasks, self.index, self.right, self.answered = [], 0, 0, False
        self.level_id = K.LEVEL_IDS[0]

        if Wenuy:
            self.win = Wenuy.app_window_new(app, tr("Clock Trainer", "Klockan"))
            Wenuy.app_window_get_header_bar(self.win).pack_end(Wenuy.window_actions_button())
        else:
            self.win = Adw.ApplicationWindow(application=app, title=tr("Clock Trainer", "Klockan"))
        self.win.set_default_size(920, 640)
        self.stack = Gtk.Stack(transition_type=Gtk.StackTransitionType.CROSSFADE, transition_duration=150)
        self.stack.add_named(self._home_page(), "home")
        self.stack.add_named(self._play_page(), "play")
        self.stack.add_named(self._result_page(), "result")
        self.stack.add_named(self._free_page(), "free")
        if Wenuy:
            Wenuy.app_window_set_content(self.win, self.stack)
        else:
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
            box.append(Adw.HeaderBar())
            box.append(self.stack)
            self.win.set_content(box)
        self.win.connect("close-request", self._closing)
        self.show_home()

    def _closing(self, *_):
        self._quiet()
        return False

    # ------------------------------------------------------------- hem
    def _home_page(self):
        page = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14, margin_top=24, margin_bottom=24, margin_start=16, margin_end=16)
        title = Gtk.Label(label=tr("Learn the clock", "Lär dig klockan"), xalign=0)
        title.add_css_class("title-1")
        page.append(title)
        self.home_sub = Gtk.Label(xalign=0, wrap=True)
        self.home_sub.add_css_class("dim-label")
        page.append(self.home_sub)
        self.levels = Gtk.ListBox(selection_mode=Gtk.SelectionMode.NONE)
        self.levels.add_css_class("boxed-list")
        page.append(self.levels)
        clamp = Adw.Clamp(maximum_size=720, child=page)
        return Gtk.ScrolledWindow(child=clamp, hscrollbar_policy=Gtk.PolicyType.NEVER)

    def show_home(self):
        self._quiet()
        self.levels.remove_all()
        for n, lv in enumerate(K.LEVELS, 1):
            row = Adw.ActionRow(title=f"{n}. " + tr(*lv["name"]), subtitle=tr(*lv["about"]), activatable=True)
            got = Gtk.Label(label=stars_text(self.progress.stars.get(lv["id"], 0)))
            got.add_css_class("title-3")
            row.add_suffix(got)
            row.add_suffix(Gtk.Image(icon_name="go-next-symbolic"))
            row.connect("activated", lambda _r, lid=lv["id"]: self.start(lid))
            self.levels.append(row)
        free = Adw.ActionRow(title=tr("Explore the clock", "Utforska klockan"),
                             subtitle=tr("Drag the hands and see the time in words and digits.", "Dra i visarna och se tiden i ord och siffror."),
                             activatable=True)
        free.add_suffix(Gtk.Image(icon_name="go-next-symbolic"))
        free.connect("activated", lambda _r: self.show_free())
        self.levels.append(free)
        self.home_sub.set_label(tr("Ten tasks per round. Three stars for all correct.", "Tio uppgifter per omgång. Tre stjärnor för alla rätt.")
                                + "  " + tr("Stars so far:", "Stjärnor hittills:") + f" {self.progress.total()} / {3 * len(K.LEVELS)}")
        self.stack.set_visible_child_name("home")

    # ------------------------------------------------------------- övning
    def _button(self, text, cb, *classes):
        b = Gtk.Button(label=text)
        for c in classes:
            b.add_css_class(c)
        b.connect("clicked", cb)
        return b

    def _play_page(self):
        page = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=20, margin_top=16, margin_bottom=16, margin_start=16, margin_end=16)
        left = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8, hexpand=True, width_request=360)
        self.clock = ClockFace()
        self.clock.on_change = lambda *_: None
        left.append(self.clock)
        self.digital_big = Gtk.Label(vexpand=True, hexpand=True)
        self.digital_big.add_css_class("title-1")
        self.digital_big.add_css_class("numeric")
        big = Pango.AttrList()
        big.insert(Pango.attr_scale_new(3.0))   # som en digital klocka: stora siffror
        self.digital_big.set_attributes(big)
        left.append(self.digital_big)
        self.hint = Gtk.Label(wrap=True, justify=Gtk.Justification.CENTER)
        self.hint.add_css_class("dim-label")
        left.append(self.hint)
        self.help_check = Gtk.CheckButton(label=tr("Show the minutes around the clock", "Visa minuterna runt klockan"), halign=Gtk.Align.CENTER)
        self.help_check.connect("toggled", self._help_toggled)
        left.append(self.help_check)
        page.append(left)

        right = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12, width_request=440)
        top = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        top.append(self._button(tr("Levels", "Nivåer"), lambda _b: self.show_home()))
        self.level_name = Gtk.Label(xalign=0, hexpand=True, ellipsize=Pango.EllipsizeMode.END)
        self.level_name.add_css_class("heading")
        top.append(self.level_name)
        self.count = Gtk.Label()
        self.count.add_css_class("numeric")
        top.append(self.count)
        right.append(top)
        self.bar = Gtk.ProgressBar()
        right.append(self.bar)
        self.question = Gtk.Label(xalign=0, wrap=True)
        self.question.add_css_class("title-2")
        right.append(self.question)
        self.shown = Gtk.Label(xalign=0, wrap=True)
        self.shown.add_css_class("title-1")
        right.append(self.shown)

        self.options_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        self.option_buttons = []
        for i in range(4):
            b = self._button("", lambda _b, i=i: self.answer(i))
            b.set_size_request(-1, 44)
            self.option_buttons.append(b)
            self.options_box.append(b)
        right.append(self.options_box)

        self.set_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        self.set_now = Gtk.Label(xalign=0)
        self.set_now.add_css_class("dim-label")
        self.set_box.append(self.set_now)
        grid = Gtk.Grid(column_spacing=8, row_spacing=8, column_homogeneous=True)
        self.minus_h = self._button(tr("− 1 hour", "− 1 timme"), lambda _b: self.nudge(self.clock, -60))
        self.plus_h = self._button(tr("+ 1 hour", "+ 1 timme"), lambda _b: self.nudge(self.clock, 60))
        self.minus_m = self._button("", lambda _b: self.nudge(self.clock, -self._nudge_step()))
        self.plus_m = self._button("", lambda _b: self.nudge(self.clock, self._nudge_step()))
        grid.attach(self.minus_h, 0, 0, 1, 1)
        grid.attach(self.plus_h, 1, 0, 1, 1)
        grid.attach(self.minus_m, 0, 1, 1, 1)
        grid.attach(self.plus_m, 1, 1, 1, 1)
        self.set_box.append(grid)
        self.done = self._button(tr("Done", "Klar"), lambda _b: self.answer((self.clock.h, self.clock.m)), "suggested-action")
        self.done.set_size_request(-1, 44)
        self.set_box.append(self.done)
        right.append(self.set_box)

        self.feedback = Gtk.Label(xalign=0, wrap=True)
        right.append(self.feedback)
        fill = Gtk.Box(vexpand=True)
        right.append(fill)
        bottom = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        self.listen = self._button(tr("Listen", "Lyssna"), lambda _b: self.say_task())
        self.listen.set_visible(bool(self.speaker))
        bottom.append(self.listen)
        bottom.append(Gtk.Box(hexpand=True))
        self.next = self._button(tr("Next", "Nästa"), lambda _b: self.advance(), "suggested-action")
        self.next.set_size_request(120, 44)
        bottom.append(self.next)
        right.append(bottom)
        page.append(right)
        return page

    def _nudge_step(self):
        step = K.level(self.level_id)["step"]
        return 5 if step >= 60 else step

    def _help_toggled(self, check):
        self.clock.minute_help = check.get_active()
        self.clock.queue_draw()

    def start(self, level_id):
        self.level_id = level_id
        lv = K.level(level_id)
        self.tasks = K.make_round(level_id, self.rng, SV)
        self.index, self.right = 0, 0
        self.level_name.set_label(tr(*lv["name"]))
        self.hint.set_label(tr("Short hand = hours. Long hand = minutes.", "Kort visare = timmar. Lång visare = minuter.") + "\n" + tr(*lv["about"]))
        self.help_check.set_active(level_id in ("fem", "minut"))
        self.help_check.set_visible(lv["step"] < 60)
        step = self._nudge_step()
        self.minus_m.set_label(tr(f"− {step} min", f"− {step} min"))
        self.plus_m.set_label(tr(f"+ {step} min", f"+ {step} min"))
        for b in (self.minus_m, self.plus_m):
            b.set_visible(lv["step"] < 60)
        self.stack.set_visible_child_name("play")
        self.show_task()

    def show_task(self):
        self._quiet()
        t = self.tasks[self.index]
        self.answered = False
        self.count.set_label(f"{self.index + 1} / {len(self.tasks)}")
        self.bar.set_fraction(self.index / len(self.tasks))
        self.question.set_label(t["question"])
        setting = t["target"] is not None
        clock = t["show"] == "clock"
        self.clock.set_visible(clock)
        self.hint.set_visible(clock)
        self.help_check.set_visible(clock and K.level(self.level_id)["step"] < 60)
        self.digital_big.set_visible(not clock)
        self.digital_big.set_label(t["shown"] if not clock else "")
        self.shown.set_visible(setting)
        self.shown.set_label(t["shown"] if setting else "")
        self.clock.step = t["step"]
        self.clock.editable = setting
        if setting:
            self.clock.set_time(12, 0)
            self.clock.on_change = lambda h, m: self._set_changed()
            self._set_changed()
        else:
            self.clock.on_change = None
            self.clock.set_time(t["h"], t["m"])
        self.options_box.set_visible(not setting)
        self.set_box.set_visible(setting)
        for w in (self.minus_h, self.plus_h, self.minus_m, self.plus_m, self.done):
            w.set_sensitive(True)
        if not setting:
            for b, text in zip(self.option_buttons, t["options"]):
                b.set_label(text)
                b.set_sensitive(True)
                b.remove_css_class("success")
                b.remove_css_class("error")
        self.feedback.set_label("")
        self.feedback.remove_css_class("success")
        self.feedback.remove_css_class("error")
        self.next.set_visible(False)

    def _set_changed(self):
        self.set_now.set_label(tr("Drag the hands, or use the buttons.", "Dra i visarna, eller använd knapparna."))

    def nudge(self, clock, minutes):
        clock.set_time(*K.step_time(clock.h, clock.m, minutes))
        if clock.on_change:
            clock.on_change(clock.h, clock.m)

    def answer(self, value):
        if self.answered:
            return
        self.answered = True
        t = self.tasks[self.index]
        ok = K.check(t, value)
        self.right += ok
        if t["target"] is None:
            for i, b in enumerate(self.option_buttons):
                b.set_sensitive(False)
                if i == t["correct"]:
                    b.add_css_class("success")
                elif i == value:
                    b.add_css_class("error")
        else:
            self.clock.editable = False
            for w in (self.minus_h, self.plus_h, self.minus_m, self.plus_m, self.done):
                w.set_sensitive(False)
            if not ok:   # visa hur det skulle se ut
                self.clock.set_time(*t["target"])
        text = (tr("Correct!", "Rätt!") if ok else tr("Not quite.", "Inte riktigt.")) + " " + t["explain"]
        if not ok and t["hint"] and t["show"] == "clock":
            text += "\n" + t["hint"]   # var visarna står
        self.feedback.set_label(text)
        self.feedback.add_css_class("success" if ok else "error")
        self.bar.set_fraction((self.index + 1) / len(self.tasks))
        last = self.index + 1 >= len(self.tasks)
        self.next.set_label(tr("See the result", "Se resultatet") if last else tr("Next", "Nästa"))
        self.next.set_visible(True)
        self.next.grab_focus()

    def advance(self):
        self.index += 1
        if self.index >= len(self.tasks):
            self.show_result()
        else:
            self.show_task()

    # ------------------------------------------------------------- resultat
    def _result_page(self):
        self.result = Adw.StatusPage(icon_name="emblem-ok-symbolic")
        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12, halign=Gtk.Align.CENTER)
        box.append(self._button(tr("Levels", "Nivåer"), lambda _b: self.show_home()))
        self.again = self._button(tr("One more round", "En omgång till"), lambda _b: self.start(self.level_id), "suggested-action")
        box.append(self.again)
        self.result.set_child(box)
        return self.result

    def show_result(self):
        self._quiet()
        total = len(self.tasks)
        got, best = self.progress.record(self.level_id, self.right, total)
        self.result.set_title(tr(f"{self.right} of {total} correct", f"{self.right} av {total} rätt") + "   " + stars_text(got))
        if got == 3:
            text = tr("All correct – you know this level!", "Alla rätt – den här nivån kan du!")
        elif got >= 1:
            text = tr("Well done. Try again for more stars.", "Bra jobbat. Försök igen för fler stjärnor.")
        else:
            text = tr("Keep practising – look at where the short hand points first.", "Öva lite till – titta först var den korta visaren pekar.")
        if best:
            text += "\n" + tr("New best for this level.", "Nytt rekord på den här nivån.")
        self.result.set_description(tr(*K.level(self.level_id)["name"]) + "\n" + text)
        self.stack.set_visible_child_name("result")
        self.again.grab_focus()

    # ------------------------------------------------------------- utforska
    def _free_page(self):
        page = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=20, margin_top=16, margin_bottom=16, margin_start=16, margin_end=16)
        self.free_clock = ClockFace()
        self.free_clock.editable = True
        self.free_clock.minute_help = True
        self.free_clock.on_change = lambda h, m: self._free_changed()
        page.append(self.free_clock)
        right = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12, width_request=340)
        top = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        top.append(self._button(tr("Levels", "Nivåer"), lambda _b: self.show_home()))
        name = Gtk.Label(label=tr("Explore the clock", "Utforska klockan"), xalign=0, hexpand=True)
        name.add_css_class("heading")
        top.append(name)
        right.append(top)
        self.free_words = Gtk.Label(xalign=0, wrap=True)
        self.free_words.add_css_class("title-1")
        right.append(self.free_words)
        self.free_digits = Gtk.Label(xalign=0, wrap=True)
        self.free_digits.add_css_class("title-3")
        self.free_digits.add_css_class("numeric")
        right.append(self.free_digits)
        grid = Gtk.Grid(column_spacing=8, row_spacing=8, column_homogeneous=True)
        for i, (en, sv, minutes) in enumerate((("− 1 hour", "− 1 timme", -60), ("+ 1 hour", "+ 1 timme", 60),
                                               ("− 5 min", "− 5 min", -5), ("+ 5 min", "+ 5 min", 5),
                                               ("− 1 min", "− 1 min", -1), ("+ 1 min", "+ 1 min", 1))):
            grid.attach(self._button(tr(en, sv), lambda _b, n=minutes: self.nudge(self.free_clock, n)), i % 2, i // 2, 1, 1)
        right.append(grid)
        self.free_minutes = Gtk.CheckButton(label=tr("The long hand stops at every minute", "Den långa visaren stannar på varje minut"))
        self.free_minutes.connect("toggled", lambda c: setattr(self.free_clock, "step", 1 if c.get_active() else 5))
        right.append(self.free_minutes)
        tip = Gtk.Label(xalign=0, wrap=True, label=tr("Short hand = hours. Long hand = minutes. Drag a hand to move it.",
                                                      "Kort visare = timmar. Lång visare = minuter. Dra i en visare för att flytta den."))
        tip.add_css_class("dim-label")
        right.append(tip)
        right.append(Gtk.Box(vexpand=True))
        listen = self._button(tr("Listen", "Lyssna"), lambda _b: self.speak(self._free_sentence()))
        listen.set_visible(bool(self.speaker))
        listen.set_halign(Gtk.Align.START)
        right.append(listen)
        page.append(right)
        return page

    def _free_sentence(self):
        return tr("The time is ", "Klockan är ") + K.say(self.free_clock.h, self.free_clock.m, SV)

    def _free_changed(self):
        h, m = self.free_clock.h, self.free_clock.m
        self.free_words.set_label(K.say(h, m, SV).capitalize())
        self.free_digits.set_label(K.digital(h % 12, m) + "  " + tr("or", "eller") + "  " + K.digital(h % 12 + 12, m))

    def show_free(self):
        self._quiet()
        self.free_clock.step = 1 if self.free_minutes.get_active() else 5
        self.free_clock.set_time(3, 0)
        self._free_changed()
        self.stack.set_visible_child_name("free")

    # ------------------------------------------------------------- uppläsning (bara när man trycker Lyssna)
    def say_task(self):
        t = self.tasks[self.index]
        if self.answered:
            self.speak(t["explain"])
        else:
            self.speak(t["question"] + " " + (t["shown"] if t["target"] is not None or t["show"] == "digital" else ""))

    def _quiet(self):
        if self.voice and self.voice.poll() is None:
            self.voice.terminate()
        self.voice = None

    def speak(self, text):
        if not self.speaker:
            return
        self._quiet()
        try:
            self.voice = subprocess.Popen([self.speaker, "-v", "sv" if SV else "en", "-s", "140", "--", text.replace(":", " ")],
                                          stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except OSError:
            self.voice = None


class App(Adw.Application):
    def __init__(self):
        super().__init__(application_id="se.wenuyos.app.klockan")
        self.ui = None

    def do_activate(self):
        if not self.ui:
            self.ui = Klockan(self)
        self.ui.win.present()


if __name__ == "__main__":
    sys.exit(App().run(sys.argv))
