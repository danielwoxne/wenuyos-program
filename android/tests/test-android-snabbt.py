#!/usr/bin/env python3
"""Android-appar som vanliga program (Daniel 15 nov 2026: "Jag vill att det ska gå snabbt och se ut som ett vanligt
program eller spel"). Provar med ett låtsat waydroid (ingen Android körs här): Android förbereds bara på datorer med
minne nog och bara om Android-appar används, stängs inte medan det hålls förberett (utom när minnet tar slut),
appar kan visas som spel, och snurran i dockan väntar på en Android-app."""
import os, pathlib, re, subprocess, sys, tempfile, time
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

ROOT = pathlib.Path(__file__).resolve().parents[1]   # programmappen android/
SCRIPT = ROOT / "bin/wenuyos-android"
fails, total = [], 0


def check(ok, what):
    global total
    total += 1
    print(("  ok   " if ok else "  FEL  ") + what)
    if not ok:
        fails.append(what)


tmp = pathlib.Path(tempfile.mkdtemp(prefix="test-android-"))
home, conf, binp, run = tmp / "home", tmp / "home/.config/wenuyos", tmp / "bin", tmp / "run"
for d in (conf, binp, run, home / ".local/share/applications"):
    d.mkdir(parents=True)
meminfo = tmp / "meminfo"
for name, body in {"waydroid": 'echo "waydroid $*" >> "$LOG"\nexit 0', "sudo": 'echo "sudo $*" >> "$LOG"\nexit 0', "notify-send": "exit 0"}.items():
    (binp / name).write_text("#!/bin/sh\n" + body + "\n")
    (binp / name).chmod(0o755)
log = tmp / "calls.log"
env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / ".config"), XDG_RUNTIME_DIR=str(run), LANG="sv_SE.UTF-8",
           PATH=str(binp) + os.pathsep + os.environ["PATH"], WENUYOS_MEMINFO=str(meminfo), LOG=str(log))


def mem(total_kb, avail_kb):
    meminfo.write_text(f"MemTotal:       {total_kb} kB\nMemFree:         1000 kB\nMemAvailable:   {avail_kb} kB\n")


def cmd(*a):
    r = subprocess.run(["bash", str(SCRIPT), *a], env=env, capture_output=True, text=True, timeout=30)
    return r.returncode, r.stdout.strip()


used = conf / "android-last-used"
# auto: minne nog OCH Android-appar används
mem(7_900_000, 5_000_000)
check(cmd("prestart") == (0, "no"), "auto: Android-appar har inte använts → förbereds inte")
used.touch()
check(cmd("prestart") == (0, "yes"), "auto: 8 GB minne och en app använd nyligen → förbereds")
mem(3_800_000, 2_000_000)
check(cmd("prestart") == (0, "yes"), "auto: en dator med 4 GB (3,8 GB synligt) räknas")
mem(1_900_000, 900_000)
check(cmd("prestart") == (0, "no"), "auto: 2 GB-datorn förbereder aldrig Android (2 GB-golvet)")
old = time.time() - 20 * 86400
os.utime(used, (old, old))
mem(7_900_000, 5_000_000)
check(cmd("prestart") == (0, "no"), "auto: ingen Android-app på 20 dagar → förbereds inte längre")
check(cmd("prestart", "on")[0] == 0 and cmd("prestart") == (0, "yes"), "prestart on: alltid")
check(cmd("prestart", "off")[0] == 0 and cmd("prestart") == (0, "no"), "prestart off: aldrig")
check(cmd("prestart", "kanske")[0] != 0 and (conf / "android-prestart").read_text().strip() == "off", "ett okänt värde ändrar ingenting")
cmd("prestart", "auto")

