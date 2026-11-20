#!/usr/bin/env python3
"""Klockan: logiken (klocka.py) och programmets filer. Fönstret provas för sig (tests/README i PR:et: labwc, low/soft)."""
import json, os, pathlib, random, re, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import klocka as K

fails, total = [], 0


def check(ok, what):
    global total
    total += 1
    print(("  ok   " if ok else "  FEL  ") + what)
    if not ok:
        fails.append(what)


# ---- tiden i ord, svenska
sv = {(3, 0): "tre", (3, 5): "fem över tre", (3, 10): "tio över tre", (3, 15): "kvart över tre", (3, 20): "tjugo över tre",
      (3, 25): "fem i halv fyra", (3, 30): "halv fyra", (3, 35): "fem över halv fyra", (3, 40): "tjugo i fyra",
      (3, 45): "kvart i fyra", (3, 50): "tio i fyra", (3, 55): "fem i fyra", (12, 30): "halv ett", (12, 45): "kvart i ett",
      (1, 0): "ett", (11, 55): "fem i tolv", (15, 15): "kvart över tre", (0, 0): "tolv", (23, 30): "halv tolv",
      (3, 1): "en minut över tre", (3, 7): "sju minuter över tre", (3, 21): "tjugoen minuter över tre",
      (3, 24): "tjugofyra minuter över tre", (3, 26): "fyra minuter i halv fyra", (3, 29): "en minut i halv fyra",
      (3, 31): "en minut över halv fyra", (3, 34): "fyra minuter över halv fyra", (3, 36): "tjugofyra minuter i fyra",
      (3, 59): "en minut i fyra"}
bad = {k: (K.say_sv(*k), v) for k, v in sv.items() if K.say_sv(*k) != v}
check(not bad, f"svenska: {len(sv)} tider i ord (kvart, halv, fem i halv, minuter) {bad or ''}")
en = {(3, 0): "three o'clock", (3, 5): "five past three", (3, 15): "quarter past three", (3, 25): "twenty-five past three",
      (3, 30): "half past three", (3, 35): "twenty-five to four", (3, 45): "quarter to four", (12, 50): "ten to one",
      (3, 1): "one minute past three", (3, 47): "thirteen minutes to four", (15, 20): "twenty past three"}
bad = {k: (K.say_en(*k), v) for k, v in en.items() if K.say_en(*k) != v}
check(not bad, f"engelska: {len(en)} tider i ord {bad or ''}")
check(len({K.say_sv(h, m) for h in range(1, 13) for m in range(60)}) == 720 and len({K.say_en(h, m) for h in range(1, 13) for m in range(60)}) == 720,
      "varje tid på urtavlan har sin egen fras (720 olika, båda språken)")
check(K.digital(3, 5) == "03:05" and K.digital(15, 30) == "15:30" and K.digital(24, 0) == "00:00", "digital tid med två siffror")
check([K.part_of_day(h, True) for h in (2, 7, 11, 12, 15, 19)] == ["på natten", "på morgonen", "på förmiddagen", "på eftermiddagen", "på eftermiddagen", "på kvällen"],
      "tid på dygnet")
check(K.duration(85, True) == "1 tim 25 min" and K.duration(60, True) == "1 tim" and K.duration(45, False) == "45 min" and K.duration(120, False) == "2 h", "hur lång tid i ord")

# ---- visarna
check(K.angles(3, 0) == (90, 0) and K.angles(3, 30) == (105, 180) and K.angles(12, 0) == (0, 0) and K.angles(15, 15) == (97.5, 90),
      "visarnas vinklar: timvisaren går med minuterna")
check(K.minute_from_angle(90) == 15 and K.minute_from_angle(92, 5) == 15 and K.minute_from_angle(359, 5) == 0 and K.minute_from_angle(100, 15) == 15
      and K.minute_from_angle(170, 30) == 30 and K.minute_from_angle(200, 60) == 0 and K.minute_from_angle(-6) == 59, "minutvisaren fäster på nivåns steg")
check(K.hour_from_angle(90, 0) == 3 and K.hour_from_angle(105, 30) == 3 and K.hour_from_angle(2, 0) == 12 and K.hour_from_angle(358, 0) == 12
      and K.hour_from_angle(345, 30) == 11, "timvisaren: timmen räknas ut med minuterna borträknade")
