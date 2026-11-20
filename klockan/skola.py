"""Klockan – tid utöver urtavlan, efter grundskolans kursplan i matematik (Lgr22, centralt innehåll om tid):

  åk 1–3  "Jämförelser och uppskattningar av storheter."
          "Mätning av längd, massa, volym och tid med vanliga nutida och äldre måttenheter."
            → tidsenheter (sekund … år, kvart, decennium, sekel), uppskatta hur lång tid något tar, kalendern
  åk 4–6  "Jämförelse, uppskattning och mätning av längd, area, massa, volym, tid och vinkel med standardiserade
          måttenheter samt enhetsbyten i samband med detta."
            → enhetsbyten (h, min, s, dygn, veckor, timmar i decimalform), tidtabeller, över midnatt, dagar mellan
              datum, sekunder och tiondelar
  åk 7–9  "Härledda enheter, till exempel km/h och kr/kg."
            → hastighet, sträcka och tid, km/h och m/s

Varje uppgift är flerval med fyra svar. Felsvaren är de vanliga misstagen (1,5 h = 1 h 50 min, 12,4 s < 12,38 s,
34 mars, 100 minuter på en timme …). Ingen GTK här – provas i tests/test-klockan.py."""

# ------------------------------------------------------------------ tal och tid som text
MONTHS_SV = ["januari", "februari", "mars", "april", "maj", "juni", "juli", "augusti", "september", "oktober", "november", "december"]
MONTHS_EN = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"]
DAYS_IN = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]          # ett vanligt år (inte skottår)
WEEKDAYS_SV = ["måndag", "tisdag", "onsdag", "torsdag", "fredag", "lördag", "söndag"]
WEEKDAYS_EN = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]


def num(x, sv):
    """3 → "3", 2.5 → "2,5" (svenska) / "2.5"; högst två decimaler, inga nollor på slutet"""
    s = f"{round(x, 2):.2f}".rstrip("0").rstrip(".")
    return s.replace(".", ",") if sv else s


def hm(minutes, sv):
    """135 → "2 h 15 min", 45 → "45 min", 120 → "2 h" """
    h, m = divmod(int(minutes), 60)
    if h and m:
        return f"{h} h {m} min"
    return f"{h} h" if h else f"{m} min"


def ms(seconds):
    """100 → "1 min 40 s" """
    m, s = divmod(int(seconds), 60)
    if m and s:
        return f"{m} min {s} s"
    return f"{m} min" if m else f"{s} s"


def clock(minutes):
    """minuter efter midnatt → "07:05" (går runt efter 24 h)"""
    minutes %= 24 * 60
    return f"{minutes // 60:02d}:{minutes % 60:02d}"


def date_text(day, month, sv):
    """(3, 4) → "3 april" / "April 3" (månad 1–12)"""
    return f"{day} {MONTHS_SV[month - 1]}" if sv else f"{MONTHS_EN[month - 1]} {day}"


FEB_END = 59   # 28 februari är årets dag 59 ett vanligt år


def passes_february(start, n):
    """går dagarna start → start + n förbi slutet av februari? Då beror svaret på skottår (ChatGPT 20 nov:
    27 februari → 2 mars är 3 dagar ett vanligt år, 4 ett skottår) – uppgiften säger då "ett vanligt år"."""
    return any((start + k - 1) % 365 + 1 == FEB_END for k in range(n))


def leap_note(sv):
    return (" Februari har 28 dagar ett vanligt år och 29 ett skottår." if sv
            else " February has 28 days in a common year and 29 in a leap year.")


def day_of_year(day, month):
    return sum(DAYS_IN[:month - 1]) + day


def from_day_of_year(n):
    """1–365 → (dag, månad); går runt till nästa år"""
    n = (n - 1) % 365 + 1
    for i, d in enumerate(DAYS_IN):
        if n <= d:
            return n, i + 1
        n -= d
    raise ValueError(n)


# ------------------------------------------------------------------ nivåerna (läggs till i klocka.LEVELS)
LGR = {
    "1-3": ("Grades 1–3: comparing and estimating quantities; measuring time with common modern and older units.",
            "Åk 1–3: \"Jämförelser och uppskattningar av storheter.\" \"Mätning av längd, massa, volym och tid med vanliga "
            "nutida och äldre måttenheter.\""),
    "4-6": ("Grades 4–6: comparing, estimating and measuring time with standard units, and converting between units.",
            "Åk 4–6: \"Jämförelse, uppskattning och mätning av längd, area, massa, volym, tid och vinkel med standardiserade "
            "måttenheter samt enhetsbyten i samband med detta.\""),
    "7-9": ("Grades 7–9: derived units, for example km/h.",
            "Åk 7–9: \"Härledda enheter, till exempel km/h och kr/kg.\""),
}
# Kursplanen anger centralt innehåll per stadium, inte i vilken årskurs varje del ska läras (ChatGPT 20 nov):
# nivåernas placering är programmets förslag på progression.
PROGRESSION_NOTE = ("The order of the levels is this program's suggested progression; the syllabus describes core content per stage, "
                    "not a requirement for each level.",
                    "Nivåernas ordning är programmets förslag på progression – kursplanen beskriver centralt innehåll per stadium, "
                    "inte krav för varje nivå.")
