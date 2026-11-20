"""Klockan – logiken, utan fönster (provas i tests/test-klockan.py).

  tiden i ord        say(h, m, sv)      "kvart över tre" / "quarter past three"
  digital tid        digital(h, m)      "15:15"
  visarnas vinklar   angles(h, m), minute_from_angle(), hour_from_angle(), drag_minute(), nearest_hand()
  uppgifter          make_task(level, rng, sv), make_round(level, rng, sv)
  framsteg           Progress(path)     stjärnor per nivå, sparas som JSON

Nivåerna följer grundskolans kursplan i matematik (Lgr22, centralt innehåll om tid – citaten i skola.LGR):
  åk 1–3  hel timme → halv → kvart → fem minuter → minuter → dygnets 24 timmar → hur lång tid → tidsenheter → kalendern
  åk 4–6  enhetsbyten → tidtabeller och datum → sekunder och tiondelar
  åk 7–9  hastighet, sträcka och tid
Urtavlan och 24-timmarsklockan finns här; resten av tiden (enheter, kalender, enhetsbyten, km/h) i skola.py."""
import json
import os
import random

import skola

# ------------------------------------------------------------------ tal i ord
_SV = ["noll", "ett", "två", "tre", "fyra", "fem", "sex", "sju", "åtta", "nio", "tio", "elva", "tolv", "tretton",
       "fjorton", "femton", "sexton", "sjutton", "arton", "nitton"]
_EN = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve",
       "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"]


def number_sv(n, count=False):
    """1–29 i ord. count=True: "en" i stället för "ett" (en minut, tjugoen minuter)"""
    if n >= 20:
        rest = n - 20
        return "tjugo" + ("" if rest == 0 else ("en" if rest == 1 and count else _SV[rest]))
    return "en" if n == 1 and count else _SV[n]


def number_en(n):
    if n >= 20:
        return "twenty" + ("" if n == 20 else "-" + _EN[n - 20])
    return _EN[n]


def hour12(h):
    """timmen på urtavlan: 1–12"""
    return h % 12 or 12


# ------------------------------------------------------------------ tiden i ord
def _minutes_sv(x):
    if x == 15:
        return "kvart"
    if x % 5 == 0:
        return number_sv(x)
    return number_sv(x, count=True) + (" minut" if x == 1 else " minuter")


def say_sv(h, m):
    """så säger man tiden på svenska: 15:20 → "tjugo över tre", 15:25 → "fem i halv fyra" """
    here, nxt = number_sv(hour12(h)), number_sv(hour12(h + 1))
    if m == 0:
        return here
    if m < 25:
        return f"{_minutes_sv(m)} över {here}"
    if m < 30:
        return f"{_minutes_sv(30 - m)} i halv {nxt}"
    if m == 30:
        return f"halv {nxt}"
    if m <= 35:
        return f"{_minutes_sv(m - 30)} över halv {nxt}"
    return f"{_minutes_sv(60 - m)} i {nxt}"


def _minutes_en(x):
    if x == 15:
        return "quarter"
    if x % 5 == 0:
        return number_en(x)
    return number_en(x) + (" minute" if x == 1 else " minutes")


def say_en(h, m):
    here, nxt = number_en(hour12(h)), number_en(hour12(h + 1))
    if m == 0:
        return f"{here} o'clock"
    if m == 30:
        return f"half past {here}"
    if m < 30:
        return f"{_minutes_en(m)} past {here}"
    return f"{_minutes_en(60 - m)} to {nxt}"


def say(h, m, sv):
    return say_sv(h, m) if sv else say_en(h, m)


def digital(h, m):
    return f"{h % 24:02d}:{m:02d}"


def part_of_day(h, sv):
    """förmiddag/eftermiddag … – så att en analog klocka kan bli en tid på dygnet"""
    h %= 24
    if h < 5:
        return "på natten" if sv else "at night"
    if h < 10:
        return "på morgonen" if sv else "in the morning"
    if h < 12:
        return "på förmiddagen" if sv else "before noon"
    if h < 18:
        return "på eftermiddagen" if sv else "in the afternoon"
    return "på kvällen" if sv else "in the evening"


def duration(minutes, sv):
    """85 → "1 tim 25 min" / "1 h 25 min" """
    h, m = divmod(minutes, 60)
    hour = "tim" if sv else "h"
    if h and m:
        return f"{h} {hour} {m} min"
    return f"{h} {hour}" if h else f"{m} min"