check(K.drag_minute(3, 55, 0) == (4, 0) and K.drag_minute(3, 0, 55) == (2, 55) and K.drag_minute(12, 50, 5) == (1, 5) and K.drag_minute(1, 5, 50) == (12, 50)
      and K.drag_minute(3, 20, 25) == (3, 25), "minutvisaren förbi tolvan flyttar timmen")
check(K.step_time(12, 55, 5) == (1, 0) and K.step_time(1, 0, -5) == (12, 55) and K.step_time(3, 15, 60) == (4, 15) and K.step_time(12, 0, -60) == (11, 0),
      "knapparna + och − räknar förbi tolv")
check(K.nearest_hand(180, 3, 30) == "minute" and K.nearest_hand(100, 3, 30) == "hour" and K.nearest_hand(92, 3, 15, 0.3) == "hour"
      and K.nearest_hand(92, 3, 15, 0.9) == "minute", "rätt visare greppas (nära mitten: den korta)")
check(K.same_time(3, 15, 15, 15) and not K.same_time(3, 15, 4, 15) and K.same_time(12, 0, 0, 0), "03:15 och 15:15 är samma läge på urtavlan")

check(K.hands_hint(3, 0, True) == "Den korta visaren pekar på 3, den långa på 12."
      and K.hands_hint(3, 15, True) == "Den korta visaren står mellan 3 och 4, den långa på 3 (15 min)."
      and K.hands_hint(12, 47, True) == "Den korta visaren står mellan 12 och 1, den långa på minutstreck 47 (47 min)."
      and K.hands_hint(6, 5, False) == "The short hand is between 6 and 7, the long hand at 1 (5 min).", "förklaringen efter ett fel: var visarna står")

# ---- uppgifterna: 300 per nivå och språk
steps = {"hel": 60, "halv": 30, "kvart": 15, "fem": 5, "minut": 1, "dygn": 5, "tid": 5}
order = ["hel", "halv", "kvart", "fem", "minut", "dygn", "tid", "enheter", "kalender", "byten", "tidtabell", "sekunder", "fart"]
check(K.LEVEL_IDS == order and all(K.level(i)["step"] == s for i, s in steps.items()), "13 nivåer i skolans ordning: " + ", ".join(K.LEVEL_IDS))
stages = [lv["stage"] for lv in K.LEVELS]
check(stages == sorted(stages, key=["1-3", "4-6", "7-9"].index) and set(stages) == {"1-3", "4-6", "7-9"} and set(K.LGR) == set(stages),
      "varje nivå hör till ett stadium (åk 1–3, 4–6, 7–9), och varje stadium har nivåer och kursplanens text")
# kursplanens ord (Lgr22, Skolverket – SKOLFS 2010:37 i senaste lydelse) ordagrant i programmet
for stage, quote in (("1-3", "Mätning av längd, massa, volym och tid med vanliga nutida och äldre måttenheter."),
                     ("1-3", "Jämförelser och uppskattningar av storheter."),
                     ("4-6", "Jämförelse, uppskattning och mätning av längd, area, massa, volym, tid och vinkel med standardiserade måttenheter samt enhetsbyten i samband med detta."),
                     ("7-9", "Härledda enheter, till exempel km/h och kr/kg.")):
    check(quote in K.LGR[stage][1], f"Lgr22 åk {stage}: {quote[:48]} …")