STAGES = [("1-3", ("Grades 1–3", "Årskurs 1–3")), ("4-6", ("Grades 4–6", "Årskurs 4–6")), ("7-9", ("Grades 7–9", "Årskurs 7–9"))]

LEVELS = [
    {"id": "enheter", "stage": "1-3", "step": 5, "kinds": ["longest", "unitfact", "estimate", "estimate"],
     "name": ("Seconds, minutes, hours …", "Sekund, minut, timme …"),
     "about": ("60 seconds = 1 minute, 60 minutes = 1 hour, 24 hours = 1 day.",
               "60 sekunder = 1 minut, 60 minuter = 1 timme, 24 timmar = 1 dygn.")},
    {"id": "kalender", "stage": "1-3", "step": 5, "kinds": ["weekday", "month", "monthdays", "date"],
     "name": ("Days, months and dates", "Veckodagar, månader och datum"),
     "about": ("7 days in a week, 12 months in a year.", "7 dagar på en vecka, 12 månader på ett år.")},
    {"id": "byten", "stage": "4-6", "step": 5, "kinds": ["h2min", "min2h", "s2min", "decimal", "dayh"],
     "name": ("Changing units of time", "Byta enhet för tid"),
     "about": ("1.5 h is 1 h 30 min – not 1 h 50 min. An hour has 60 minutes.",
               "1,5 h är 1 h 30 min – inte 1 h 50 min. En timme har 60 minuter.")},
    {"id": "tidtabell", "stage": "4-6", "step": 5, "kinds": ["journey", "midnight", "nextbus", "datediff"],
     "name": ("Timetables and dates", "Tidtabeller och datum"),
     "about": ("Count up to the next whole hour, then on.", "Räkna upp till nästa hela timme, sedan vidare.")},
    {"id": "sekunder", "stage": "4-6", "step": 5, "kinds": ["fastest", "fastest", "secdiff", "secconv"],
     "name": ("Seconds and tenths", "Sekunder och tiondelar"),
     "about": ("12.38 s is faster than 12.4 s – compare tenths first.", "12,38 s är snabbare än 12,4 s – jämför tiondelarna först.")},
    {"id": "fart", "stage": "7-9", "step": 5, "kinds": ["speed", "distance", "traveltime", "kmh", "hdec"],
     "name": ("Speed, distance and time", "Hastighet, sträcka och tid"),
     "about": ("speed = distance / time. 1 m/s = 3.6 km/h.", "hastighet = sträcka / tid. 1 m/s = 3,6 km/h.")},
]
KINDS = {k for lv in LEVELS for k in lv["kinds"]}


# ------------------------------------------------------------------ uppgifterna
def make(kind, rng, sv, t, options):
    """fyller i uppgiften t (show "text", shown, question, options, correct, explain). options = klocka._options"""
    t = _make(kind, rng, sv, t, options)
    t["explain"] = t["explain"][:1].upper() + t["explain"][1:]
    return t


