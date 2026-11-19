#!/usr/bin/python3
"""Android-appar – fönstret för Android i WenuyOS (Waydroid).

Förut sidan Inställningar → Android; sedan 20 nov 2026 ett eget program (Daniel 19 nov: Android ut ur systemet).
Allt görs med kommandot wenuyos-android bredvid (samma som i terminalen). Root-delen ligger kvar i systemet:
/usr/lib/wenuyos/wenuyos-android-root med fasta kommandon.

  main.py            öppna fönstret
  main.py --setup    bara genvägarna (Start, sökfältet, kommandot wenuyos-android) – görs också när fönstret öppnas
"""
import json
import os
import subprocess
import sys
import threading

HERE = os.path.dirname(os.path.realpath(__file__))
CMD = os.path.join(HERE, "bin", "wenuyos-android")
SV = os.environ.get("LANG", "").startswith("sv")


def tr(en, sv):
    return sv if SV else en


def cfg(key):
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.join(os.path.expanduser("~"), ".config")
    try:
        with open(os.path.join(base, "wenuyos", "android-" + key), encoding="utf-8") as f:
            return f.read().strip()
    except OSError:
        return ""


def run(args, timeout=20):
    try:
        r = subprocess.run(["bash", CMD] + args, capture_output=True, text=True, timeout=timeout)
        return r.returncode == 0, r.stdout
    except (OSError, subprocess.TimeoutExpired):
        return False, ""


def apps():
    """installerade Android-appar → [(paket, namn)]"""
    out = []
    for line in run(["apps"])[1].splitlines():
        if "\t" in line:
            pkg, name = line.split("\t", 1)
            out.append((pkg, name))
    return out


def status_text(j):
    """status --json → (rad under Android, installerat, körs)"""
    installed, running = bool(j.get("installed")), bool(j.get("running"))
    if not installed:
        return tr("Not enabled – each Android app in its own window, no Google services",
                  "Inte påslaget – varje Android-app i eget fönster, utan Googles tjänster"), False, False
    sub = (tr("Closed – starts when you open an Android app", "Stängt – startar när du öppnar en Android-app") if not running
           else tr("Paused (no Android windows open)", "Pausat (inga Android-fönster öppna)") if j.get("frozen")
           else tr("Running", "Körs"))
    mem = str(j.get("memory_mb", ""))
    if running and mem not in ("", "0"):
        sub += " · " + mem + tr(" MB memory", " MB minne")
    gfx = tr("software", "mjukvara") if j.get("graphics") == "software" else tr("graphics card", "grafikkortet")
    sub += " · " + str(j.get("apps", 0)) + tr(" apps · graphics: ", " appar · grafik: ") + gfx
    return sub, True, running


def terminal(args):
    subprocess.Popen(["wenuyos-terminal", "-H"] + args, start_new_session=True)