# stängs inte medan det hålls förberett – det användaren valt går före
src = SCRIPT.read_text(encoding="utf-8")
fn = src[src.index("MEMINFO=${WENUYOS_MEMINFO"):src.index("app_name() {")]
def sh(expr):
    pre = 'CONF="$XDG_CONFIG_HOME/wenuyos"; cfg() { cat "$CONF/android-$1" 2>/dev/null; }\n' + fn
    r = subprocess.run(["bash", "-c", pre + "\n" + expr], env=env, capture_output=True, text=True)
    return r.returncode, r.stdout.strip()
os.utime(used, None)
check(sh("autostop_minutes") == (0, "never"), "förberett Android stängs inte efter 10 minuter (det pausade Android är det som gör starten snabb)")
cmd("prestart", "off")
check(sh("autostop_minutes") == (0, "10"), "utan förberedelse: som förut, 10 minuter")
cmd("prestart", "on"); cmd("autostop", "30")
check(sh("autostop_minutes") == (0, "30"), "det användaren valt (30 minuter) går före")
check(cmd("autostop", "auto")[0] == 0 and not (conf / "android-autostop").exists() and sh("autostop_minutes") == (0, "never"), "autostop auto: tillbaka till automatiskt")
mem(7_900_000, 5_000_000)
check(sh("memory_low")[0] != 0, "gott om minne: inte ont om minne")
mem(7_900_000, 700_000)
check(sh("memory_low")[0] == 0, "mindre än en åttondel ledigt: ont om minne")
w = src[src.index("  watch)"):]
check("prestart_wanted && ! running && ! memory_low" in w and "nice -n 10" in w and 'prestarted=yes' in w, "bevakningen förbereder en gång per inloggning, med låg prioritet, inte när minnet är slut")
check('[ -z "$(cfg autostop)" ] && [ $idle -ge 300 ] && memory_low' in w and "stop_android" in w, "förberett men ont om minne: Android stängs (startar igen när en app öppnas)")
check("sleep 20" in w and w.count("while ") == 1, "ingen ny bakgrundstjänst: samma bevakning som förut (var 20:e sekund)")
check("sleep 0.33" in src[src.index("start_wait() {"):src.index("# ---- Snabb start")], "starten upptäcks inom en tredjedels sekund (var en sekund)")
la = src[src.index("  launch)"):src.index("  open)")]
check('touch "$CONF/android-last-used"' in la and 'note "$(app_name "$pkg")"' in la, "en startad app räknas som använd; ett fel nämner appens namn, inte bara Android")

# ---- fryses inte mitt i (Claude på ThinkPaden 15 nov: .xapk-installationen och `full` frystes under uppstarten)
bw = src[src.index("busy_mark() {"):src.index("session_up() {")]
def sh2(expr, **extra):
    pre = ('RUN="$XDG_RUNTIME_DIR/wenuyos"; mkdir -p "$RUN"; note() { echo "NOTE $*"; }; T() { echo "$2"; }\n' + bw)
    r = subprocess.run(["bash", "-c", pre + "\n" + expr], env=dict(env, **extra), capture_output=True, text=True, timeout=60)
    return r.returncode, r.stdout.strip()
rund = run / "wenuyos"
check(sh2("busy_now")[0] != 0, "ingen använder Android: inte upptaget")
check(sh2('busy_mark; busy_now && ls "$RUN" | grep -c android-busy') == (0, "1"), "den som använder Android märks som upptagen")
check(not list(rund.glob("android-busy.*")), "märket tas bort när processen slutar")
(rund / "android-busy.999999999").touch(); (rund / "android-busy.skrap").touch()
check(sh2("busy_now")[0] != 0 and not list(rund.glob("android-busy.*")), "märket efter en process som inte finns längre städas bort (och räknas inte)")
# färdigstartat: väntar på sys.boot_completed
(binp / "waydroid").write_text('#!/bin/sh\necho "waydroid $*" >> "$LOG"\nif [ "$1 $2" = "prop get" ]; then n=$(cat "$CNT" 2>/dev/null || echo 0); n=$((n+1)); echo $n > "$CNT"\n'
                               '  if [ "$n" -lt "${READY_AT:-3}" ]; then echo "Failed to get service waydroidplatform"; exit ${FAILRC:-0}; fi; echo "${PROPVAL-1}"; fi\nexit 0\n')