def _make(kind, rng, sv, t, options):
    T = (lambda en, s: s) if sv else (lambda en, s: en)
    t["show"] = "text"
    t["h"], t["m"] = rng.randrange(1000000), 0      # (make_round jämför (h, m) för att inte upprepa en uppgift)
    pick = rng.choice

    # ---- åk 1–3: tidsenheter
    if kind == "longest":
        units = [(1, ("1 second", "1 sekund")), (60, ("1 minute", "1 minut")), (15 * 60, ("a quarter of an hour", "en kvart")),
                 (30 * 60, ("half an hour", "en halvtimme")), (3600, ("1 hour", "1 timme")), (86400, ("1 day (24 hours)", "1 dygn")),
                 (7 * 86400, ("1 week", "1 vecka")), (30 * 86400, ("1 month", "1 månad")), (365 * 86400, ("1 year", "1 år")),
                 (10 * 365 * 86400, ("a decade (10 years)", "ett decennium")), (100 * 365 * 86400, ("a century (100 years)", "ett sekel"))]
        four = rng.sample(units, 4)
        longest = rng.random() < 0.6
        best = max(four) if longest else min(four)
        t["question"] = T("Which is the longest time?", "Vilken tid är längst?") if longest else T("Which is the shortest time?", "Vilken tid är kortast?")
        t["shown"] = T("Compare", "Jämför")
        t["options"] = [T(*u[1]) for u in four]
        t["correct"] = four.index(best)
        order = sorted(four)
        t["explain"] = T("From shortest to longest: ", "Från kortast till längst: ") + " < ".join(T(*u[1]) for u in order) + "."
        return t
    if kind == "unitfact":
        facts = [(60, ("seconds in a minute", "sekunder på en minut")), (60, ("minutes in an hour", "minuter på en timme")),
                 (24, ("hours in a day", "timmar på ett dygn")), (7, ("days in a week", "dagar på en vecka")),
                 (12, ("months in a year", "månader på ett år")), (15, ("minutes in a quarter of an hour", "minuter på en kvart")),
                 (30, ("minutes in half an hour", "minuter på en halvtimme")), (365, ("days in a year (not a leap year)", "dagar på ett vanligt år")),
                 (52, ("weeks in a year (about)", "veckor på ett år (ungefär)")), (10, ("years in a decade", "år på ett decennium")),
                 (100, ("years in a century", "år på ett sekel"))]
        n, what = pick(facts)
        t["question"] = T(f"How many {what[0]}?", f"Hur många {what[1]}?")
        t["shown"] = T(*what).split("(")[0].strip()
        wrong = [100, 60, 24, 7, 12, 10, 30, 365, 52, 15, 50, 31]
        t["options"], t["correct"] = options(rng, str(n), [str(w) for w in wrong if w != n], lambda: str(rng.randint(5, 120)))
        t["explain"] = T(f"There are {n} {what[0]}.", f"Det är {n} {what[1]}.")
        return t
    if kind == "estimate":
        # (antal, enhet, (engelska, svenska), är det något man gör?) – samma tal med fel enhet är felsvaren
        acts = [(2, 1, ("brush your teeth", "borsta tänderna"), True), (90, 1, ("a football match", "en fotbollsmatch"), False),
                (10, 2, ("sleep a whole night", "sova en hel natt"), True), (1, 0, ("blink", "blinka"), True),
                (10, 4, ("the summer holiday", "sommarlovet"), False), (6, 2, ("a school day", "en skoldag"), False),
                (15, 1, ("walk one kilometre", "gå en kilometer"), True), (2, 2, ("a long film", "en lång film"), False),
                (5, 0, ("tie your shoes", "knyta skorna"), True), (9, 5, ("a pregnancy", "en graviditet"), False),
                (3, 1, ("boil a soft egg", "koka ett löskokt ägg"), True), (2, 3, ("a weekend", "en helg"), False),
                (40, 1, ("a lesson", "en lektion"), False), (30, 0, ("put on a jacket", "ta på sig jackan"), True)]
        names = [("seconds", "sekunder"), ("minutes", "minuter"), ("hours", "timmar"), ("days", "dagar"), ("weeks", "veckor"), ("months", "månader"), ("years", "år")]
        n, unit, what, doing = pick(acts)
        t["question"] = (T(f"About how long does it take to {what[0]}?", f"Ungefär hur lång tid tar det att {what[1]}?") if doing
                         else T(f"About how long is {what[0]}?", f"Ungefär hur lång tid är {what[1]}?"))
        t["shown"] = T(*what).capitalize()
        near = [u for u in (unit - 2, unit - 1, unit + 1, unit + 2, unit + 3, unit - 3) if 0 <= u < len(names)]
        t["options"], t["correct"] = options(rng, f"{n} {T(*names[unit])}", [f"{n} {T(*names[u])}" for u in near], lambda: "")
        t["explain"] = T(f"About {n} {names[unit][0]}.", f"Ungefär {n} {names[unit][1]}.")
        return t

    # ---- åk 1–3: kalendern
    if kind == "weekday":
        d, n = rng.randrange(7), rng.choice([1, 2, 3, 4, 5, 6, -1, -2, 7, 8, 10])
        days = WEEKDAYS_SV if sv else WEEKDAYS_EN
        ans = days[(d + n) % 7]
        if n == -1:
            q = T(f"Today is {days[d]}. What day was it yesterday?", f"I dag är det {days[d]}. Vilken dag var det i går?")
        elif n == -2:
            q = T(f"Today is {days[d]}. What day was it the day before yesterday?", f"I dag är det {days[d]}. Vilken dag var det i förrgår?")
        elif n == 1:
            q = T(f"Today is {days[d]}. What day is it tomorrow?", f"I dag är det {days[d]}. Vilken dag är det i morgon?")
        else:
            q = T(f"Today is {days[d]}. What day is it in {n} days?", f"I dag är det {days[d]}. Vilken dag är det om {n} dagar?")
        t["question"], t["shown"] = q, days[d].capitalize()
        t["options"], t["correct"] = options(rng, ans, [days[(d + n + k) % 7] for k in (1, -1, 2, -2)] + [days[(d - n) % 7]], lambda: pick(days))
        t["explain"] = T(f"{days[d]} + {n} days = {ans}." if n > 0 else f"{abs(n)} days before {days[d]} is {ans}.",
                         f"{days[d]} + {n} dagar = {ans}." if n > 0 else f"{abs(n)} dagar före {days[d]} är {ans}.")
        if n >= 7:
            t["explain"] += " " + T("After 7 days it is the same day again.", "Efter 7 dagar är det samma dag igen.")
        return t
    if kind == "month":
        months = MONTHS_SV if sv else MONTHS_EN
        m = rng.randrange(12)
        how = rng.choice(["after", "before", "nth"])
        if how == "after":
            ans, q = months[(m + 1) % 12], T(f"Which month comes after {months[m]}?", f"Vilken månad kommer efter {months[m]}?")
            t["explain"] = T(f"After {months[m]} comes {ans}.", f"Efter {months[m]} kommer {ans}.")
        elif how == "before":
            ans, q = months[(m - 1) % 12], T(f"Which month comes before {months[m]}?", f"Vilken månad kommer före {months[m]}?")
            t["explain"] = T(f"Before {months[m]} comes {ans}.", f"Före {months[m]} kommer {ans}.")
        else:
            ans, q = months[m], T(f"Which is month number {m + 1} of the year?", f"Vilken är årets månad nummer {m + 1}?")
            t["explain"] = T(f"Month {m + 1} is {ans}.", f"Månad nummer {m + 1} är {ans}.")
        t["question"], t["shown"] = q, T("The months", "Månaderna")
        t["options"], t["correct"] = options(rng, ans, [months[(m + k) % 12] for k in (2, -2, 1, -1, 0)], lambda: pick(months))
        return t
    if kind == "monthdays":
        months = MONTHS_SV if sv else MONTHS_EN
        m = rng.randrange(12)
        n = DAYS_IN[m]
        t["question"] = T(f"How many days are there in {months[m]} (not a leap year)?", f"Hur många dagar har {months[m]} ett vanligt år?")
        t["shown"] = months[m].capitalize()
        t["options"], t["correct"] = options(rng, str(n), ["28", "29", "30", "31", "32"], lambda: "")
        t["explain"] = T(f"{months[m]} has {n} days. Count on your knuckles: a knuckle is 31 days.",
                         f"{months[m]} har {n} dagar. Räkna på knogarna: en knoge är 31 dagar.")
        return t
    if kind == "date":
        start = rng.randint(1, 365)
        sd, sm = from_day_of_year(start)
        while sd < DAYS_IN[sm - 1] - 8:   # nära slutet av månaden, så att man går över till nästa
            start = rng.randint(1, 365)
            sd, sm = from_day_of_year(start)
        n = rng.choice([n for n in (7, 7, 3, 5, 10, 14) if sd + n > DAYS_IN[sm - 1]] or [14])   # alltid in i nästa månad
        ed, em = from_day_of_year(start + n)
        ans = date_text(ed, em, sv)
        what = T("a week", "en vecka") if n == 7 else T("two weeks", "två veckor") if n == 14 else T(f"{n} days", f"{n} dagar")
        feb = passes_february(start, n)
        t["question"] = (T(f"Which date is it {what} after {date_text(sd, sm, False)}?", f"Vilket datum är det {what} efter {date_text(sd, sm, True)}?")
                         + (T(" (A common year, not a leap year.)", " (Ett vanligt år, inte skottår.)") if feb else ""))
        t["shown"] = date_text(sd, sm, sv)
        naive = f"{sd + n} {MONTHS_SV[sm - 1]}" if sv else f"{MONTHS_EN[sm - 1]} {sd + n}"   # "34 mars" – det vanliga felet
        wrong = [naive, date_text(*from_day_of_year(start + n + 1), sv), date_text(*from_day_of_year(start + n - 1), sv),
                 date_text(min(ed, DAYS_IN[sm - 1]), sm, sv)]
        t["options"], t["correct"] = options(rng, ans, wrong, lambda: date_text(*from_day_of_year(start + n + rng.choice([2, -2, 3])), sv))
        t["explain"] = T(f"{MONTHS_EN[sm - 1]} has {DAYS_IN[sm - 1]} days, so {what} after {date_text(sd, sm, False)} is {ans}.",
                         f"{MONTHS_SV[sm - 1]} har {DAYS_IN[sm - 1]} dagar, så {what} efter {date_text(sd, sm, True)} är det {ans}.")
        if feb:
            t["explain"] += leap_note(sv)
        return t

    # ---- åk 4–6: enhetsbyten
    if kind == "h2min":
        h = pick([1, 2, 3, 4, 1.5, 2.5, 3.5, 0.5, 1.25, 2.75])
        ans = int(h * 60)
        t["question"] = T(f"{num(h, sv)} h = ? min", f"{num(h, sv)} h = ? min")
        t["shown"] = f"{num(h, sv)} h"
        whole, frac = int(h), round(h - int(h), 2)
        wrong = [int(h * 100), whole * 60 + int(frac * 100), int(h * 60) + 30, int(h * 60) - 30, int(h * 60) + 10]
        t["options"], t["correct"] = options(rng, f"{ans} min", [f"{w} min" for w in wrong if w > 0], lambda: f"{ans + rng.choice([-15, 15, 45])} min")
        t["explain"] = T(f"1 h = 60 min, so {num(h, sv)} h = {num(h, sv)} · 60 min = {ans} min.",
                         f"1 h = 60 min, så {num(h, sv)} h = {num(h, sv)} · 60 min = {ans} min.")
        return t
    if kind == "min2h":
        m = rng.choice([75, 90, 105, 135, 150, 165, 200, 250, 95, 125])
        t["question"] = T(f"{m} min = ? h ? min", f"{m} min = ? h ? min")
        t["shown"] = f"{m} min"
        h, r = divmod(m, 60)
        wrong = [f"{m // 100} h {m % 100} min", hm(m + 60, sv), hm(m - 60, sv) if m > 60 else "", f"{h} h {r + 10} min", hm(m - 10, sv)]
        t["options"], t["correct"] = options(rng, hm(m, sv), [w for w in wrong if w], lambda: hm(m + rng.choice([-20, 20, 30]), sv))
        t["explain"] = T(f"{m} min = {h} · 60 min + {r} min = {hm(m, sv)}.", f"{m} min = {h} · 60 min + {r} min = {hm(m, sv)}.")
        return t
    if kind == "s2min":
        s = rng.choice([90, 100, 125, 150, 200, 75, 130, 245])
        t["question"] = T(f"{s} s = ? min ? s", f"{s} s = ? min ? s")
        t["shown"] = f"{s} s"
        wrong = [f"{s // 100} min {s % 100} s", ms(s + 60), ms(s - 30), ms(s + 10)]
        t["options"], t["correct"] = options(rng, ms(s), wrong, lambda: ms(s + rng.choice([-20, 20, 40])))
        t["explain"] = T(f"1 min = 60 s, so {s} s = {ms(s)}.", f"1 min = 60 s, så {s} s = {ms(s)}.")
        return t
    if kind == "decimal":
        h = pick([0.25, 0.5, 0.75, 1.5, 1.25, 2.75, 0.1, 0.2, 1.2])
        mins = round(h * 60)
        if rng.random() < 0.5:   # decimal → h och min
            t["question"] = f"{num(h, sv)} h = ? min" if h < 1 else f"{num(h, sv)} h = ? h ? min"
            t["shown"] = f"{num(h, sv)} h"
            whole, frac = int(h), round(h - int(h), 2)
            naive = whole * 60 + int(round(frac * 100))     # 1,5 h → 1 h 50 min
            wrong = [hm(naive, sv), hm(mins + 15, sv), hm(mins - 5, sv) if mins > 5 else "", hm(mins + 30, sv)]
            t["options"], t["correct"] = options(rng, hm(mins, sv), [w for w in wrong if w and w != "0 min"], lambda: hm(mins + rng.choice([10, 20, -10]), sv))
        else:                    # minuter → decimal
            t["question"] = T(f"{mins} min = ? h", f"{mins} min = ? h")
            t["shown"] = f"{mins} min"
            wrong = [num(mins / 100, sv), num(round(h + 0.1, 2), sv), num(round(h * 2, 2), sv), num(round(h / 2, 2), sv)]
            t["options"], t["correct"] = options(rng, num(h, sv) + " h", [w + " h" for w in wrong], lambda: "")
        tail = f" = {hm(mins, sv)}" if mins >= 60 else ""
        t["explain"] = T(f"{num(h, sv)} h = {num(h, sv)} · 60 min = {mins} min{tail}.", f"{num(h, sv)} h = {num(h, sv)} · 60 min = {mins} min{tail}.")
        return t
    if kind == "dayh":
        if rng.random() < 0.5:
            d = rng.choice([2, 3, 4, 5, 7])
            t["question"] = T(f"{d} days = ? hours", f"{d} dygn = ? timmar")
            t["shown"] = T(f"{d} days", f"{d} dygn")
            t["options"], t["correct"] = options(rng, str(d * 24), [str(d * 60), str(d * 12), str(d * 24 + 24), str(d * 100)], lambda: "")
            t["explain"] = T(f"1 day = 24 h, so {d} days = {d} · 24 h = {d * 24} h.", f"1 dygn = 24 h, så {d} dygn = {d} · 24 h = {d * 24} h.")
        else:
            w = rng.choice([2, 3, 4, 6, 8])
            t["question"] = T(f"{w} weeks = ? days", f"{w} veckor = ? dagar")
            t["shown"] = T(f"{w} weeks", f"{w} veckor")
            t["options"], t["correct"] = options(rng, str(w * 7), [str(w * 5), str(w * 10), str(w * 7 + 7), str(w * 30)], lambda: "")
            t["explain"] = T(f"1 week = 7 days, so {w} weeks = {w * 7} days.", f"1 vecka = 7 dagar, så {w} veckor = {w * 7} dagar.")
        return t

    # ---- åk 4–6: tidtabeller och datum
    if kind in ("journey", "midnight"):
        if kind == "journey":
            start, span = 0, 0
            while (start % 60) + (span % 60) < 60:      # över en hel timme – där blir det fel
                start, span = rng.randint(6 * 60, 20 * 60), rng.randint(14, 110)
            what = pick([("The bus", "Bussen"), ("The train", "Tåget"), ("The boat", "Båten")])
        else:
            start = rng.randint(21 * 60, 23 * 60 + 50)
            span = rng.randint(70, 300)
            while start + span < 24 * 60 + 10:
                span = rng.randint(70, 300)
            what = pick([("The night train", "Nattåget"), ("The plane", "Flyget"), ("The night bus", "Nattbussen")])
        end = start + span
        t["shown"] = f"{clock(start)} → {clock(end)}"
        t["question"] = T(f"{what[0]} leaves at {clock(start)} and arrives at {clock(end)}. How long is the journey?",
                          f"{what[1]} går {clock(start)} och kommer fram {clock(end)}. Hur lång tid tar resan?")
        naive = (end % (24 * 60)) // 60 * 100 + end % 60 - (start // 60 * 100 + start % 60)   # räknat som vanliga tal
        wrong = [hm(span + 60, sv), hm(span - 60, sv) if span > 60 else "", hm(span + 40, sv), hm(span - 40, sv) if span > 40 else "",
                 hm(naive, sv) if 0 < naive < 600 else "", hm(span + 10, sv)]
        t["options"], t["correct"] = options(rng, hm(span, sv), [w for w in wrong if w], lambda: hm(span + rng.choice([-20, 20, 30]), sv))
        up = 60 - start % 60
        t["explain"] = T(f"From {clock(start)} to {clock(start + up)} is {up} min, then {hm(span - up, sv)} more: {hm(span, sv)}.",
                         f"Från {clock(start)} till {clock(start + up)} är det {up} min, sedan {hm(span - up, sv)} till: {hm(span, sv)}.")
        return t
    if kind == "nextbus":
        every = pick([10, 15, 20, 30])
        first = rng.randint(6 * 60, 8 * 60)
        deps = [first + i * every for i in range(5)]
        k = rng.randint(1, 3)
        arrive = deps[k] - rng.randint(1, every - 1)
        t["shown"] = "  ".join(clock(d) for d in deps[:4]) + " …"
        t["question"] = T(f"The bus leaves at {', '.join(clock(d) for d in deps[:3])} … every {every} minutes. You get to the stop at {clock(arrive)}. "
                          "When does the next bus leave?",
                          f"Bussen går {', '.join(clock(d) for d in deps[:3])} … var {every}:e minut. Du kommer till hållplatsen {clock(arrive)}. "
                          "När går nästa buss?")
        t["options"], t["correct"] = options(rng, clock(deps[k]), [clock(deps[k - 1]), clock(deps[k + 1]), clock(arrive + every), clock(deps[k] + 5)],
                                             lambda: clock(deps[k] + rng.choice([-3, 3, 7])))
        t["explain"] = T(f"The bus at {clock(deps[k - 1])} has left; the next one is {clock(deps[k])}.",
                         f"Bussen {clock(deps[k - 1])} har gått; nästa går {clock(deps[k])}.")
        return t
    if kind == "datediff":
        start = rng.randint(1, 340)
        sd, sm = from_day_of_year(start)
        while sd < DAYS_IN[sm - 1] - 10:
            start = rng.randint(1, 340)
            sd, sm = from_day_of_year(start)
        n = rng.randint(DAYS_IN[sm - 1] - sd + 1, DAYS_IN[sm - 1] - sd + 15)   # alltid in i nästa månad
        ed, em = from_day_of_year(start + n)
        t["shown"] = f"{date_text(sd, sm, sv)} → {date_text(ed, em, sv)}"
        feb = passes_february(start, n)
        t["question"] = (T(f"How many days is it from {date_text(sd, sm, False)} to {date_text(ed, em, False)}?",
                           f"Hur många dagar är det från {date_text(sd, sm, True)} till {date_text(ed, em, True)}?")
                         + (T(" (A common year, not a leap year.)", " (Ett vanligt år, inte skottår.)") if feb else ""))
        naive = abs(ed - sd)
        t["options"], t["correct"] = options(rng, str(n), [str(naive), str(n + 1), str(n - 1), str(n + 2)], lambda: str(n + rng.choice([3, -2, 4])))
        rest = DAYS_IN[sm - 1] - sd
        t["explain"] = T(f"{rest} days to the end of {MONTHS_EN[sm - 1]} ({DAYS_IN[sm - 1]} days), then {ed} more: {rest} + {ed} = {n}.",
                         f"{rest} dagar till slutet av {MONTHS_SV[sm - 1]} ({DAYS_IN[sm - 1]} dagar), sedan {ed} till: {rest} + {ed} = {n}.")
        if feb:
            t["explain"] += leap_note(sv)
        return t

    # ---- åk 4–6: sekunder och tiondelar
    if kind == "fastest":
        names = ["Ali", "Bea", "Cem", "Dana", "Elin", "Farah", "Gustav", "Hana"]
        who = rng.sample(names, 4)
        base = rng.randint(9, 15)
        tenths = rng.sample(range(0, 10), 2)
        a = base + max(tenths) / 10                     # t.ex. 12,4
        b = base + min(tenths) / 10 + rng.randint(1, 9) / 100   # t.ex. 12,38 – fler siffror men mindre
        c = base + rng.randint(0, 9) / 10 + rng.randint(1, 9) / 100 + 0.2
        d = base + 1 + rng.randint(0, 9) / 10
        times = [round(x, 2) for x in (a, b, c, d)]
        if len(set(times)) < 4:
            times = [round(base + i * 0.13 + 0.01, 2) for i in range(4)]
        best = min(range(4), key=lambda i: times[i])
        t["shown"] = "\n".join(f"{n} {num(x, sv)} s" for n, x in zip(who, times))   # en löpare per rad
        t["question"] = T("60 metres: who was fastest?", "60 meter: vem var snabbast?")
        t["options"], t["correct"] = list(who), best
        order = sorted(range(4), key=lambda i: times[i])
        t["explain"] = T("Fastest = shortest time. ", "Snabbast = kortast tid. ") + " < ".join(f"{num(times[i], sv)} s" for i in order) + "."
        return t
    if kind == "secdiff":
        a = rng.randint(100, 180) / 10
        b = round(a + rng.randint(2, 15) / 10, 1)
        names = rng.sample(["Ali", "Bea", "Cem", "Dana", "Elin", "Farah"], 2)
        diff = round(b - a, 1)
        t["shown"] = f"{names[0]} {num(a, sv)} s\n{names[1]} {num(b, sv)} s"
        t["question"] = T(f"{names[0]} ran in {num(a, False)} s and {names[1]} in {num(b, False)} s. How much faster was {names[0]}?",
                          f"{names[0]} sprang på {num(a, True)} s och {names[1]} på {num(b, True)} s. Hur mycket snabbare var {names[0]}?")
        wrong = [num(diff + 1, sv), num(round(diff / 10, 2), sv), num(round(diff + 0.1, 1), sv), num(round(diff - 0.1, 1), sv) if diff > 0.2 else ""]
        t["options"], t["correct"] = options(rng, f"{num(diff, sv)} s", [w + " s" for w in wrong if w], lambda: "")
        t["explain"] = T(f"{num(b, sv)} − {num(a, sv)} = {num(diff, sv)} s.", f"{num(b, sv)} − {num(a, sv)} = {num(diff, sv)} s.")
        return t
    if kind == "secconv":
        m, s = rng.randint(1, 3), rng.choice([5, 10, 15, 20, 25, 30, 40, 45, 50])
        tot = m * 60 + s
        t["question"] = T(f"{m} min {s} s = ? s", f"{m} min {s} s = ? s")
        t["shown"] = f"{m} min {s} s"
        t["options"], t["correct"] = options(rng, f"{tot} s", [f"{m * 100 + s} s", f"{m * 60} s", f"{tot + 60} s", f"{m + s} s", f"{tot - 10} s"], lambda: "")
        t["explain"] = T(f"{m} min = {m * 60} s, and {m * 60} + {s} = {tot} s.", f"{m} min = {m * 60} s, och {m * 60} + {s} = {tot} s.")
        return t

    # ---- åk 7–9: hastighet, sträcka och tid
    if kind == "speed":
        v = rng.choice([40, 45, 50, 60, 70, 75, 80, 90, 100])
        h = pick([0.5, 1.5, 2, 2.5, 3, 0.75])
        s = v * h
        t["shown"] = f"{num(s, sv)} km, {hm(h * 60, sv)}"
        t["question"] = T(f"A car drives {num(s, False)} km in {hm(h * 60, False)}. What is its average speed?",
                          f"En bil kör {num(s, True)} km på {hm(h * 60, True)}. Vilken medelhastighet har den?")
        mins = round(h * 60)
        read_as = mins // 60 + (mins % 60) / 100       # "1 h 30 min" läst som 1,3 h – det vanliga felet
        wrong = [num(s * h, sv), num(round(s / read_as), sv), num(v + 10, sv), num(v - 10, sv), num(s, sv)]
        t["options"], t["correct"] = options(rng, f"{num(v, sv)} km/h", [w + " km/h" for w in wrong if w != num(v, sv)], lambda: f"{num(v + rng.choice([-15, 15, 20]), sv)} km/h")
        t["explain"] = T(f"speed = distance / time = {num(s, sv)} km / {num(h, sv)} h = {num(v, sv)} km/h.",
                         f"hastighet = sträcka / tid = {num(s, sv)} km / {num(h, sv)} h = {num(v, sv)} km/h.")
        return t
    if kind == "distance":
        v = rng.choice([12, 15, 18, 24, 30, 36, 45, 60, 90])
        mins = rng.choice([10, 20, 30, 40, 45, 15])
        s = v * mins / 60
        t["shown"] = f"{v} km/h, {mins} min"
        t["question"] = T(f"You cycle at {v} km/h for {mins} minutes. How far do you go?", f"Du cyklar {v} km/h i {mins} minuter. Hur långt kommer du?")
        wrong = [num(v * mins, sv), num(v * mins / 100, sv), num(s * 2, sv), num(s + 3, sv)]
        t["options"], t["correct"] = options(rng, f"{num(s, sv)} km", [w + " km" for w in wrong if w != num(s, sv)], lambda: f"{num(s + rng.choice([-2, 2, 5]), sv)} km")
        t["explain"] = T(f"{mins} min = {mins}/60 h. distance = {v} km/h · {mins}/60 h = {num(s, sv)} km.",
                         f"{mins} min = {mins}/60 h. sträcka = {v} km/h · {mins}/60 h = {num(s, sv)} km.")
        return t
    if kind == "traveltime":
        v = rng.choice([40, 60, 80, 90, 100, 120])
        mins = rng.choice([15, 30, 45, 90, 75, 150])
        s = v * mins / 60
        while s != int(s):
            mins = rng.choice([30, 90, 150])
            s = v * mins / 60
        t["shown"] = f"{num(s, sv)} km, {v} km/h"
        t["question"] = T(f"How long does it take to drive {num(s, False)} km at {v} km/h?", f"Hur lång tid tar det att köra {num(s, True)} km i {v} km/h?")
        h = mins / 60
        naive = int(h) * 60 + round((h - int(h)) * 100)   # 1,5 h → 1 h 50 min
        wrong = [hm(naive, sv) if naive != mins else "", hm(mins + 15, sv), hm(mins - 15, sv) if mins > 15 else "", hm(round(v / s * 60), sv) if s else ""]
        t["options"], t["correct"] = options(rng, hm(mins, sv), [w for w in wrong if w and w != "0 min"], lambda: hm(mins + rng.choice([-10, 10, 30]), sv))
        t["explain"] = T(f"time = distance / speed = {num(s, sv)} / {v} h = {num(h, sv)} h = {hm(mins, sv)}.",
                         f"tid = sträcka / hastighet = {num(s, sv)} / {v} h = {num(h, sv)} h = {hm(mins, sv)}.")
        return t
    if kind == "kmh":
        if rng.random() < 0.5:
            ms_ = rng.choice([5, 10, 15, 20, 25, 30])
            ans = num(ms_ * 3.6, sv)
            t["question"], t["shown"] = f"{ms_} m/s = ? km/h", f"{ms_} m/s"
            wrong = [num(ms_ * 60, sv), num(ms_ / 3.6, sv), num(ms_ * 6, sv), num(ms_ * 36, sv)]
            t["options"], t["correct"] = options(rng, ans + " km/h", [w + " km/h" for w in wrong if w != ans], lambda: "")
            t["explain"] = T(f"1 m/s = 3600 m per hour = 3.6 km/h, so {ms_} m/s = {ans} km/h.",
                             f"1 m/s = 3 600 m i timmen = 3,6 km/h, så {ms_} m/s = {ans} km/h.")
        else:
            kmh = rng.choice([18, 36, 54, 72, 90, 108])
            ans = num(kmh / 3.6, sv)
            t["question"], t["shown"] = f"{kmh} km/h = ? m/s", f"{kmh} km/h"
            wrong = [num(kmh * 3.6, sv), num(kmh / 60, sv), num(kmh / 6, sv), num(kmh / 2, sv)]
            t["options"], t["correct"] = options(rng, ans + " m/s", [w + " m/s" for w in wrong if w != ans], lambda: "")
            t["explain"] = T(f"Divide by 3.6: {kmh} / 3.6 = {ans} m/s.", f"Dela med 3,6: {kmh} / 3,6 = {ans} m/s.")
        return t
    if kind == "hdec":
        h = pick([1.2, 2.4, 0.6, 1.75, 3.25, 2.1, 0.9, 1.4])
        mins = round(h * 60)
        t["question"] = T(f"The calculator says {num(h, False)} h. How many hours and minutes is that?",
                          f"Räknaren visar {num(h, True)} h. Hur många timmar och minuter är det?")
        t["shown"] = f"{num(h, sv)} h"
        whole, frac = int(h), round(h - int(h), 2)
        naive = whole * 60 + int(round(frac * 100)) if frac * 100 < 60 else 0   # 2,4 h → 2 h 40 min
        wrong = [hm(naive, sv) if naive and naive != mins else "", hm(whole * 60 + int(round(frac * 10)), sv), hm(mins + 6, sv), hm(mins - 6, sv)]
        t["options"], t["correct"] = options(rng, hm(mins, sv), [w for w in wrong if w and w != "0 min" and w != hm(mins, sv)], lambda: hm(mins + rng.choice([-12, 12, 18]), sv))
        t["explain"] = T(f"{num(frac, sv)} h = {num(frac, sv)} · 60 min = {round(frac * 60)} min, so {num(h, sv)} h = {hm(mins, sv)}.",
                         f"{num(frac, sv)} h = {num(frac, sv)} · 60 min = {round(frac * 60)} min, så {num(h, sv)} h = {hm(mins, sv)}.")
        return t
    raise KeyError(kind)
