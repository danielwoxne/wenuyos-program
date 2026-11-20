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

# ---- upptäckten (ChatGPT 20 nov: den första versionen godkände efter ~6 s, före två byten). bw_step med påhittade
# tidsserier: ett varv var 0,33 s, processnumret läses var 6:e varv (~2 s), Androids tjänst svarar och säger
# boot_completed=1 efter 3 s.
bw = SRC[SRC.index("SF_STABLE=${WENUYOS_ANDROID_SF_STABLE:-12}"):SRC.index("boot_wait() {")]


def judge(pid_at, booted_from=3.0, seconds=60, alive_only=False):
    """pid_at(t) → processnummer ("" = går inte att läsa). → (svar, sekund) för första svaret som inte är 1."""
    lines = []
    for i in range(1, int(seconds / 0.33) + 1):
        t = i * 0.33
        p = pid_at(t) if i % 6 == 0 else "-"
        out, alive = ("1", 1) if t >= booted_from else ("", 0)   # före starten svarar Androids tjänst inte
        if alive_only: out, alive = "", 1   # tjänsten svarar hela tiden, men egenskapen går aldrig att läsa
        lines.append(f'rc=0; bw_step {int(t)} "{out}" {alive} "{p}" || rc=$?; [ $rc != 1 ] && {{ echo "$rc {t:.1f}"; exit; }}')
    script = "set -u\n" + bw + "\nbw_reset\n" + "\n".join(lines) + "\necho '1 -'\n"
    r = subprocess.run(["bash", "-c", script], capture_output=True, text=True, timeout=30)
    rc, t = r.stdout.split()
    return int(rc), (None if t == "-" else float(t))


for period in (4, 5, 6, 7):
    rc, t = judge(lambda t, period=period: str(1000 + int(t // period)))
    check(rc == 2 and t is not None and t <= 3 * period + 2.5,
          f"grafiken kraschar var {period}:e sekund (boot_completed=1 ändå): kraschslinga efter {t} s – aldrig godkänd först")
rc, t = judge(lambda t: str(1000 + int(t // 5)), alive_only=True)
check(rc == 2, f"A275-fallet: tjänsten svarar men egenskapen går inte att läsa, grafiken kraschar var 5:e s → kraschslinga ({t} s)")
rc, t = judge(lambda t: "4242")
check(rc == 0 and 12 <= t <= 16, f"frisk grafik: startat när processnumret stått still i 12 s ({t} s)")
rc, t = judge(lambda t: "4242", booted_from=30)
check(rc == 0 and 30 <= t <= 31, f"Android startar långsamt (30 s): startat när det säger det, grafiken har redan stått still ({t} s)")
rc, t = judge(lambda t: "4242" if t < 20 else "5151")
check(rc == 0 and t is not None and t < 20, "en enda omstart efter att Android redan räknats som startat ändrar inget")
rc, t = judge(lambda t: "100" if t < 8 else "200")
check(rc == 0 and t >= 20, f"en omstart under starten: väntar 12 s till med det nya numret ({t} s), ingen kraschslinga")
rc, t = judge(lambda t: "")
check(rc == 0 and 6 <= t <= 8, f"processnumret går inte att läsa (äldre root-hjälpare): som förut efter ~6 s ({t} s)")
rc, t = judge(lambda t: "4242", booted_from=999)
check(rc == 1, "Android blir aldrig klart: inget svar (som förut)")
check("printf '%(%s)T' -1" in SRC and 'bw_step "$(bw_now)"' in SRC, "boot_wait räknar i riktiga sekunder (inte varv)")

# ---- bytet till mjukvarugrafik: varje steg måste lyckas (ChatGPT 20 nov)
def fallback(fail):
    log.write_text("")
    (tmp / "android-graphics").unlink(missing_ok=True)
    script = f'''set -uo pipefail
RUN="{tmp}/run"; LOG="{log}"; CONF="{tmp}"; mkdir -p "$RUN"; : > "$RUN/android-booted"
cfg() {{ cat "$CONF/android-$1" 2>/dev/null; }}; setcfg() {{ echo "$2" > "$CONF/android-$1"; }}
T() {{ echo "$2"; }}; note() {{ echo "note $*" >> "$LOG"; }}; gfx_now() {{ echo hardware; }}
waydroid() {{ return 0; }}
ROOT() {{ echo "ROOT $*" >> "$LOG"; [ "$1" = "{fail}" ] && return 1; return 0; }}
{fb}
gfx_fallback; echo rc=$?
'''
    r = subprocess.run(["bash", "-c", script], capture_output=True, text=True, timeout=30)
    saved = (tmp / "android-graphics").read_text().strip() if (tmp / "android-graphics").exists() else ""
    return r.stdout.strip(), log.read_text(), saved, (tmp / "run/android-booted").exists()
for step in ("gfx", "restart"):
    out, calls, saved, booted = fallback(step)
    check(out == "rc=3" and saved == "" and "Kunde inte byta Android till mjukvarugrafik" in calls and booted,
          f"ROOT {step} misslyckas: inget sparas, användaren får veta det")
out, calls, saved, booted = fallback("inget")
check(out == "rc=0" and saved == "software" and not booted, "allt lyckas: mjukvarugrafik sparas")


# root-hjälpens sf-pid provas i New-wenuyos (tools/check/test-android-root.py)
r = subprocess.run(["bash", "-n", str(ROOT / "bin/wenuyos-android")], capture_output=True, text=True)
check(r.returncode == 0, "wenuyos-android: skalsyntax")

print(f"\nAndroid grafik i kraschslinga: {total - len(fails)} av {total} prov godkända")
sys.exit(1 if fails else 0)
