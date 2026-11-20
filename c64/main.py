#!/usr/bin/python3
"""Commodore 64 – programmet (wenuyos-program/c64). Förut en del av Retrospel och Inställningar → Emulatorer i
WenuyOS; sedan 20 nov 2026 ett eget program (Daniel 19 nov: bara C64, eget program).

Spelen öppnas i Retrospel som förut: retro-tillagg.py bredvid lägger in C64 där. Det här fönstret är resten:
starta en tom C64 (BASIC), ROM-filerna, helskärm och joystickporten.
  main.py           fönstret
  main.py --basic   en Commodore 64 direkt (den blå BASIC-skärmen)
"""
import importlib.machinery, importlib.util, os, sys

HERE = os.path.dirname(os.path.realpath(__file__))
SV = os.environ.get("LANG", "").startswith("sv")


def tr(en, sv):
    return sv if SV else en


def retro():
    """Retrospel (WenuyOS) och programmets egen kod (retro-kod.py) – i det här programmets process"""
    core = os.environ.get("WENUYOS_RETRO") or "/usr/bin/wenuyos-retro"
    if not os.path.isfile(core):
        return None, None
    sys.path.insert(0, HERE)
    import retro_kor
    try:
        R = retro_kor.core()
    except Exception:   # noqa: BLE001 – en för gammal eller trasig Retrospel: fönstret säger till
        return None, None
    if getattr(R, "TILLAGG_API", 0) != 2:
        return R, None
    R.load_tillagg([HERE])   # namnen på systemen, som i Retrospel (bara beskrivningen)
    return retro_kor.load(R)


def joyport(R):
    return 1 if R.load_cfg().get("c64_joyport") == 1 else 2


def set_cfg(R, key, value):
    c = R.load_cfg()
    c[key] = value
    R.save_cfg(c)


def basic(R, K):
    _, pre, inst = K.emulator_for("C64")
    if not pre:
        return False
    R.start(K.vice_command(pre, [], R.load_cfg()), "Commodore 64")
    return True


def remove_rom(K, path):
    """bara filer i ~/Retro/C64/ROM tas bort"""
    p = os.path.abspath(path)
    if os.path.dirname(p) == os.path.abspath(K.C64DIR) and os.path.isfile(p):
        os.remove(p)
        return True
    return False


