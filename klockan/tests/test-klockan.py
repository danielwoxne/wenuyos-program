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

# ---- uppgifterna: 300 per nivå och språk
steps = {"hel": 60, "halv": 30, "kvart": 15, "fem": 5, "minut": 1, "dygn": 5, "tid": 5}
check(K.LEVEL_IDS == list(steps) and all(K.level(i)["step"] == s for i, s in steps.items()), "sju nivåer i skolans ordning: " + ", ".join(K.LEVEL_IDS))
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
    check(not problems, f"{lang}: 2 100 uppgifter – fyra olika svar, exakt ett rätt, tider inom nivån {problems[:3] or ''}")
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
check(not re.search(r"#[0-9a-fA-F]{6}\b|#[0-9a-fA-F]{3}\b|rgba?\(", main), "inga hårdkodade färger – visarna får temats färger")
check(not re.search(r"timeout_add|add_tick_callback|idle_add", main), "inget som tickar i bakgrunden")
texts = re.findall(r'(?:label|title|subtitle|tooltip_text)="([A-ZÅÄÖa-zåäö][^"]*)"', main)
check(not texts, f"alla texter i fönstret går genom tr() {texts[:3] or ''}")

print(f"\nKlockan: {total - len(fails)} av {total} prov godkända")
sys.exit(1 if fails else 0)
