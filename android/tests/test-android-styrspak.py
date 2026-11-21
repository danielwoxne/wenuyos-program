#!/usr/bin/env python3
"""Styrspakar och handkontroller i Android (Claude på ThinkPaden 15 nov, meddelande 2332: THE C64-styrspaken syntes i
Android först med persist.waydroid.uevent=true, och bara om den kopplades in efter att Android startat).
Provar utan Android: uevent är förvalt i root-hjälpens grundinställningar, och när Android startas får styrspakar som
redan sitter i ett nytt "add" – en gång, efter att Android startat färdigt, och bara när Android just startades."""
import os, pathlib, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]   # programmappen android/
SRC = (ROOT / "bin/wenuyos-android").read_text(encoding="utf-8")
fails, total = [], 0


def check(ok, what):
    global total
    total += 1
    print(("  ok   " if ok else "  FEL  ") + what)
    if not ok:
        fails.append(what)


# root-hjälpen (base, joysticks) provas i New-wenuyos (tools/check/test-android-root.py)

# ---- session_up: med låtsade hjälpare
su = SRC[SRC.index("session_up() {"):SRC.index("# Android-appar i menyn startar via")]
tmp = pathlib.Path(tempfile.mkdtemp(prefix="test-styrspak-"))
log = tmp / "calls.log"


def run(running):
    log.write_text("")
    script = f'''set -uo pipefail
RUN="{tmp}/run"; SHARE="{tmp}/Android"; HOME="{tmp}"; LOG="{log}"
qgrep() {{ grep "$@" >/dev/null; }}
busy_mark() {{ :; }}; prepare() {{ echo prepare >> "$LOG"; }}; cfg() {{ echo; }}; gfx_now() {{ echo hardware; }}
heal_size() {{ return 1; }}; start_wait() {{ echo start >> "$LOG"; return 0; }}
boot_wait() {{ echo boot_wait >> "$LOG"; return 0; }}
ROOT() {{ echo "ROOT $*" >> "$LOG"; return 0; }}
waydroid() {{ [ "$1" = status ] && {{ [ "{running}" = yes ] && echo "Session:	RUNNING" || echo "Session:	STOPPED"; }}; return 0; }}
{su}
session_up; echo rc=$?
'''
    r = subprocess.run(["bash", "-c", script], capture_output=True, text=True, timeout=30)
    return r.stdout.strip(), log.read_text().split("\n")


out, calls = run("no")
check("rc=0" in out and "ROOT joysticks" in calls, "Android startas: styrspakar som redan sitter i skickas in")
check(calls.count("ROOT joysticks") == 1 and calls.index("boot_wait") < calls.index("ROOT joysticks"),
      "en gång, och först när Android har startat färdigt (annars lyssnar ingen)")
out, calls = run("yes")
check("rc=0" in out and "ROOT joysticks" not in calls, "Android kör redan: inget nytt add (de är redan inne)")

print()
print(f"Styrspakar i Android: {total - len(fails)} av {total} godkända")
sys.exit(1 if fails else 0)