def main():
    R, K = retro()
    if "--basic" in sys.argv[1:]:
        if not K or not basic(R, K):
            print(tr("VICE is not installed – open Commodore 64 to install it.", "VICE är inte installerat – öppna Commodore 64 för att installera det."))
            return 1
        return 0

    import gi
    gi.require_version("Gtk", "4.0")
    gi.require_version("Adw", "1")
    from gi.repository import Adw, Gio, GLib, Gtk
    import subprocess

    class Win(Adw.ApplicationWindow):
        def __init__(self, app):
            super().__init__(application=app, title="Commodore 64")
            self.set_default_size(600, 680)
            self.toasts = Adw.ToastOverlay()
            tv = Adw.ToolbarView()
            tv.add_top_bar(Adw.HeaderBar())
            self.page = Adw.PreferencesPage()
            tv.set_content(self.page)
            self.toasts.set_child(tv)
            self.set_content(self.toasts)
            self.rom_rows = []
            self.build()

        def toast(self, t):
            self.toasts.add_toast(Adw.Toast(title=t))

        def group(self, title, desc=None):
            g = Adw.PreferencesGroup(title=title)
            if desc: g.set_description(desc)
            self.page.add(g)
            return g

        def build(self):
            if not K:
                g = self.group("Commodore 64")
                g.add(Adw.ActionRow(title=tr("Retro games is missing", "Retrospel saknas"),
                                    subtitle=tr("Update WenuyOS – Commodore 64 needs Retro games 0.6 or later.",
                                                "Uppdatera WenuyOS – Commodore 64 behöver Retrospel 0.6 eller senare.")))
                return
            name, pre, inst = K.emulator_for("C64")
            g = self.group("Commodore 64", tr("C64 games open in Retro games and from Files – double-click a .d64, .t64, .prg or .crt file.",
                                              "C64-spel öppnas i Retrospel och från Filer – dubbelklicka på en .d64-, .t64-, .prg- eller .crt-fil."))
            r = Adw.ActionRow(title="VICE", subtitle=tr("Installed (free software, GPL-2.0)", "Installerat (fri programvara, GPL-2.0)") if pre
                              else tr("Not installed – Install downloads VICE and Open ROMs from Debian (internet), in a terminal window",
                                                                                         "Inte installerat – Installera hämtar VICE och Open ROMs från Debian (internet), i ett terminalfönster"))
            if not pre:
                b = Gtk.Button(label=tr("Install", "Installera"), valign=Gtk.Align.CENTER)
                b.add_css_class("suggested-action")
                b.connect("clicked", lambda *_: subprocess.Popen(["wenuyos-terminal", "-H"] + inst, start_new_session=True))
                r.add_suffix(b)
            else:
                b = Gtk.Button(label=tr("Start a Commodore 64", "Starta en Commodore 64"), valign=Gtk.Align.CENTER)
                b.add_css_class("suggested-action")
                b.connect("clicked", lambda *_: basic(R, K))
                r.add_suffix(b)
            g.add(r)
            fs = Adw.SwitchRow(title=tr("Full screen", "Helskärm"), subtitle=tr("Otherwise in a window that you can move and resize",
                                                                                "Annars i ett fönster som du kan flytta och ändra storlek på"),
                               active=bool(R.load_cfg().get("c64_fullscreen")))
            fs.connect("notify::active", lambda row, _p: set_cfg(R, "c64_fullscreen", row.get_active()))
            g.add(fs)
            jp = Adw.ComboRow(title=tr("Joystick port", "Joystickport"), subtitle=tr("Most C64 games use port 2", "De flesta C64-spel använder port 2"))
            jp.set_model(Gtk.StringList.new([tr("Port 2 (most games)", "Port 2 (de flesta spel)"), tr("Port 1", "Port 1")]))
            jp.set_selected(1 if joyport(R) == 1 else 0)
            jp.connect("notify::selected", lambda row, _p: set_cfg(R, "c64_joyport", 1 if row.get_selected() == 1 else 2))
            g.add(jp)
            o = Adw.ActionRow(title=tr("Open Retro games", "Öppna Retrospel"), subtitle=tr("All your games in one list", "Alla dina spel i en lista"),
                              activatable=True)
            o.connect("activated", lambda *_: subprocess.Popen(["wenuyos-retro"], start_new_session=True))
            g.add(o)

            self.rg = self.group(tr("C64 ROM", "C64-ROM"),
                                 tr("Add your own legal ROM files (KERNAL, BASIC, character set, 1541) – they are recognised by their contents. "
                                    "Without them, Open ROMs is used: free replacement ROMs.",
                                    "Lägg in dina egna, lagliga ROM-filer (KERNAL, BASIC, teckenuppsättning, 1541) – de känns igen på innehållet. "
                                    "Utan dem används Open ROMs: fria ersättnings-ROM:ar."))
            add = Gtk.Button(icon_name="list-add-symbolic", tooltip_text=tr("Add ROM files…", "Lägg till ROM-filer…"))
            add.add_css_class("flat")
            add.connect("clicked", lambda *_: self.add_roms())
            self.rg.set_header_suffix(add)
            self.using = Adw.ActionRow(title=tr("ROMs in use", "ROM:ar som används"))
            self.rg.add(self.using)
            self.fill_roms()

        def fill_roms(self):
            for r in self.rom_rows: self.rg.remove(r)
            self.rom_rows = []
            s = K.c64_set()
            t = (tr("Open ROMs – most machine-code games work, BASIC programs need the original ROMs",
                    "Open ROMs – de flesta spel i maskinkod fungerar, BASIC-program behöver original-ROM:arna") if s["open"]
                 else tr("Your own KERNAL and BASIC", "Din egen KERNAL och BASIC"))
            t += tr(" · real 1541 drive", " · riktig 1541-station") if s["dos1541"] else tr(" · fast virtual disk drive", " · snabb virtuell diskettstation")
            self.using.set_subtitle(t)
            for rom in K.c64_roms():
                row = Adw.ActionRow(title=rom["name"], subtitle=K.C64_TYPES.get(rom["kind"], "") + " · " + os.path.basename(rom["file"]), use_markup=False)
                d = Gtk.Button(icon_name="user-trash-symbolic", valign=Gtk.Align.CENTER, tooltip_text=tr("Remove", "Ta bort"))
                d.add_css_class("flat")
                d.connect("clicked", lambda _b, f=rom["file"]: (remove_rom(K, f), self.fill_roms(), self.toast(tr("ROM removed", "ROM-filen är borttagen"))))
                row.add_suffix(d)
                self.rg.add(row)
                self.rom_rows.append(row)

        def add_roms(self):
            dlg = Gtk.FileDialog(title=tr("Add C64 ROM files", "Lägg till C64-ROM-filer"))

            def done(d, res):
                try: files = d.open_multiple_finish(res)
                except GLib.Error: return
                added = []
                for i in range(files.get_n_items()):
                    p = files.get_item(i).get_path()
                    if not p: continue
                    try: data = R.rom_bytes(p)
                    except OSError: data = None
                    added += K.add_file(os.path.basename(p), data or b"")
                self.toast(tr(f"Added: {', '.join(r['name'] for r in added)}", f"Inlagt: {', '.join(r['name'] for r in added)}") if added
                           else tr("Not a C64 ROM", "Inte en C64-ROM"))
                self.fill_roms()
            dlg.open_multiple(self, None, done)

    app = Adw.Application(application_id="se.wenuyos.app.c64", flags=Gio.ApplicationFlags.DEFAULT_FLAGS)
    app.connect("activate", lambda a: (a.get_active_window() or Win(a)).present())
    return app.run([sys.argv[0]])


if __name__ == "__main__":
    sys.exit(main())