for lang, is_sv in (("svenska", True), ("engelska", False)):
    problems, kinds = [], {}
    for lid in K.LEVEL_IDS:
        rng = random.Random(1234 + len(lid))
        for i in range(300):
            t = K.make_task(lid, rng, is_sv)
            kinds.setdefault(lid, set()).add(t["kind"])
            if not t["question"] or not t["explain"]:
                problems.append((lid, i, "text saknas"))
            if t["target"] is None:
                o = t["options"]
                if not (len(o) == 4 and len(set(o)) == 4 and 0 <= t["correct"] < 4 and all(isinstance(x, str) and x for x in o)):
                    problems.append((lid, i, "alternativ", o))
                if not K.check(t, t["correct"]) or any(K.check(t, j) for j in range(4) if j != t["correct"]):
                    problems.append((lid, i, "rätt svar"))
            else:
                if not K.check(t, t["target"]) or K.check(t, K.step_time(t["target"][0], t["target"][1], 5 if t["step"] < 60 else 60)):
                    problems.append((lid, i, "ställ"))
            if lid in ("hel", "halv", "kvart", "fem") and t["m"] % steps[lid] != 0:
                problems.append((lid, i, "minut utanför nivån", t["m"]))
            if t["kind"] == "read" and t["options"][t["correct"]] != K.say(t["h"], t["m"], is_sv):
                problems.append((lid, i, "frasen"))
            if t["kind"] in ("digital", "day") and t["options"][t["correct"]] != K.digital(t["h"], t["m"]):
                problems.append((lid, i, "digital"))
    check(not problems, f"{lang}: 3 900 uppgifter – fyra olika svar, exakt ett rätt, tider inom nivån {problems[:3] or ''}")
    check(all(kinds[l["id"]] == set(l["kinds"]) for l in K.LEVELS), f"{lang}: varje nivå ger alla sina uppgiftstyper")

