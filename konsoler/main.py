#!/usr/bin/python3
"""Konsoler och hemdatorer – programmet (wenuyos-program/konsoler). Förut en del av Retrospel och Inställningar →
Emulatorer i WenuyOS; sedan 20 nov 2026 ett eget program (Daniel 20 nov: konsolerna och hemdatorerna ut).

Spelen öppnas i Retrospel som förut: retro-tillagg.py bredvid lägger in systemen där. Det här fönstret visar
systemen och deras emulatorer (installeras från Debian/Flathub när du trycker), BIOS/TOS och helskärm.
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


def systems(R, K):
    """→ [(id, namn, emulator, installerad, installationskommando, text)] i Retrospels ordning"""
    out = []
    for s in K.ORDER:
        name, pre, inst = K.emulator_for(s)
        text, _btn = K.describe(s, pre)
        if s in K.MEDNAFEN and pre and s != "PlayStation":
            text = None   # tangenterna står en gång, i rubriken
        out.append((s, R.SYSTEM_NAME.get(s, s), name, bool(pre), inst, text))
    return out


def main():
    R, K = retro()
    import gi
    gi.require_version("Gtk", "4.0")
    gi.require_version("Adw", "1")
    from gi.repository import Adw, Gio, GLib, Gtk
    import subprocess

    class Win(Adw.ApplicationWindow):
        def __init__(self, app):
            super().__init__(application=app, title=tr("Consoles and home computers", "Konsoler och hemdatorer"))
            self.set_default_size(640, 720)
            self.toasts = Adw.ToastOverlay()
            tv = Adw.ToolbarView()
            hb = Adw.HeaderBar()
            tv.add_top_bar(hb)
            self.page = Adw.PreferencesPage()
            tv.set_content(self.page)
            self.toasts.set_child(tv)
            self.set_content(self.toasts)
            self.groups = []
            self.fill()
            self.connect("notify::is-active", lambda *_: self.is_active() and self.fill())   # efter en installation

        def toast(self, t):
            self.toasts.add_toast(Adw.Toast(title=t))

        def group(self, title, desc=None):
            g = Adw.PreferencesGroup(title=title)
            if desc: g.set_description(desc)
            self.page.add(g); self.groups.append(g)
            return g

        def fill(self):
            for g in self.groups: self.page.remove(g)
            self.groups = []
            if not K:
                g = self.group(tr("Consoles and home computers", "Konsoler och hemdatorer"))
                g.add(Adw.ActionRow(title=tr("Retro games is missing", "Retrospel saknas"),
                                    subtitle=tr("Update WenuyOS – this program needs Retro games 0.6 or later.",
                                                "Uppdatera WenuyOS – programmet behöver Retrospel 0.6 eller senare.")))
                return
            g = self.group(tr("Play", "Spela"), tr("Put the games in the Retro folder, one folder per system (or add them in Retro games) – they show up in Retro games.",
                                                   "Lägg spelen i mappen Retro, en mapp per system (eller lägg till dem i Retrospel) – de syns i Retrospel."))
            o = Adw.ActionRow(title=tr("Open Retro games", "Öppna Retrospel"), activatable=True)
            o.connect("activated", lambda *_: subprocess.Popen(["wenuyos-retro"], start_new_session=True))
            g.add(o)
            fs = Adw.SwitchRow(title=tr("Full screen", "Helskärm"), subtitle=tr("Consoles and home computers start in full screen",
                                                                                "Konsolerna och hemdatorerna startar i helskärm"),
                               active=bool(R.load_cfg().get("console_fullscreen")))
            fs.connect("notify::active", lambda row, _p: self.set_cfg("console_fullscreen", row.get_active()))
            g.add(fs)

            gs = self.group(tr("Systems", "System"), tr("Each emulator is free software from Debian (PSP: Flathub). It is downloaded from the internet only when you press Install – you see it happen in a terminal window. "
                                                           "Mednafen: arrow keys and Z/X · Enter = Start · Alt+Shift+1 in the game sets up a gamepad.",
                                                        "Varje emulator är fri programvara från Debian (PSP: Flathub). Den hämtas från internet först när du trycker Installera – du ser det hända i ett terminalfönster. "
                                                           "Mednafen: piltangenterna och Z/X · Enter = Start · Alt+Skift+1 i spelet ställer in en handkontroll."))
            for sid, name, emu, have, inst, text in systems(R, K):
                r = Adw.ActionRow(title=name, subtitle=(emu + " · " + (tr("installed", "installerat") if have else tr("not installed", "inte installerat")))
                                  + (" · " + text if text else ""), use_markup=False)
                if not have and inst:
                    b = Gtk.Button(label=tr("Install", "Installera"), valign=Gtk.Align.CENTER)
                    b.connect("clicked", lambda _b, inst=inst: (subprocess.Popen(["wenuyos-terminal", "-H"] + inst, start_new_session=True),
                                                               self.toast(tr("Installing in a terminal window …", "Installerar i ett terminalfönster …"))))
                    r.add_suffix(b)
                gs.add(r)

            gb = self.group(tr("BIOS and TOS", "BIOS och TOS"),
                            tr("PlayStation needs the BIOS from your own console. Atari ST needs TOS: your own, or the free EmuTOS (zip file from emutos.sourceforge.io).",
                               "PlayStation behöver BIOS från din egen konsol. Atari ST behöver TOS: din egen, eller fria EmuTOS (zip-filen från emutos.sourceforge.io)."))
            add = Gtk.Button(icon_name="list-add-symbolic", tooltip_text=tr("Add BIOS or TOS…", "Lägg in BIOS eller TOS…"))
            add.add_css_class("flat"); add.connect("clicked", lambda *_: self.add_bios()); gb.set_header_suffix(add)
            bl = K.bios_list()
            for b in bl:
                gb.add(Adw.ActionRow(title=os.path.basename(b["file"]), use_markup=False,
                                     subtitle=R.SYSTEM_NAME.get(b["system"], b["system"]) + " · " + (b.get("region") or b.get("kind") or "")
                                     + ("" if b["ok"] else tr(" · not recognised", " · känns inte igen"))))
            if not bl:
                gb.add(Adw.ActionRow(title=tr("None yet", "Inga än")))

        def set_cfg(self, k, v):
            c = R.load_cfg(); c[k] = v; R.save_cfg(c)

        def add_bios(self):
            dlg = Gtk.FileDialog(title=tr("Add BIOS or TOS", "Lägg in BIOS eller TOS"))

            def done(d, res):
                try: files = d.open_multiple_finish(res)
                except GLib.Error: return
                added = []
                for i in range(files.get_n_items()):
                    p = files.get_item(i).get_path()
                    if not p or os.path.getsize(p) > 64 * 1048576: continue
                    try: data = open(p, "rb").read()
                    except OSError: continue
                    added += K.add_file(os.path.basename(p), data)
                self.toast(tr(f"Added: {', '.join(r['name'] for r in added)}", f"Inlagt: {', '.join(r['name'] for r in added)}") if added
                           else tr("Not a PlayStation BIOS or Atari ST TOS", "Inte en PlayStation-BIOS eller Atari ST-TOS"))
                self.fill()
            dlg.open_multiple(self, None, done)

    app = Adw.Application(application_id="se.wenuyos.app.konsoler", flags=Gio.ApplicationFlags.DEFAULT_FLAGS)
    app.connect("activate", lambda a: (a.get_active_window() or Win(a)).present())
    return app.run([sys.argv[0]])


if __name__ == "__main__":
    sys.exit(main())