cnt = tmp / "cnt"
def boot(**extra):
    cnt.write_text("0"); (rund / "android-booted").unlink(missing_ok=True)
    rc, out = sh2("boot_wait; echo rc=$?", CNT=str(cnt), **extra)
    return out, int(cnt.read_text()), (rund / "android-booted").exists()
out, n, booted = boot(READY_AT="4")
check("rc=0" in out and n == 4 and booted, "väntar tills Android säger att det har startat (sys.boot_completed = 1)")
out, n, booted = boot(READY_AT="1")
check("rc=0" in out and n == 1 and "NOTE" not in out, "redan startat: ingen väntan och inget meddelande")
rc, out = sh2("boot_wait; echo rc=$?", CNT=str(cnt))
check("rc=0" in out and int(cnt.read_text()) == 1, "färdigstartat minns till nästa gång (ingen ny fråga)")
out, n, booted = boot(READY_AT="1", PROPVAL="")
check("rc=0" in out and n == 18 and booted,
      "egenskapen går inte att läsa men Androids tjänst svarar: klart efter 15 svar i rad, när grafiken kontrollerats tre gånger")
# kraschslinga i grafiken (Claude på ThinkPaden 19 nov: surfaceflinger startade om var 5:e s i 5 min, 15 nov på A275 –
# tjänsten svarade ändå, så starten räknades som lyckad och mjukvarugrafiken provades aldrig)
sfc = tmp / "sfcnt"
def boot_sf(sf, **extra):
    sfc.write_text("0")
    rootf = 'ROOT() { [ "$1" = sf-pid ] || return 0; n=$(cat "$SFC"); n=$((n+1)); echo $n > "$SFC"; ' + sf + '; }\n'
    cnt.write_text("0"); (rund / "android-booted").unlink(missing_ok=True)
    rc, out = sh2(rootf + "boot_wait; echo rc=$?", CNT=str(cnt), SFC=str(sfc), **extra)
    return out, (rund / "android-booted").exists()
out, booted = boot_sf('echo $((1000 + n))', READY_AT="1", PROPVAL="")
check("rc=2" in out and not booted, "grafiken får nytt processnummer gång på gång: kraschslinga (2), inte färdigstartat")
out, booted = boot_sf('echo 1234', READY_AT="1", PROPVAL="")
check("rc=0" in out and booted, "samma processnummer hela tiden: grafiken håller, klart som förut")
out, booted = boot_sf('[ "$n" = 1 ] && echo 1234 || echo', READY_AT="1", PROPVAL="")
check("rc=0" in out and booted, "processnumret går inte att läsa (tomt): ingen falsk kraschslinga")
out, booted = boot_sf('echo $((1000 + n))', READY_AT="4")
check("rc=0" in out and booted, "Android säger själv att det har startat (sys.boot_completed = 1): klart, även före grafikkollen")
out, n, booted = boot(READY_AT="999", WENUYOS_ANDROID_BOOT_WAIT="20")
check("rc=1" in out and not booted and "NOTE Android Startar" in out, "Android svarar aldrig: ger upp, säger till efter 5 s, och märks inte som färdigstartat")
out, n, booted = boot(READY_AT="999", WENUYOS_ANDROID_BOOT_WAIT="20", WENUYOS_ANDROID_QUIET="1")
check("NOTE" not in out, "förberedelsen i bakgrunden säger ingenting")
su = src[src.index("session_up() {"):src.index("# Android-appar i menyn startar via")]
check(su.index("busy_mark") < su.index("ROOT unfreeze") and "boot_wait || true" in su and 'rm -f "$RUN/android-booted"' in su,
      "session_up: märker upptaget först, och väntar på att Android startat färdigt innan en installation eller app fortsätter")