# hur lång tid: räkna efter
rng = random.Random(7)
ok = True
for _ in range(400):
    t = K.make_task("tid", rng, True)
    right = t["options"][t["correct"]]
    if t["kind"] == "length":
        a, b = t["shown"].split(" – ")
        mins = (int(b[:2]) * 60 + int(b[3:])) - (int(a[:2]) * 60 + int(a[3:]))
        ok &= right == K.duration(mins % (24 * 60), True)
    else:
        m = re.search(r"(?:om|för) (?:(\d+) tim)? ?(?:(\d+) min)?", t["question"])
        span = int(m.group(1) or 0) * 60 + int(m.group(2) or 0)
        start = int(t["shown"][:2]) * 60 + int(t["shown"][3:])
        end = start + span if t["kind"] == "later" else start - span
        ok &= span > 0 and right == K.digital((end // 60) % 24, end % 60)
check(ok, "hur lång tid: svaret stämmer när man räknar efter (400 uppgifter)")
# ---- tid utan urtavla (skola.py): varje svar räknat efter på eget sätt, 300 uppgifter per sort
import skola as S
DAY = {"måndag": 0, "tisdag": 1, "onsdag": 2, "torsdag": 3, "fredag": 4, "lördag": 5, "söndag": 6}
def doy(text):   # "3 april" → dag på året
    d, mname = text.split()
    return S.day_of_year(int(d), S.MONTHS_SV.index(mname) + 1)
def mins_of(text):   # "2 h 15 min" → 135
    h = re.search(r"(\d+) h", text); m = re.search(r"(\d+) min", text)
    return (int(h.group(1)) * 60 if h else 0) + (int(m.group(1)) if m else 0)
def f(x):
    return float(x.replace(",", "."))
wrongs = {}
for lv in [lv for lv in K.LEVELS if lv["id"] not in steps]:
    for kind in sorted(set(lv["kinds"])):
        rng = random.Random(len(kind) * 97)
        bad = []
        for i in range(300):
            t = K.make_task(lv["id"], rng, True, kind)
            q, sh, right = t["question"], t["shown"], t["options"][t["correct"]]
            if kind == "h2min":
                okk = right == f"{round(f(sh[:-2]) * 60)} min"
            elif kind == "min2h":
                okk = mins_of(right) == int(sh.split()[0])
            elif kind == "s2min":
                mm = re.match(r"(?:(\d+) min)? ?(?:(\d+) s)?", right)
                okk = int(mm.group(1) or 0) * 60 + int(mm.group(2) or 0) == int(sh.split()[0])
            elif kind == "decimal":
                okk = (mins_of(right) == round(f(sh[:-2]) * 60)) if sh.endswith(" h") else abs(f(right[:-2]) * 60 - int(sh.split()[0])) < 1e-9
            elif kind == "dayh":
                n = int(sh.split()[0])
                okk = right == str(n * (24 if "dygn" in sh else 7))
            elif kind in ("journey", "midnight"):
                a, b = sh.split(" → ")
                okk = mins_of(right) == ((int(b[:2]) * 60 + int(b[3:])) - (int(a[:2]) * 60 + int(a[3:]))) % 1440 and mins_of(right) > 0
            elif kind == "nextbus":
                arrive = re.search(r"hållplatsen (\d\d:\d\d)", q).group(1)
                times = re.findall(r"\d\d:\d\d", sh)
                okk = right == min(x for x in times + [S.clock(int(times[-1][:2]) * 60 + int(times[-1][3:]) + 10 ** 3)] if x > arrive)
            elif kind == "datediff":
                a, b = sh.split(" → ")
                okk = int(right) == doy(b) - doy(a) or int(right) == doy(b) + 365 - doy(a)
            elif kind == "date":
                n = 7 if "en vecka" in q else 14 if "två veckor" in q else int(re.search(r"(\d+) dagar", q).group(1))
                okk = (doy(right) - doy(sh)) % 365 == n
            elif kind == "weekday":
                d = DAY[sh.lower()]
                n = -1 if "i går" in q else -2 if "i förrgår" in q else 1 if "i morgon" in q else int(re.search(r"om (\d+) dagar", q).group(1))
                okk = DAY[right] == (d + n) % 7
            elif kind == "monthdays":
                okk = int(right) == S.DAYS_IN[S.MONTHS_SV.index(sh.lower())]
            elif kind == "month":
                okk = right in S.MONTHS_SV
            elif kind == "fastest":
                times = dict(re.findall(r"(\w+) ([\d,]+) s", sh))
                okk = right == min(times, key=lambda k: f(times[k]))
            elif kind == "secdiff":
                a, b = [f(x) for x in re.findall(r"([\d,]+) s", sh)]
                okk = abs(f(right[:-2]) - (b - a)) < 1e-9
            elif kind == "secconv":
                mm = re.match(r"(\d+) min (\d+) s", sh)
                okk = right == f"{int(mm.group(1)) * 60 + int(mm.group(2))} s"
            elif kind == "speed":
                km = f(sh.split(" km")[0]); okk = abs(f(right.split()[0]) - km / (mins_of(sh.split(", ")[1]) / 60)) < 1e-9
            elif kind == "distance":
                v, m = re.match(r"(\d+) km/h, (\d+) min", sh).groups(); okk = abs(f(right.split()[0]) - int(v) * int(m) / 60) < 1e-9
            elif kind == "traveltime":
                km, v = re.match(r"([\d,]+) km, (\d+) km/h", sh).groups(); okk = mins_of(right) == round(f(km) / int(v) * 60)
            elif kind == "kmh":
                val, unit = sh.split()
                okk = abs(f(right.split()[0]) - (f(val) * 3.6 if unit == "m/s" else f(val) / 3.6)) < 0.006
            elif kind == "hdec":
                okk = mins_of(right) == round(f(sh[:-2]) * 60)
            elif kind == "unitfact":
                facts = {"sekunder på en minut": 60, "minuter på en timme": 60, "timmar på ett dygn": 24, "dagar på en vecka": 7, "månader på ett år": 12,
                         "minuter på en kvart": 15, "minuter på en halvtimme": 30, "dagar på ett vanligt år": 365, "veckor på ett år": 52,
                         "år på ett decennium": 10, "år på ett sekel": 100}
                okk = int(right) == facts[sh]
            elif kind == "longest":
                size = {"1 sekund": 1, "1 minut": 2, "en kvart": 3, "en halvtimme": 4, "1 timme": 5, "1 dygn": 6, "1 vecka": 7, "1 månad": 8,
                        "1 år": 9, "ett decennium": 10, "ett sekel": 11}
                pickf = max if "längst" in q else min
                okk = right == pickf(t["options"], key=size.get)
            elif kind == "estimate":
                okk = right.split()[0] == t["options"][0].split()[0] and len({o.split()[0] for o in t["options"]}) == 1
            else:
                okk = False
            if not okk:
                bad.append((kind, q, sh, right))
            wrongs.setdefault(kind, set()).update(o for j, o in enumerate(t["options"]) if j != t["correct"])
        check(not bad, f"{lv['id']}/{kind}: rätt svar räknat efter i 300 uppgifter {bad[:1] or ''}")
# de vanliga felen finns bland felsvaren
check("1 h 50 min" in wrongs["decimal"] or "1 h 50 min" in wrongs["hdec"], "felsvar: 1,5 h läst som 1 h 50 min")
check(any(re.match(r"3[2-9] \w+", w) for w in wrongs["date"]), "felsvar: datum som inte finns (34 mars) – man glömmer att månaden tar slut")
check(any(o.startswith("1 h") for o in wrongs["journey"]), "tidtabellen: räknat som vanliga tal (14:47 → 15:22 blir 1 h 15 min)")
check("100 min" in wrongs["h2min"] or "150 min" in wrongs["h2min"], "felsvar: en timme som 100 minuter")
t = K.make_task("sekunder", random.Random(2), True, "fastest")
check(t["show"] == "text" and len(t["options"]) == 4, "sekunder: fyra löpare att välja mellan")
check(S.num(2.5, True) == "2,5" and S.num(2.5, False) == "2.5" and S.num(3.0, True) == "3" and S.hm(135, True) == "2 h 15 min"
      and S.hm(45, True) == "45 min" and S.ms(100) == "1 min 40 s" and S.clock(25 * 60 + 5) == "01:05", "tal och tider skrivs rätt (decimalkomma på svenska)")
check(S.from_day_of_year(S.day_of_year(3, 4)) == (3, 4) and S.from_day_of_year(366) == (1, 1) and S.date_text(3, 4, False) == "April 3",
      "kalendern: dag på året fram och tillbaka, nyår, engelskt datum")

# dygnet: eftermiddagens urtavla har två digitala tider – bara den som passar tiden på dygnet är rätt
rng = random.Random(3)
seen12 = 0
for _ in range(300):
    t = K.make_task("dygn", rng, True, "day")
    seen12 += K.digital((t["h"] + 12) % 24, t["m"]) in t["options"]
check(seen12 == 300, "dygnet: fel halva av dygnet (03:30 för 15:30) finns alltid bland svaren")

r1, r2 = K.make_round("fem", random.Random(5), True), K.make_round("fem", random.Random(5), True)
check(len(r1) == 10 and r1 == r2 and all((a["h"], a["m"]) != (b["h"], b["m"]) for a, b in zip(r1, r1[1:])),
      "en omgång: tio uppgifter, samma frö ger samma omgång, aldrig samma tid två gånger i rad")
check([K.stars(n) for n in (10, 9, 8, 7, 6, 5, 0)] == [3, 2, 2, 1, 1, 0, 0], "stjärnor: 10 rätt = 3, 8 = 2, 6 = 1")

# ---- 24-timmarsnivån: att ställa 15:15 kräver eftermiddag, inte bara visarna (ChatGPT 20 nov)
rng = random.Random(24)
sets = [K.make_task("dygn", rng, True, kind="set24") for _ in range(40)]
check(all(t.get("day") and t["target"] is not None for t in sets), "ställ-uppgifterna på 24-timmarsnivån frågar efter tiden på dygnet")
t = dict(sets[0], target=(15, 15))
check(K.check(t, (15, 15)) and not K.check(t, (3, 15)) and not K.check(t, (15, 20)), "15:15: eftermiddag rätt, 03:15 fel, fel minut fel")
t = dict(sets[0], target=(3, 15))
check(K.check(t, (3, 15)) and not K.check(t, (15, 15)), "03:15: förmiddag rätt, 15:15 fel")
t = dict(sets[0], target=(0, 30))
check(K.check(t, (0, 30)) and K.check(t, (24, 30)) and not K.check(t, (12, 30)), "00:30 (natt) och 12:30 (mitt på dagen) skiljs åt")
plain = K.make_task("halv", random.Random(1), True, kind="set")
check(not plain.get("day") and K.check(dict(plain, target=(3, 30)), (15, 30)), "övriga nivåer: bara visarna bedöms (03:30 = 15:30 på urtavlan)")
mainpy = (ROOT / "main.py").read_text(encoding="utf-8")
check('t.get("day")' in mainpy and "self.pm" in mainpy, "fönstret har förmiddag/eftermiddag för de uppgifterna")

# ---- framsteg
with tempfile.TemporaryDirectory() as d:
    path = os.path.join(d, "x", "framsteg.json")
    p = K.Progress(path)
    check(p.stars == {} and p.record("hel", 8) == (2, True) and p.record("hel", 6) == (1, False) and p.record("hel", 10) == (3, True), "bästa omgången räknas")
    q = K.Progress(path)
    check(q.stars == {"hel": 3} and q.rounds == 3 and q.total() == 3 and not os.path.exists(path + ".tmp"), "framstegen sparas och läses igen")
    for junk in ("{", "[1,2]", '{"stars": {"hel": 99, "finns-inte": 2, "halv": "x", "kvart": true}, "rounds": -4}', ""):
        open(path, "w").write(junk)
        z = K.Progress(path)
        if not (z.rounds == 0 and set(z.stars) <= {"hel"} and z.stars.get("hel", 3) == 3):
            check(False, f"trasig fil: {junk!r}")
    check(True, "en trasig eller påhittad framstegsfil ger en tom start, ingen krasch")
    # ChatGPT 20 nov: stars som inte är en ordbok kraschade (AttributeError) och programmet öppnades inte
    for junk in ('{"stars": "hej", "rounds": 1}', '{"stars": [1, 2, 3], "rounds": 2}', '{"stars": ["hel"]}', '{"stars": null, "rounds": 5}',
                 '{"stars": 7}', '{"stars": true}', "null", '"text"', "7"):
        open(path, "w").write(junk)
        try:
            z = K.Progress(path)
            okj = z.stars == {} and isinstance(z.rounds, int)
        except Exception as e:   # noqa: BLE001 – precis det som provas
            okj = False
            junk += f" → {type(e).__name__}"
        check(okj, f"stars av fel sort ger en tom start: {junk}")
    ro = K.Progress("/proc/finns-inte/framsteg.json")
    check(ro.record("hel", 10) == (3, True), "går inte filen att skriva fungerar programmet ändå")

# ---- programmets filer
desk = (ROOT / "data/se.wenuyos.app.klockan.desktop").read_text(encoding="utf-8")
check(all(k in desk for k in ("Name=", "Name[sv]=", "Comment=", "Comment[sv]=", "Icon=se.wenuyos.app.klockan", "Exec=", "Keywords=", "Keywords[sv]=",
                              "Categories=Education;", "StartupWMClass=se.wenuyos.app.klockan")), ".desktop: namn, beskrivning, sökord på båda språken")
check((ROOT / "data/se.wenuyos.app.klockan.svg").is_file(), "ikonen finns")
import tomllib
man = tomllib.loads((ROOT / "wenuyos-app.toml").read_text(encoding="utf-8"))
check(man["id"] == "se.wenuyos.app.klockan" and man["needs_network"] is False and man["min_ram_mb"] <= 2048 and man["description_sv"],
      "manifestet: id, inget nätverk, 2 GB")
main = (ROOT / "main.py").read_text(encoding="utf-8")
check('application_id="se.wenuyos.app.klockan"' in main, "samma id i programmet som i .desktop-filen")
code = "\n".join(l for l in (main + (ROOT / "klocka.py").read_text(encoding="utf-8")).splitlines() if not l.lstrip().startswith("#"))
check(not re.search(r"\b(urllib|requests|socket|http\.client|curl|wget)\b", code), "programmet kopplar aldrig upp sig")
check(not re.search(r"#[0-9a-fA-F]{6}\b|#[0-9a-fA-F]{3}\b|(?<![_a-z])rgba?\(", main), "inga hårdkodade färger – visarna får temats färger")
check(not re.search(r"timeout_add|add_tick_callback|idle_add", main), "inget som tickar i bakgrunden")
texts = re.findall(r'(?:label|title|subtitle|tooltip_text)="([A-ZÅÄÖa-zåäö][^"]*)"', main)
check(not texts, f"alla texter i fönstret går genom tr() {texts[:3] or ''}")

print(f"\nKlockan: {total - len(fails)} av {total} prov godkända")
sys.exit(1 if fails else 0)