# ------------------------------------------------------------------ visarna
def angles(h, m):
    """(timvisaren, minutvisaren) i grader medurs från tolvan; timvisaren går med minuterna"""
    return (h % 12) * 30 + m * 0.5, m * 6.0


def minute_from_angle(deg, step=1):
    """minuten som visaren pekar på, avrundad till nivåns steg (60 = bara hel timme)"""
    step = max(1, min(60, step))
    return int(round((deg % 360) / 6.0 / step) * step) % 60


def hour_from_angle(deg, m):
    """timmen (1–12) när timvisaren dras till vinkeln – minuterna behålls"""
    return hour12(int(round(((deg % 360) - m * 0.5) / 30.0)))


def drag_minute(h, m, new_m):
    """minutvisaren dras från m till new_m: förbi tolvan framåt = nästa timme, bakåt = förra"""
    if m >= 45 and new_m < 15:
        h += 1
    elif m < 15 and new_m >= 45:
        h -= 1
    return hour12(h), new_m


def step_time(h, m, minutes):
    """flytta klockan ett antal minuter (knapparna + och −); timmen 1–12"""
    total = (hour12(h) % 12) * 60 + m + minutes
    total %= 12 * 60
    return hour12(total // 60), total % 60


def _diff(a, b):
    d = abs(a - b) % 360
    return min(d, 360 - d)


def nearest_hand(deg, h, m, radius=1.0):
    """vilken visare som greppas: nära mitten bara timvisaren (den är kort), annars den som ligger närmast i vinkel"""
    ha, ma = angles(h, m)
    if radius < 0.45 and _diff(deg, ha) <= _diff(deg, ma) + 20:
        return "hour"
    return "hour" if _diff(deg, ha) + 8 < _diff(deg, ma) else "minute"


def hands_hint(h, m, sv):
    """var visarna står, i ord – förklaringen efter ett fel svar"""
    here, nxt = hour12(h), hour12(h + 1)
    if sv:
        short = f"Den korta visaren pekar på {here}" if m == 0 else f"Den korta visaren står mellan {here} och {nxt}"
        long = f"den långa på {m // 5 or 12}" if m % 5 == 0 else f"den långa på minutstreck {m}"
        return f"{short}, {long} ({m} min)." if m else f"{short}, {long}."
    short = f"The short hand points at {here}" if m == 0 else f"The short hand is between {here} and {nxt}"
    long = f"the long hand at {m // 5 or 12}" if m % 5 == 0 else f"the long hand at minute mark {m}"
    return f"{short}, {long} ({m} min)." if m else f"{short}, {long}."


def same_time(h1, m1, h2, m2):
    """samma läge på urtavlan (03:15 och 15:15 ser likadana ut)"""
    return h1 % 12 == h2 % 12 and m1 == m2


# ------------------------------------------------------------------ nivåer
LEVELS = [
    {"id": "hel", "stage": "1-3", "step": 60, "kinds": ["read", "read", "set"],
     "name": ("Whole hours", "Hel timme"),
     "about": ("The long hand points at 12. The short hand shows the hour.",
               "Den långa visaren pekar på 12. Den korta visar timmen.")},
    {"id": "halv", "stage": "1-3", "step": 30, "kinds": ["read", "read", "set"],
     "name": ("Half hours", "Halv"),
     "about": ("The long hand points at 6 – half past.", "Den långa visaren pekar på 6. Halv fyra är en halvtimme före fyra.")},
    {"id": "kvart", "stage": "1-3", "step": 15, "kinds": ["read", "read", "set", "digital"],
     "name": ("Quarter past and quarter to", "Kvart över och kvart i"),
     "about": ("The long hand at 3 is quarter past, at 9 quarter to.", "Lång visare på 3 är kvart över, på 9 kvart i.")},
    {"id": "fem", "stage": "1-3", "step": 5, "kinds": ["read", "read", "set", "digital", "words"],
     "name": ("Five-minute steps", "Fem minuter i taget"),
     "about": ("Each number is five minutes for the long hand.", "Varje siffra är fem minuter för den långa visaren.")},
    {"id": "minut", "stage": "1-3", "step": 1, "kinds": ["read", "set", "digital", "words"],
     "name": ("Every minute", "Varje minut"),
     "about": ("The small marks are single minutes.", "De små strecken är enskilda minuter.")},
    {"id": "dygn", "stage": "1-3", "step": 5, "kinds": ["day", "day", "words24", "set24"],
     "name": ("24 hours – morning and afternoon", "Dygnets 24 timmar"),
     "about": ("After noon the digital clock keeps counting: 13, 14, 15 …",
               "Efter klockan tolv fortsätter den digitala klockan: 13, 14, 15 …")},
    {"id": "tid", "stage": "1-3", "step": 5, "kinds": ["later", "later", "earlier", "length"],
     "name": ("How long does it take?", "Hur lång tid?"),
     "about": ("Count on from one time to another.", "Räkna vidare från en tid till en annan.")},
]
LEVELS += skola.LEVELS            # åk 1–3 (enheter, kalender), åk 4–6, åk 7–9 – i ordning efter stadium
LEVELS.sort(key=lambda lv: ["1-3", "4-6", "7-9"].index(lv["stage"]))
LEVEL_IDS = [lv["id"] for lv in LEVELS]
STAGES = skola.STAGES
LGR = skola.LGR
ROUND = 10


def level(level_id):
    for lv in LEVELS:
        if lv["id"] == level_id:
            return lv
    raise KeyError(level_id)


def random_time(rng, step, afternoon=False):
    """en tid som passar nivån; timmen 1–12 (afternoon: 13–23)"""
    h = rng.randint(13, 23) if afternoon else rng.randint(1, 12)
    if step >= 60:
        return h, 0
    if step == 30:
        return h, 30
    if step == 15:
        return h, rng.choice([15, 45, 15, 45, 0, 30])
    return h, rng.randrange(0, 60, step)


def _confusions(h, m):
    """tider som barn ofta blandar ihop med (h, m): visarna förväxlade, fel timme vid "i" och "halv", speglade minuter"""
    out = []
    if m % 5 == 0:
        out.append((m // 5 or 12, (hour12(h) % 12) * 5))   # långa och korta visaren förväxlade
    out += [(h + 1, m), (h - 1, m)]                        # "halv fyra" läst som 4:30, "kvart i" på fel timme
    if m not in (0, 30):
        out.append((h, 60 - m))                            # över ↔ i
        out.append((h + 1, 60 - m))
    out += [(h, (m + 30) % 60), (h, 0), (h, 30)]
    return [(hour12(a), b % 60) for a, b in out]


def _fit(times, step):
    """bara tider som hör till nivån (på nivån Hel timme finns inga halvtimmar bland svaren)"""
    unit = 60 if step >= 60 else step
    return [(a, b) for a, b in times if b % unit == 0]


def _options(rng, correct, wrong_candidates, filler, n=4, must=()):
    """n olika svar med det rätta på en slumpad plats: först de som måste med, sedan förväxlingarna, sedan utfyllnad"""
    opts = [correct]
    pool = list(wrong_candidates)
    rng.shuffle(pool)
    for cand in list(must) + pool + [filler() for _ in range(40)]:
        if cand not in opts:
            opts.append(cand)
        if len(opts) == n:
            break
    if len(opts) < n:
        raise ValueError("för få svarsalternativ")
    rng.shuffle(opts)
    return opts, opts.index(correct)


def make_task(level_id, rng, sv=True, kind=None):
    """en uppgift som en ordlista: kind, h, m, show, shown, question, options, correct, target, step, explain"""
    lv = level(level_id)
    step = lv["step"]
    kind = kind or rng.choice(lv["kinds"])
    T = (lambda en, s: s) if sv else (lambda en, s: en)
    t = {"level": level_id, "kind": kind, "step": step, "show": "clock", "shown": "", "options": None, "correct": -1, "target": None, "hint": ""}

    if kind in skola.KINDS:   # tid utan urtavla: enheter, kalender, enhetsbyten, tidtabeller, km/h
        return skola.make(kind, rng, sv, t, _options)

    if kind in ("read", "digital", "words", "set"):
        h, m = random_time(rng, step)
        t["h"], t["m"] = h, m
        phrase = say(h, m, sv)
        if kind == "read":      # urtavla → ord
            t["question"] = T("What time is it?", "Vad är klockan?")
            t["options"], t["correct"] = _options(rng, phrase, [say(a, b, sv) for a, b in _fit(_confusions(h, m), step)],
                                                  lambda: say(*random_time(rng, step), sv))
        elif kind == "digital":  # urtavla → siffror
            t["question"] = T("Which digital clock shows the same time?", "Vilken digital klocka visar samma tid?")
            t["options"], t["correct"] = _options(rng, digital(h, m), [digital(a, b) for a, b in _fit(_confusions(h, m), step)],
                                                  lambda: digital(*random_time(rng, step)))
        elif kind == "words":    # siffror → ord
            t["show"], t["shown"] = "digital", digital(h, m)
            t["question"] = T("How do you say this time?", "Hur säger man den här tiden?")
            t["options"], t["correct"] = _options(rng, phrase, [say(a, b, sv) for a, b in _fit(_confusions(h, m), step)],
                                                  lambda: say(*random_time(rng, step), sv))
        else:                    # ord → ställ visarna
            t["target"] = (h, m)
            t["shown"] = phrase
            t["question"] = T("Set the clock to:", "Ställ klockan på:")
        t["explain"] = T(f"The time is {phrase} ({digital(h, m)}).", f"Klockan är {phrase} ({digital(h, m)}).")
        t["hint"] = hands_hint(h, m, sv)
        return t

    if kind in ("day", "words24", "set24"):
        h, m = random_time(rng, step, afternoon=rng.random() < 0.75)
        t["h"], t["m"] = h, m
        when = part_of_day(h, sv)
        phrase = say(h, m, sv)
        if kind == "day":        # urtavla + tid på dygnet → 24-timmarstid
            t["shown"] = when
            t["question"] = T(f"The clock looks like this {when}. What does the digital clock show?",
                              f"Så här ser klockan ut {when}. Vad visar den digitala klockan?")
            wrong = [digital((a % 12) + 12 if h >= 12 else a, b) for a, b in _fit(_confusions(h, m), step)]
            t["options"], t["correct"] = _options(rng, digital(h, m), wrong, lambda: digital(*random_time(rng, step, afternoon=h >= 12)),
                                                  must=[digital((h + 12) % 24, m)])
        elif kind == "words24":  # 24-timmarstid → ord
            t["show"], t["shown"] = "digital", digital(h, m)
            t["question"] = T("How do you say this time?", "Hur säger man den här tiden?")
            t["options"], t["correct"] = _options(rng, phrase, [say(a, b, sv) for a, b in _fit(_confusions(h, m), step)] + [say(h % 12 + 2, m, sv)],
                                                  lambda: say(*random_time(rng, step), sv))
        else:                    # 24-timmarstid → ställ visarna och välj förmiddag/eftermiddag
            t["target"] = (h, m)
            t["day"] = True      # urtavlan är likadan kl. 03:15 och 15:15 – tiden på dygnet väljs för sig (ChatGPT 20 nov)
            t["shown"] = digital(h, m)
            t["question"] = T("Set the clock, and choose before or after noon:", "Ställ klockan och välj förmiddag eller eftermiddag:")
        t["explain"] = T(f"{digital(h, m)} is {phrase} {when}.", f"{digital(h, m)} är {phrase} {when}.")
        t["hint"] = hands_hint(h, m, sv)
        return t

    # ---- hur lång tid: senare, tidigare, hur länge
    t["show"] = "digital"
    h, m = rng.randint(7, 20), rng.randrange(0, 60, 5)
    start = h * 60 + m
    span = rng.choice([10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 75, 90, 105, 120])
    fmt = lambda mins: digital((mins // 60) % 24, mins % 60)   # noqa: E731
    t["h"], t["m"] = h, m
    if kind == "later":
        end = start + span
        t["shown"] = fmt(start)
        t["question"] = T(f"What time is it in {duration(span, False)}?", f"Vad är klockan om {duration(span, True)}?")
        wrong = [fmt(start - span), fmt(end + 60), fmt(end - 60), fmt(end + 10), fmt(end - 10), fmt(end + 5), fmt(end - 5)]
        t["options"], t["correct"] = _options(rng, fmt(end), wrong, lambda: fmt(end + rng.choice([-30, -20, 20, 30, 40])))
        t["explain"] = T(f"{fmt(start)} + {duration(span, False)} = {fmt(end)}.", f"{fmt(start)} + {duration(span, True)} = {fmt(end)}.")
    elif kind == "earlier":
        end = start - span
        t["shown"] = fmt(start)
        t["question"] = T(f"What time was it {duration(span, False)} ago?", f"Vad var klockan för {duration(span, True)} sedan?")
        wrong = [fmt(start + span), fmt(end + 60), fmt(end - 60), fmt(end + 10), fmt(end - 10), fmt(end + 5), fmt(end - 5)]
        t["options"], t["correct"] = _options(rng, fmt(end), wrong, lambda: fmt(end + rng.choice([-30, -20, 20, 30, 40])))
        t["explain"] = T(f"{fmt(start)} − {duration(span, False)} = {fmt(end)}.", f"{fmt(start)} − {duration(span, True)} = {fmt(end)}.")
    else:   # length
        end = start + span
        t["shown"] = f"{fmt(start)} – {fmt(end)}"
        what = rng.choice([("The film", "Filmen"), ("The lesson", "Lektionen"), ("The football practice", "Fotbollsträningen"),
                           ("The bus ride", "Bussresan"), ("The break", "Rasten")])
        t["question"] = T(f"{what[0]} starts at {fmt(start)} and ends at {fmt(end)}. How long is it?",
                          f"{what[1]} börjar {fmt(start)} och slutar {fmt(end)}. Hur lång tid tar det?")
        wrong = [duration(x, sv) for x in (span + 60, span - 60, span + 10, span - 10, span + 5, span - 5, span + 30) if x > 0]
        t["options"], t["correct"] = _options(rng, duration(span, sv), wrong, lambda: duration(span + rng.choice([15, 20, 25, 40, 45]), sv))
        t["explain"] = T(f"From {fmt(start)} to {fmt(end)} is {duration(span, False)}.", f"Från {fmt(start)} till {fmt(end)} är det {duration(span, True)}.")
    return t


def make_round(level_id, rng=None, sv=True, n=ROUND):
    """en omgång: nivåns uppgiftstyper i tur och ordning (blandade), aldrig samma tid två gånger i rad"""
    rng = rng or random.Random()
    kinds = level(level_id)["kinds"]
    order = [kinds[i % len(kinds)] for i in range(n)]
    rng.shuffle(order)
    out, last = [], None
    for kind in order:
        for _ in range(20):
            t = make_task(level_id, rng, sv, kind)
            if (t["h"], t["m"]) != last:
                break
        last = (t["h"], t["m"])
        out.append(t)
    return out


def check(task, answer):
    """answer: index för flervalsuppgifter, (h, m) för ställ-uppgifter"""
    if task["target"] is not None:
        if task.get("day"):   # 24-timmarsnivån: visarna och förmiddag/eftermiddag ska stämma (svaret är 0–23)
            return answer[0] % 24 == task["target"][0] % 24 and answer[1] == task["target"][1]
        return same_time(answer[0], answer[1], task["target"][0], task["target"][1])
    return answer == task["correct"]


def stars(right, total=ROUND):
    """0–3 stjärnor för en omgång"""
    if total <= 0:
        return 0
    share = right / total
    return 3 if share >= 1 else 2 if share >= 0.8 else 1 if share >= 0.6 else 0


# ------------------------------------------------------------------ framsteg
class Progress:
    """stjärnor per nivå (bästa omgången) – en liten JSON-fil i användarens datamapp; en trasig fil ger en tom start"""

    def __init__(self, path):
        self.path = path
        self.stars = {}
        self.rounds = 0
        try:
            with open(path, encoding="utf-8") as f:
                d = json.load(f)
            if isinstance(d, dict):
                st = d.get("stars")
                # bara en ordbok räknas – {"stars": "hej"} gav AttributeError och programmet öppnades inte (ChatGPT 20 nov)
                for k, v in (st.items() if isinstance(st, dict) else ()):
                    if k in LEVEL_IDS and isinstance(v, int) and not isinstance(v, bool):
                        self.stars[k] = max(0, min(3, v))
                r = d.get("rounds", 0)
                self.rounds = r if isinstance(r, int) and not isinstance(r, bool) and r >= 0 else 0
        except (OSError, ValueError):
            pass

    def record(self, level_id, right, total=ROUND):
        """sparar omgången; returnerar (stjärnor nu, nytt rekord?)"""
        got = stars(right, total)
        best = got > self.stars.get(level_id, 0)
        if best:
            self.stars[level_id] = got
        self.rounds += 1
        self.save()
        return got, best

    def save(self):
        try:
            os.makedirs(os.path.dirname(self.path), exist_ok=True)
            tmp = self.path + ".tmp"
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump({"stars": self.stars, "rounds": self.rounds}, f)
            os.replace(tmp, self.path)
        except OSError:
            pass   # skrivskyddat eller fullt: programmet fungerar ändå, bara utan sparade stjärnor

    def total(self):
        return sum(self.stars.values())