check('busy_now || pgrep -u "$(id -u)" -f "waydroid.* (app launch|app install|show-full-ui)"' in w and "idle=0; continue" in w[w.index("busy_now ||"):][:400],
      "bevakningen: upptaget → väck om det är pausat, räkna om från noll, pausa och stäng inte")
check('[ ! -f "$RUN/android-frozen" ] && [ -f "$RUN/android-booted" ]' in w, "bevakningen pausar bara ett Android som har startat färdigt")
check("WENUYOS_ANDROID_QUIET=1 session_up" in src, "förberedelsen väntar också på att Android startat färdigt innan det får pausas")

# ---- 0×0 och genomskinliga spel (Claude på ThinkPaden 15 nov, 23:08 och 23:12)
ws = src[src.index("window_size() {"):src.index("auto_density() {")]
check('full) echo "$sw" "$sh";;' in ws and "echo 0 0" not in src, "helskärm = skärmens storlek, aldrig 0 0 (Android kraschade om och om igen)")
hz = src[src.index("heal_size() {"):src.index("ensure_window_size() {")]
def heal(got):
    pre = ('RUN="$XDG_RUNTIME_DIR/wenuyos"; note() { echo "NOTE $*"; }; T() { echo "$2"; }; window_size() { echo 1280 614; }\n'
           'ROOT() { echo "ROOT $*" >> "$LOG"; [ "$1" = size-get ] && echo "' + got + '"; return 0; }\nwaydroid() { echo "waydroid $*" >> "$LOG"; }\n' + hz)
    log.write_text("")
    r = subprocess.run(["bash", "-c", pre + "\nheal_size; echo rc=$?"], env=env, capture_output=True, text=True)
    return r.stdout, log.read_text()
out, calls = heal("0 0")
check("rc=0" in out and "ROOT size 1280 614" in calls and "waydroid session stop" in calls and "NOTE Android" in out, "sparad storlek 0×0: rättas till den valda storleken, Android startas om, och det sägs till")
out, calls = heal("1280 0")
check("rc=0" in out and "ROOT size 1280 614" in calls, "bara höjden 0: rättas också")
out, calls = heal("1280 614")
check("rc=1" in out and "ROOT size 1" not in calls and "session stop" not in calls, "en riktig storlek rörs inte")
out, calls = heal(" ")
check("rc=1" in out and "ROOT size 1" not in calls, "går storleken inte att läsa: ingenting ändras")
su2 = src[src.index("session_up() {"):src.index("# Android-appar i menyn startar via")]
check("if heal_size; then start_wait || up=no; fi" in su2 and '[ ! -f "$RUN/android-booted" ] && heal_size' in su2, "session_up rättar 0×0 både vid start och när Android redan kör utan att bli klart")
# appar som öppnas i Android-fönstret
check(cmd("fullui", "list") == (0, "com.innersloth.spacemafia"), "Among Us öppnas i Android-fönstret från början (delvis genomskinligt i eget fönster)")
check(cmd("fullui", "add", "com.exempel.unity")[0] == 0 and cmd("fullui", "list")[1].split() == ["com.exempel.unity", "com.innersloth.spacemafia"], "fullui add: en egen app")
cmd("fullui", "add", "com.exempel.unity")
check((conf / "android-fullui").read_text().count("com.exempel.unity") == 1, "fullui add två gånger: en rad")
check(cmd("fullui", "remove", "com.innersloth.spacemafia")[0] == 0 and cmd("fullui", "list") == (0, "com.exempel.unity"), "fullui remove: också en inbyggd app går att ta bort")
check(cmd("fullui", "add", "com.innersloth.spacemafia")[0] == 0 and "com.innersloth.spacemafia" in cmd("fullui", "list")[1] and "-com.innersloth" not in (conf / "android-fullui").read_text(),
      "… och lägga tillbaka")
