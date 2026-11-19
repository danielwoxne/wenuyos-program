#!/usr/bin/env python3
"""Android: grafiken kraschar i en slinga → mjukvarugrafik i stället (Claude på ThinkPaden 19 nov 2026).
15 och 16 nov på A275 startade surfaceflinger om var 5:e sekund i flera minuter, men Androids tjänst svarade ändå –
starten räknades som lyckad och reservläget med mjukvarugrafik slog aldrig till. Provar session_up med låtsade
hjälpare (ingen Android körs här): boot_wait säger 2 (kraschslinga) → mjukvarugrafik, valet sparas, en ny start."""
import pathlib, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]   # programmappen android/
SRC = (ROOT / "bin/wenuyos-android").read_text()
fails, total = [], 0


def check(ok, what):
    global total
    total += 1
    print(("  ok   " if ok else "  FEL  ") + what)
    if not ok:
        fails.append(what)


fb = SRC[SRC.index("gfx_fallback() {"):SRC.index("session_up() {")]
su = SRC[SRC.index("session_up() {"):SRC.index("# Android-appar i menyn startar via")]
tmp = pathlib.Path(tempfile.mkdtemp(prefix="test-android-grafik-"))
log = tmp / "calls.log"


def run(boot_rcs, graphics="", now="hardware"):
    """boot_rcs: vad boot_wait svarar, i tur och ordning (t.ex. "2 0")."""
    log.write_text("")
    (tmp / "android-graphics").unlink(missing_ok=True)
    if graphics:
        (tmp / "android-graphics").write_text(graphics + "\n")
    (tmp / "bw").write_text(boot_rcs)
    script = f'''set -uo pipefail
RUN="{tmp}/run"; SHARE="{tmp}/Android"; HOME="{tmp}"; LOG="{log}"; CONF="{tmp}"; mkdir -p "$RUN"
qgrep() {{ grep "$@" >/dev/null; }}
cfg() {{ cat "$CONF/android-$1" 2>/dev/null; }}; setcfg() {{ echo "$2" > "$CONF/android-$1"; }}
T() {{ echo "$2"; }}; note() {{ echo "note $*" >> "$LOG"; }}
busy_mark() {{ :; }}; prepare() {{ :; }}; gfx_now() {{ echo {now}; }}
heal_size() {{ return 1; }}; start_wait() {{ echo start >> "$LOG"; return 0; }}
boot_wait() {{ read -r rc rest < "{tmp}/bw"; echo "$rest" > "{tmp}/bw"; echo "boot_wait $rc" >> "$LOG"; return "${{rc:-0}}"; }}
ROOT() {{ echo "ROOT $*" >> "$LOG"; return 0; }}
waydroid() {{ [ "$1" = status ] && echo "Session:	RUNNING"; [ "$1" = session ] && echo "waydroid $*" >> "$LOG"; return 0; }}
{fb}
{su}
session_up; echo rc=$?
'''
    r = subprocess.run(["bash", "-c", script], capture_output=True, text=True, timeout=30)
    saved = (tmp / "android-graphics").read_text().strip() if (tmp / "android-graphics").exists() else ""
    return r.stdout.strip(), log.read_text().split("\n"), saved


out, calls, saved = run("2 0")
check("rc=0" in out and "ROOT gfx software" in calls and "ROOT restart" in calls,
      "kraschslinga i grafiken (automatiskt läge): byter till mjukvarugrafik och startar om containern")
check(saved == "software", "valet sparas, så att kraschslingan inte kommer tillbaka vid nästa start")
check(calls.count("start") == 1 and calls.index("ROOT gfx software") < calls.index("start") and calls.count("boot_wait 2") == 1
      and "boot_wait 0" in calls, "startar Android igen efter bytet och väntar på det en gång till")
check(any(c.startswith("note Android Startar med mjukvarugrafik") for c in calls), "säger till på svenska att mjukvarugrafik används")

out, calls, saved = run("2 2")
check("rc=0" in out and calls.count("ROOT gfx software") == 1, "kraschar också med mjukvarugrafik: byter bara en gång, ingen slinga")

out, calls, saved = run("2", graphics="hardware")
check("ROOT gfx software" not in calls and saved == "hardware", "användaren har valt grafikkortet (hardware): rörs inte")
check(any("wenuyos-android graphics software" in c for c in calls), "… men får veta hur mjukvarugrafik slås på")

out, calls, saved = run("2", now="software")
check("ROOT gfx software" not in calls, "redan mjukvarugrafik: inget att byta till")

out, calls, saved = run("1")
check("rc=0" in out and "ROOT gfx software" not in calls, "Android svarar bara inte i tid (1): som förut, ingen grafikändring")

out, calls, saved = run("0")
check("rc=0" in out and "ROOT gfx software" not in calls and calls.count("boot_wait 0") == 1, "allt fungerar: inget händer")

# root-hjälpens sf-pid provas i New-wenuyos (tools/check/test-android-root.py)
r = subprocess.run(["bash", "-n", str(ROOT / "bin/wenuyos-android")], capture_output=True, text=True)
check(r.returncode == 0, "wenuyos-android: skalsyntax")

print(f"\nAndroid grafik i kraschslinga: {total - len(fails)} av {total} prov godkända")
sys.exit(1 if fails else 0)