def main():
    if "--setup" in sys.argv[1:]:
        return subprocess.call(["bash", CMD, "setup"])
    subprocess.call(["bash", CMD, "setup"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    import gi
    gi.require_version("Gtk", "4.0")
    gi.require_version("Adw", "1")
    from gi.repository import Adw, GLib, Gio, Gtk

    class Win(Adw.ApplicationWindow):
        def __init__(self, app):
            super().__init__(application=app, title=tr("Android apps", "Android-appar"))
            self.set_default_size(620, 720)
            self.timer = 0
            self.toasts = Adw.ToastOverlay()
            tv = Adw.ToolbarView()
            tv.add_top_bar(Adw.HeaderBar())
            self.page = Adw.PreferencesPage()
            tv.set_content(self.page)
            self.toasts.set_child(tv)
            self.set_content(self.toasts)
            self.build()
            self.connect("map", lambda *_: self.start_timer())
            self.connect("unmap", lambda *_: self.stop_timer())
            self.connect("close-request", lambda *_: self.stop_timer() or False)

        # ---- hjälpare
        def toast(self, text):
            self.toasts.add_toast(Adw.Toast(title=text))

        def later(self, args, done=None, msg=True):
            """kör i bakgrunden; fönstret står kvar under tiden"""
            def finish(ok, out):
                if msg:
                    out = out.strip()
                    self.toast((out or tr("Applies the next time Android starts", "Gäller nästa gång Android startar")) if ok
                               else tr("Could not change – is Android enabled?", "Kunde inte ändra – är Android påslaget?"))
                if done:
                    done()
                return False
            threading.Thread(target=lambda: GLib.idle_add(finish, *run(args, timeout=120)), daemon=True).start()

        def group(self, title, desc=None):
            g = Adw.PreferencesGroup(title=title)
            if desc:
                g.set_description(desc)
            self.page.add(g)
            return g

        def choice(self, g, title, sub, opts, cur, on):
            r = Adw.ComboRow(title=title)
            if sub:
                r.set_subtitle(sub)
            r.set_model(Gtk.StringList.new([o[1] for o in opts]))
            keys = [o[0] for o in opts]
            r.set_selected(keys.index(cur) if cur in keys else 0)
            r.connect("notify::selected", lambda row, _p: on(keys[row.get_selected()]))
            g.add(r)
            return r

        def link(self, g, title, sub, icon, fn):
            r = Adw.ActionRow(title=title, subtitle=sub or "", activatable=True)
            r.add_prefix(Gtk.Image.new_from_icon_name(icon))
            r.add_suffix(Gtk.Image.new_from_icon_name("go-next-symbolic"))
            r.connect("activated", lambda *_: fn())
            g.add(r)

        def button(self, label, primary, fn):
            b = Gtk.Button(label=label, valign=Gtk.Align.CENTER)
            if primary:
                b.add_css_class("suggested-action")
            b.connect("clicked", lambda *_: fn())
            return b

        # ---- sidan (samma som Inställningar → Android före 0.6)
        def build(self):
            g = self.group(tr("Android apps", "Android-appar"),
                           tr("Real Android (LineageOS) sharing the computer's Linux kernel – not an emulator.",
                              "Riktigt Android (LineageOS) som delar datorns Linux-kärna – ingen emulator."))
            self.status = Adw.ActionRow(title="Android", subtitle=tr("Checking…", "Kontrollerar…"))
            self.status.add_prefix(Gtk.Image.new_from_icon_name("se.wenuyos.app.android"))
            self.buttons = Gtk.Box(spacing=6, valign=Gtk.Align.CENTER)
            self.status.add_suffix(self.buttons)
            g.add(self.status)

            gv = self.group(tr("Display", "Visning"))
            self.choice(gv, tr("Show Android apps", "Visa Android-appar"), None,
                        [("apps", tr("Each app in its own window", "Varje app i eget fönster")),
                         ("single", tr("Whole Android in one window", "Hela Android i ett fönster"))],
                        cfg("mode") or "apps", lambda v: self.later(["mode", v]))
            disp = cfg("display") or "auto"
            self.choice(gv, tr("Size of the Android window", "Storlek på Android-fönstret"),
                        tr("Android's own resolution follows the window", "Androids upplösning följer fönstret"),
                        [("auto", tr("Large (80 % of the screen)", "Stort (80 % av skärmen)")),
                         ("medium", tr("Medium (tablet)", "Mellan (surfplatta)")),
                         ("phone", tr("Phone (standing)", "Telefon (stående)")), ("full", tr("Whole screen", "Hela skärmen"))],
                        "auto" if disp == "large" else disp, lambda v: self.later(["display", v]))
            self.choice(gv, tr("Text size in Android", "Textstorlek i Android"), None,
                        [("auto", tr("Automatic (by screen)", "Automatiskt (efter skärmen)")), ("140", tr("Small", "Liten")),
                         ("160", tr("Normal", "Normal")), ("200", tr("Large", "Stor")), ("240", tr("Larger", "Större"))],
                        cfg("density") or "auto", lambda v: self.later(["density", v]))

            gm = self.group(tr("Mouse", "Mus"))
            tm = (run(["touch", "mode"])[1].split() or ["auto"])[0]
            if tm not in ("none", "all", "some"):
                tm = "auto"

            def touch(v):
                if v == "some":
                    self.later(["touch", "none"], self.refresh, msg=False)
                    self.toast(tr("Choose the apps below", "Välj apparna nedan"))
                else:
                    self.later(["touch", v], self.refresh, msg=False)
            self.choice(gm, tr("The mouse in Android apps", "Musen i Android-appar"),
                        tr("Games may need the mouse to act as a finger", "Spel kan behöva att musen fungerar som ett finger"),
                        [("auto", tr("Automatic (finger in games when the computer has no touch screen)",
                                     "Automatiskt (finger i spel när datorn saknar pekskärm)")),
                         ("none", tr("Mouse (right click, scroll wheel)", "Mus (högerklick, rullhjul)")),
                         ("all", tr("Finger in all apps", "Finger i alla appar")),
                         ("some", tr("Finger in selected apps", "Finger i valda appar"))], tm, touch)
            self.installed_apps = apps()
            r = Adw.ComboRow(title=tr("Finger in this app", "Finger i den här appen"))
            r.set_model(Gtk.StringList.new([n for _, n in self.installed_apps]))
            add = Gtk.Button(label=tr("Add", "Lägg till"), valign=Gtk.Align.CENTER)

            def add_app(*_):
                i = r.get_selected()
                if i < len(self.installed_apps):
                    self.later(["touch", "add", self.installed_apps[i][0]], self.refresh, msg=False)
            add.connect("clicked", add_app)
            r.add_suffix(add)
            if not self.installed_apps:
                r.set_sensitive(False)
                r.set_subtitle(tr("No Android apps installed yet", "Inga Android-appar installerade än"))
            gm.add(r)
            self.touch_group = self.group(tr("Apps where the mouse is a finger", "Appar där musen är ett finger"))
            self.touch_list = Gtk.ListBox(selection_mode=Gtk.SelectionMode.NONE)
            self.touch_list.add_css_class("boxed-list")
            self.touch_group.add(self.touch_list)

            gf = self.group(tr("Files and memory", "Filer och minne"))
            sh = Adw.SwitchRow(title=tr("Shared folders", "Delade mappar"),
                               subtitle=tr("Documents, Pictures, Downloads, Music and Videos are visible in Android apps",
                                           "Dokument, Bilder, Hämtade filer, Musik och Videor syns i Android-apparna"),
                               active=cfg("share") != "off")
            sh.connect("notify::active", lambda row, _p: self.later(["share", "on" if row.get_active() else "off"]))
            gf.add(sh)
            self.link(gf, tr("Android's own storage", "Androids egen lagring"), tr("Opens in Files (~/Android)", "Öppnas i Filer (~/Android)"),
                      "folder-symbolic", lambda: subprocess.Popen(["wenuyos-files", os.path.expanduser("~/Android")], start_new_session=True))
            self.choice(gf, tr("Fast start", "Snabb start"),
                        tr("Android is prepared in the background after you log in and then paused, so apps open at once. Uses memory (about 1 GB) but no processor.",
                           "Android förbereds i bakgrunden efter inloggningen och pausas sedan, så att appar öppnas direkt. Tar minne (ungefär 1 GB) men ingen processor."),
                        [("auto", tr("Automatic (4 GB memory or more, and Android apps in use)", "Automatiskt (minst 4 GB minne, och Android-appar används)")),
                         ("on", tr("Always", "Alltid")), ("off", tr("Off", "Av"))],
                        cfg("prestart") or "auto", lambda v: self.later(["prestart", v]))
            self.choice(gf, tr("Close Android when not used", "Stäng Android när det inte används"),
                        tr("Paused one minute after the last Android window closes – frees memory",
                           "Pausas en minut efter att sista Android-fönstret stängts – frigör minne"),
                        [("auto", tr("Automatic (after 10 minutes; kept paused when Fast start applies)",
                                     "Automatiskt (efter 10 minuter; ligger kvar pausat när Snabb start gäller)")),
                         ("5", tr("After 5 minutes", "Efter 5 minuter")), ("10", tr("After 10 minutes", "Efter 10 minuter")),
                         ("30", tr("After 30 minutes", "Efter 30 minuter")), ("never", tr("Never (only pause)", "Aldrig (bara pausa)"))],
                        cfg("autostop") or "auto", lambda v: self.later(["autostop", v]))
            self.choice(gf, tr("Graphics in Android", "Grafik i Android"),
                        tr("Software is slower but works on all computers", "Mjukvara är långsammare men fungerar på alla datorer"),
                        [("auto", tr("Automatic", "Automatiskt")), ("hardware", tr("Graphics card", "Grafikkortet")),
                         ("software", tr("Software", "Mjukvara"))],
                        cfg("graphics") or "auto", lambda v: self.later(["graphics", v]))

            gs = self.group(tr("App stores and services", "Appbutiker och tjänster"))
            self.link(gs, "F-Droid", tr("Free app store – only free apps", "Fri appbutik – bara fria appar"),
                      "system-software-install-symbolic", lambda: terminal([CMD, "fdroid"]))
            self.link(gs, "Aurora Store", tr("Apps from Google Play without a Google account", "Appar från Google Play utan Google-konto"),
                      "system-software-install-symbolic", lambda: terminal([CMD, "aurora"]))
            self.link(gs, "microG", tr("Installed – settings in Android: microG Settings", "Installerat – inställningar i Android: microG-inställningar")
                      if cfg("microg") == "yes" else tr("Free replacement for Google services – for apps that need them",
                                                         "Fri ersättning för Googles tjänster – för appar som behöver dem"),
                      "security-high-symbolic", lambda: terminal([CMD, "microg"]))
            self.link(gs, tr("Install an APK file…", "Installera en APK-fil…"), tr("Or double-click the file in Files", "Eller dubbelklicka på filen i Filer"),
                      "document-open-symbolic", self.pick_apk)

            gu = self.group(tr("Maintenance", "Underhåll"))
            self.link(gu, tr("Update Android", "Uppdatera Android"),
                      tr("LineageOS updates – also done by WenuyOS updates", "LineageOS-uppdateringar – görs även av WenuyOS-uppdateringar"),
                      "software-update-available-symbolic", lambda: terminal([CMD, "upgrade"]))
            self.link(gu, tr("Android from USB", "Android från USB"),
                      tr("Signed Android images on a USB stick – no download", "Signerade Android-avbildningar på ett USB-minne – ingen nedladdning"),
                      "drive-removable-media-symbolic", lambda: terminal(["wenuyos", "update", "--from", "usb"]))
            self.link(gu, tr("Turn Android off", "Stäng av Android helt"),
                      tr("Apps and data are kept – enable again here", "Appar och data behålls – slå på igen här"),
                      "system-shutdown-symbolic", lambda: terminal([CMD, "disable"]))

        def pick_apk(self):
            d = Gtk.FileDialog(title=tr("Install an APK file", "Installera en APK-fil"))
            f = Gtk.FileFilter()
            f.set_name("APK")
            for p in ("*.apk", "*.xapk", "*.apks", "*.apkm"):
                f.add_pattern(p)
            store = Gio.ListStore.new(Gtk.FileFilter)
            store.append(f)
            d.set_filters(store)

            def done(dlg, res):
                try:
                    file = dlg.open_finish(res)
                except GLib.Error:
                    return
                if file and file.get_path():
                    subprocess.Popen([CMD, "apk", file.get_path()], start_new_session=True)
            d.open(self, None, done)

        # ---- status var 5:e sekund medan fönstret syns
        def start_timer(self):
            self.refresh()
            if not self.timer:
                self.timer = GLib.timeout_add_seconds(5, lambda: self.refresh() or True)

        def stop_timer(self):
            if self.timer:
                GLib.source_remove(self.timer)
                self.timer = 0

        def refresh(self):
            def work():
                ok, out = run(["status", "--json"])
                try:
                    j = json.loads(out) if ok and out.strip() else {}
                except ValueError:
                    j = {}
                GLib.idle_add(lambda: self.show_status(j if isinstance(j, dict) else {}) or False)
            threading.Thread(target=work, daemon=True).start()

        def show_status(self, j):
            sub, installed, running = status_text(j)
            self.status.set_subtitle(sub)
            while (c := self.buttons.get_first_child()):
                self.buttons.remove(c)
            if not installed:
                self.buttons.append(self.button(tr("Enable…", "Aktivera…"), True, lambda: terminal([CMD, "enable"])))
            else:
                self.buttons.append(self.button(tr("Window", "Fönster"), False, lambda: subprocess.Popen([CMD, "window"], start_new_session=True)))
                self.buttons.append(self.button(tr("Full screen", "Helskärm"), False, lambda: subprocess.Popen([CMD, "full"], start_new_session=True)))
                if running:
                    self.buttons.append(self.button(tr("Close", "Stäng"), False, lambda: self.later(["stop"], self.refresh, msg=False)))
            self.fill_touch(str(j.get("touch", "")) if installed else "")   # förvalet (Among Us) bara när Android finns

        def fill_touch(self, lst):
            while (c := self.touch_list.get_first_child()):
                self.touch_list.remove(c)
            pkgs = [p for p in lst.split(",") if p] if lst and lst != "*" else []
            self.touch_group.set_visible(bool(pkgs))
            names = dict(self.installed_apps)
            for p in pkgs:
                r = Adw.ActionRow(title=names.get(p, p), subtitle=p, use_markup=False)
                rm = Gtk.Button(icon_name="user-trash-symbolic", valign=Gtk.Align.CENTER,
                                tooltip_text=tr("Mouse again in this app", "Mus igen i den här appen"))
                rm.add_css_class("flat")
                rm.connect("clicked", lambda _b, p=p: self.later(["touch", "remove", p], self.refresh, msg=False))
                r.add_suffix(rm)
                self.touch_list.append(r)

    app = Adw.Application(application_id="se.wenuyos.app.android", flags=Gio.ApplicationFlags.DEFAULT_FLAGS)

    def activate(a):
        w = a.get_active_window() or Win(a)
        w.present()
    app.connect("activate", activate)
    return app.run([sys.argv[0]])


if __name__ == "__main__":
    sys.exit(main())