cmd("fullui", "remove", "com.exempel.unity")
la2 = src[src.index("  launch)"):src.index("  fullui)")]
check(la2.index('ensure_window_size "$d"') < la2.index("session_up") < la2.index("waydroid show-full-ui") and '[ "$d" = full ] && d=large' in la2,
      "launch: en sådan app startas och visas i Android-fönstret (vald storlek, inte helskärm)")
check(la2.rstrip().endswith('exec waydroid app launch "$pkg";;'), "andra appar öppnas som förut i eget fönster")

# spel: en Android-app i Spel-biblioteket
d = home / ".local/share/applications/waydroid.com.exempel.spel.desktop"
d.write_text("[Desktop Entry]\nType=Application\nName=Exempelspel\nExec=waydroid app launch com.exempel.spel\nIcon=/x.png\nCategories=X-WayDroid-App;\n")
(home / ".local/share/applications/waydroid.com.exempel.app.desktop").write_text(
    "[Desktop Entry]\nType=Application\nName=Exempelapp\nExec=waydroid app launch com.exempel.app\nCategories=X-WayDroid-App;\n")
check(cmd("game", "add", "com.exempel.spel")[0] == 0 and "Categories=Game;X-WayDroid-App;" in d.read_text()
      and "wenuyos-android launch com.exempel.spel" in d.read_text() and "/usr/lib/wenuyos/wenuyos-android " not in d.read_text(), "game add: appen blir ett spel (Categories=Game) och startar genom WenuyOS")
other = (home / ".local/share/applications/waydroid.com.exempel.app.desktop").read_text()
check("Game" not in other and "wenuyos-android launch com.exempel.app" in other, "andra appar är oförändrade (men startar genom WenuyOS)")
cmd("game", "add", "com.exempel.spel")
check(cmd("game", "list") == (0, "com.exempel.spel") and d.read_text().count("Game;") == 1, "game add två gånger: en rad, en kategori")
check(cmd("game", "remove", "com.exempel.spel")[0] == 0 and "Game" not in d.read_text() and cmd("game", "list") == (0, ""), "game remove: tillbaka till ett vanligt program")
check(cmd("game", "remove", "finns.inte")[0] == 0, "game remove på något som inte finns: inget fel")

# dockan: snurran väntar på en Android-app, och appens fönster hör ihop med appens ikon
sett = (ROOT / "main.py").read_text(encoding="utf-8")
check('tr("Fast start", "Snabb start")' in sett and 'self.later(["prestart", v])' in sett and '("auto", tr("Automatic (after 10 minutes' in sett,
      "fönstret Android-appar: Snabb start, och Automatiskt för när Android stängs – båda språken")
check(not re.search(r"curl|wget|http://|https://", fn), "inget nätverk i det nya")

# ---- bara spelet i Android-fönstret (Daniel 16 nov: "Kan vi få bort bakgrundsemuleringen och bara emulera spelet")
src_a = SCRIPT.read_text(encoding="utf-8")
la2 = src_a[src_a.index("  launch)"):src_a.index("  fullui)")]
fu = la2[la2.index('if is_fullui "$pkg"; then', la2.index("apply_touch")):]
check(fu.index("waydroid show-full-ui") < fu.index('ROOT fullapp "$pkg"'), "Android-fönstret öppnas först, spelet läggs ovanpå i helskärm")
check('|| waydroid app launch "$pkg"' in fu, "går helskärmsläget inte: som förut (appen startas ändå)")
# rotens fullapp provas i New-wenuyos (tools/check/test-android-root.py)
mem(7_900_000, 5_000_000)
check(cmd("display")[0] == 0 and cmd("display")[1].splitlines()[0] in ("auto", "large", "medium", "phone", "full"), "display utan värde: visar det valda")

print(f"\nAndroid som vanliga program: {total - len(fails)} av {total} prov godkända")
sys.exit(1 if fails else 0)
