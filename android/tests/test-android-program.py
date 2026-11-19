#!/usr/bin/env python3
"""Android-appar som eget program (Daniel 19 nov 2026): genvägarna, kommandot och fönstrets logik.
Inget Android körs här – setup provas i en tillfällig hemmapp."""
import importlib.machinery
import importlib.util
import os
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]   # programmappen android/
SCRIPT = ROOT / "bin/wenuyos-android"
fails, total = [], 0


def check(ok, what):
    global total
    total += 1
    print(("  ok   " if ok else "  FEL  ") + what)
    if not ok:
        fails.append(what)


home = pathlib.Path(tempfile.mkdtemp(prefix="test-android-program-"))
apps = home / ".local/share/applications"
apps.mkdir(parents=True)
old = apps / "waydroid.com.exempel.app.desktop"
old.write_text("[Desktop Entry]\nType=Application\nName=Exempel\nExec=/usr/lib/wenuyos/wenuyos-android launch com.exempel.app\n")
env = {**os.environ, "HOME": str(home), "XDG_DATA_HOME": str(home / ".local/share"), "PATH": "/usr/bin:/bin", "LANG": "sv_SE.UTF-8"}
r = subprocess.run(["bash", str(SCRIPT), "setup"], env=env, capture_output=True, text=True)
link = home / ".local/bin/wenuyos-android"
check(r.returncode == 0, "setup lyckas utan Android och utan update-desktop-database")
check(link.is_symlink() and link.resolve() == SCRIPT.resolve(), "kommandot ~/.local/bin/wenuyos-android pekar på programmet")
made = sorted(p.name for p in apps.glob("wenuyos-android-*.desktop"))
check(made == ["wenuyos-android-apk.desktop", "wenuyos-android-full.desktop", "wenuyos-android-open.desktop", "wenuyos-android-size.desktop",
               "wenuyos-android-stop.desktop", "wenuyos-android-window.desktop"], "sex genvägar i Start: " + ", ".join(made))
texts = {p.name: p.read_text(encoding="utf-8") for p in apps.glob("wenuyos-android-*.desktop")}
check("Exec=" + str(link) + " apk %f" in texts["wenuyos-android-apk.desktop"], "dubbelklick på en .apk/.xapk installerar i Android")
check(all("@" not in t for t in texts.values()), "inga platshållare kvar")
check(all(f"TryExec={link}" in t and f"Exec={link} " in t for t in texts.values()),
      "TryExec och Exec = kommandot: tas programmet bort försvinner genvägarna ur Start")
check("application/vnd.android.package-archive" in texts["wenuyos-android-open.desktop"] and "NoDisplay=true" in texts["wenuyos-android-open.desktop"],
      "Öppna i Android: .apk och vanliga filer, syns inte i Start")
check(old.read_text().count(f"Exec={link} launch com.exempel.app") == 1, "Android-appar från förut startar genom programmet")
r2 = subprocess.run(["bash", str(SCRIPT), "setup"], env=env, capture_output=True, text=True)
check(r2.returncode == 0 and not list(apps.glob("*.tmp")) and old.read_text().count("launch com.exempel.app") == 1, "setup två gånger: samma resultat")

src = SCRIPT.read_text(encoding="utf-8")
code = "\n".join(l for l in src.splitlines() if not l.lstrip().startswith("#"))
check("/usr/lib/wenuyos/wenuyos-android " not in code.replace("/usr/lib/wenuyos/wenuyos-android launch #Exec", ""),
      "inget i koden pekar på systemets gamla wenuyos-android")
check('ROOT() { sudo -n /usr/lib/wenuyos/wenuyos-android-root "$@"; }' in src, "root-delen är systemets låsta hjälpare (fasta kommandon)")
check('XAPK="$HERE/lib/wenuyos_xapk.py"' in src and 'python3 -I "$XAPK"' in src, "xapk-hjälparen ligger i programmet och körs isolerat")
r = subprocess.run(["bash", "-n", str(SCRIPT)], capture_output=True, text=True)
check(r.returncode == 0, "wenuyos-android: skalsyntax")

# fönstret: logiken utan GTK
loader = importlib.machinery.SourceFileLoader("android_appar", str(ROOT / "main.py"))
spec = importlib.util.spec_from_loader("android_appar", loader)
A = importlib.util.module_from_spec(spec)
os.environ["LANG"] = "sv_SE.UTF-8"
loader.exec_module(A)
A.SV = True
check(A.status_text({})[1:] == (False, False) and "Inte påslaget" in A.status_text({})[0], "inte aktiverat: säger det")
sub, inst, run = A.status_text({"installed": True, "running": True, "memory_mb": 850, "apps": 3, "graphics": "software"})
check(inst and run and sub == "Körs · 850 MB minne · 3 appar · grafik: mjukvara", "körs: " + sub)
sub, _, _ = A.status_text({"installed": True, "running": True, "frozen": True, "memory_mb": 0, "apps": 0})
check(sub.startswith("Pausat") and "MB minne" not in sub, "pausat: " + sub)
check(A.status_text({"installed": True})[0].startswith("Stängt"), "stängt: startar när en app öppnas")
win = (ROOT / "main.py").read_text(encoding="utf-8")
check(win.count("tr(") > 80 and not re.search(r'(title|label|subtitle)="[A-ZÅÄÖ][a-zåäö]', win.replace('title="Android"', "")),
      "texterna i fönstret är på båda språken (tr)")
check("timeout_add_seconds(5" in win and '"unmap"' in win and "source_remove" in win, "statusen läses bara medan fönstret syns (ingen tickande bakgrund)")
check(not re.search(r"curl|wget|urllib|requests", win), "fönstret kopplar inte upp sig självt (Aktivera/appbutiker sker i terminalen, synligt)")
desk = (ROOT / "data/se.wenuyos.app.android.desktop").read_text(encoding="utf-8")
check(all(k in desk for k in ("Name=", "Name[sv]=", "Comment=", "Comment[sv]=", "Icon=se.wenuyos.app.android", "Keywords=", "Keywords[sv]=")),
      "programmets .desktop: namn, beskrivning, ikon och sökord på båda språken")
check((ROOT / "data/se.wenuyos.app.android.svg").is_file(), "ikonen finns")

print(f"\nAndroid-appar som program: {total - len(fails)} av {total} prov godkända")
sys.exit(1 if fails else 0)
