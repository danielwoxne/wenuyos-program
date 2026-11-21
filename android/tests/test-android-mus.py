#!/usr/bin/env python3
"""Android: musen som finger av sig själv (Daniel 16 nov 2026: "Godkänn mus som finger"). Spel som bara tar emot
pekhändelser (Among Us) gick inte att styra på en dator utan pekskärm (Claude på A275, 15 nov). Provar med ett låtsat
waydroid och sudo: auto ger finger i spel bara när datorn saknar pekskärm, valen sparas, och värdet sätts direkt som
användaren när Android kör."""
import os, pathlib, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]   # programmappen android/
SCRIPT = ROOT / "bin/wenuyos-android"
fails, total = [], 0


def check(ok, what):
    global total
    total += 1
    print(("  ok   " if ok else "  FEL  ") + what)
    if not ok:
        fails.append(what)


tmp = pathlib.Path(tempfile.mkdtemp(prefix="test-android-mus-"))
home, conf, binp, run = tmp / "home", tmp / "home/.config/wenuyos", tmp / "bin", tmp / "run"
for d in (conf, binp, run):
    d.mkdir(parents=True)
log = tmp / "calls.log"
state = tmp / "running"
for name, body in {
        "waydroid": 'echo "waydroid $*" >> "$LOG"\n[ "$1" = status ] && [ -f "$STATE" ] && echo "Session:	RUNNING"\nexit 0',
        "sudo": 'echo "sudo $*" >> "$LOG"\nexit 0', "notify-send": "exit 0"}.items():
    (binp / name).write_text("#!/bin/sh\n" + body + "\n")
    (binp / name).chmod(0o755)
env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / ".config"), XDG_RUNTIME_DIR=str(run), LANG="sv_SE.UTF-8",
           PATH=str(binp) + os.pathsep + os.environ["PATH"], LOG=str(log), STATE=str(state))


def cmd(*a, touch=None):
    e = dict(env)
    if touch is not None:
        e["WENUYOS_HAS_TOUCH"] = "1" if touch else "0"
    r = subprocess.run(["bash", str(SCRIPT), *a], env=e, capture_output=True, text=True, timeout=30)
    return r.returncode, r.stdout.strip()


def calls():
    t = log.read_text() if log.exists() else ""
    log.write_text("")
    return t


check(cmd("touch", "mode") == (0, "auto"), "inget valt: auto (standard)")
check(cmd("touch", "list", touch=False) == (0, "com.innersloth.spacemafia"), "auto utan pekskärm: Among Us har musen som finger")
check(cmd("touch", "list", touch=True)[1].startswith("ingen"), "auto med pekskärm: musen är en mus")
(conf / "android-games").write_text("org.exempel.spel\ncom.innersloth.spacemafia\nfel paket;rm\n")
check(cmd("touch", "list", touch=False) == (0, "com.innersloth.spacemafia,org.exempel.spel"), "auto: appar som visas som spel räknas, en gång var, konstiga namn hoppas över")
check(cmd("touch", "none")[0] == 0 and cmd("touch", "mode") == (0, "none") and cmd("touch", "list", touch=False)[1].startswith("ingen"),
      "none: musen är en mus också utan pekskärm (valet sparas)")
(conf / "android-touch").write_text("\n")
check(cmd("touch", "mode") == (0, "none"), "äldre WenuyOS (tom fil = ingen) läses som none")
check(cmd("touch", "all")[0] == 0 and cmd("touch", "list") == (0, "*"), "all: finger i alla appar")
cmd("touch", "none")
check(cmd("touch", "add", "org.a.b")[0] == 0 and cmd("touch", "mode") == (0, "some") and cmd("touch", "list") == (0, "org.a.b"), "add: en vald app")
check(cmd("touch", "add", "x;rm -rf ~")[0] != 0 and cmd("touch", "list") == (0, "org.a.b"), "ett konstigt paketnamn sparas inte")
check(cmd("touch", "remove", "org.a.b")[0] == 0 and cmd("touch", "mode") == (0, "none"), "remove den sista: none (inte tillbaka till auto)")
check(cmd("touch", "auto")[0] == 0 and cmd("touch", "mode") == (0, "auto") and not (conf / "android-touch").exists(), "auto: tillbaka till standard")
check(cmd("touch", "kanske")[0] != 0, "okänt val: fel")

# värdet når Android: rotens filer, och direkt som användaren när Android kör
src = SCRIPT.read_text(encoding="utf-8")
ap = src[src.index("apply_touch() {"):src.index("\n}\n", src.index("apply_touch() {"))]
check('ROOT touch "${t:-none}"' in ap and 'waydroid prop set persist.waydroid.fake_touch "$t"' in ap and "RUNNING" in ap,
      "apply_touch: rotens filer (när Android startar) och waydroid prop set som användaren när Android kör")
check(src.index("apply_touch   # musen som finger gäller direkt") > src.index('session_up || { note "$(app_name "$pkg")"'), "launch: sätts när appen startas")
state.touch()
calls()
env["WENUYOS_HAS_TOUCH"] = "0"
(tmp / "var").mkdir()
r = subprocess.run(["bash", "-c", f'source <(sed -n "/^TOUCH_BUILTIN=/,/^apply_touch() {{/p" "{SCRIPT}" | head -n -1); '
                    f'CONF="{conf}"; cfg() {{ cat "$CONF/android-$1" 2>/dev/null; }}; qgrep() {{ grep "$@" >/dev/null; }}; '
                    f'ROOT() {{ echo "ROOT $*" >> "$LOG"; }}; '
                    + src[src.index("apply_touch() {"):src.index("\n}\n", src.index("apply_touch() {")) + 3] + "apply_touch"],
                   env=env, capture_output=True, text=True)
c = calls()
check(r.returncode == 0 and "ROOT touch com.innersloth.spacemafia,org.exempel.spel" in c and
      "waydroid prop set persist.waydroid.fake_touch com.innersloth.spacemafia,org.exempel.spel" in c, "Android kör: värdet sätts direkt (provat med låtsat waydroid)")
check("sudo waydroid" not in c, "… inte som root")
st = (ROOT / "main.py").read_text(encoding="utf-8")
check('run(["touch", "mode"])' in st and 'tr("Automatic (finger in games when the computer has no touch screen)",\n                                     "Automatiskt (finger i spel när datorn saknar pekskärm)")' in st,
      "fönstret Android-appar: Automatiskt finns och visar det valda")

print(f"Android mus som finger: {total - len(fails)} av {total} prov godkända")
sys.exit(1 if fails else 0)
