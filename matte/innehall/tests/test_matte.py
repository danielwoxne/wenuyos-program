"""Enhetstester – körs med  python3 -m pytest  eller  python3 -m unittest."""
import unittest
from datetime import date
from fractions import Fraction as F

from matte.uttryck import tolka, likvardiga, formatera, former, Uttrycksfel
from matte.kort import alla_kort, las_laroplan
from matte.uppgifter import NYA_TYPER, gor_uppgift, bedom
from matte.framsteg import Elev, BEHARSKAR, OVAR
from matte.rita import celler, figur
from matte.regelbok import bok_html


class Uttryck(unittest.TestCase):
    def test_former_ar_samma_tal(self):
        for s in ("1/2", "0,5", "0.5", "50 %", "50%", "2/4", "5/10", "1 - 1/2"):
            self.assertEqual(tolka(s), F(1, 2), s)

    def test_rakning(self):
        self.assertEqual(tolka("8,50 · 10"), 85)
        self.assertEqual(tolka("(1+2)*3"), 9)
        self.assertEqual(tolka("2^3"), 8)
        self.assertEqual(tolka("1 1/2"), F(3, 2))
        self.assertEqual(tolka("10:4"), F(5, 2))

    def test_fel(self):
        for s in ("", "hej", "1/0", "(1+2"):
            with self.assertRaises(Uttrycksfel):
                tolka(s)

    def test_formatera(self):
        self.assertEqual(former(F(3, 4)), {"brak": "3/4", "procent": "75 %", "decimal": "0,75"})
        self.assertEqual(formatera(F(1, 3)), "1/3")
        self.assertEqual(formatera(F(85)), "85")
        self.assertTrue(likvardiga("75%", "3/4"))


class Kort(unittest.TestCase):
    def test_korten_laddas_och_alla_mallar_fungerar(self):
        kort = alla_kort()
        self.assertGreaterEqual(len(kort), 3)
        for k in kort.values():
            for m in k.mallar:
                for fro in range(30):
                    u = gor_uppgift(k, m, fro=fro)
                    svar = u.facit_text
                    if m.svarstyp in NYA_TYPER:            # kryssa, para, mala, rita: svaret är en sträng per sak
                        svar = u.facit_svar + ([""] * (len(celler(u.figur)) - len(u.facit_svar)) if m.svarstyp == "mala" else [])
                    self.assertTrue(bedom(u, svar).ratt, (k.id, u.fraga, u.facit_text))
                    if u.figur:
                        figur(u.figur)

    def test_feltanke_ger_forklaring(self):
        k = alla_kort()["brak-delar"]
        u = gor_uppgift(k, k.mallar[0], fro=7)
        n, kk = u.varden["n"], u.varden["k"]
        b = bedom(u, f"{n}/{kk}")
        self.assertFalse(b.ratt); self.assertTrue(b.feltanke)

    def test_laroplanen_pekar_pa_kort_som_finns(self):
        kort = alla_kort()
        for m in las_laroplan():
            for kid in m.kort:
                self.assertIn(kid, kort, m.id)

    def test_regelboken_blir_html(self):
        h = bok_html(list(alla_kort().values()))
        self.assertIn("<svg", h); self.assertIn("Bråk", h)


class Framsteg(unittest.TestCase):
    def test_beharskar_och_repetition(self):
        e = Elev("test"); e.sett_kort("x")
        self.assertEqual(e.status("x").lage, OVAR)
        d = date(2026, 11, 8)
        for _ in range(4):
            e.svarade("x", True, idag=d)
        self.assertEqual(e.status("x").lage, BEHARSKAR)
        self.assertEqual(e.att_repetera(d), [])
        self.assertEqual(e.att_repetera(date(2026, 11, 9)), ["x"])
        e.svarade("x", False, idag=date(2026, 11, 9))
        self.assertEqual(e.status("x").lage, BEHARSKAR)  # fel sänker intervallet, inte läget


if __name__ == "__main__":
    unittest.main()


class Algebra(unittest.TestCase):
    def test_likvardiga(self):
        from matte.algebra import likvardiga_uttryck, los_linjar, skriv, tolka_uttryck
        self.assertTrue(likvardiga_uttryck("2x+3x", "5x"))
        self.assertTrue(likvardiga_uttryck("(x+1)(x+1)", "x²+2x+1"))
        self.assertTrue(likvardiga_uttryck("3(a+b)", "3a+3b"))
        self.assertFalse(likvardiga_uttryck("2x+3", "5x"))
        self.assertEqual(los_linjar("2x+3=11"), 4)
        self.assertEqual(skriv(tolka_uttryck("3x^2 - x + 1 - 2x^2")), "x² − x + 1")


class Svarstyper(unittest.TestCase):
    def test_val_tabell_uttryck(self):
        from matte.kort import Kort, Mall
        k = Kort("t", "t", "algebra")
        u = gor_uppgift(k, Mall("Förenkla {a}x + {b}x", "{a+b}x", {"a": "2..5", "b": "2..5"}, svarstyp="uttryck"), fro=1)
        self.assertTrue(bedom(u, f"x*{u.varden['a'] + u.varden['b']}").ratt)
        u = gor_uppgift(k, Mall("Störst?", "ja", {}, alternativ=["ja", "nej"], svarstyp="val"))
        self.assertTrue(bedom(u, "Ja").ratt); self.assertFalse(bedom(u, "nej").ratt)
        u = gor_uppgift(k, Mall("Tre former", ["{k*10} %", "0,{k}"], {"k": "1..9"}, falt=["Procent", "Decimal"], svarstyp="tabell"), fro=2)
        self.assertTrue(bedom(u, [f"{u.varden['k']}0 %", f"{u.varden['k']}/10"]).ratt)
        self.assertEqual(bedom(u, ["1 %", "0,1"]).fel_falt, [0] if u.varden["k"] == 1 else [0, 1])


class Placering(unittest.TestCase):
    def test_hittar_startpunkt(self):
        from matte.placering import placera_alla
        kort = alla_kort(); lp = las_laroplan(); e = Elev("prov")
        for p in placera_alla(e, lp, kort):
            kan = min(3, len(p.kort_ids))
            while not p.klar:
                k, _u = p.nasta(); p.svara(p.kort_ids.index(k.id) < kan)
            self.assertEqual(p.start, kan, p.omrade)
            self.assertLessEqual(p.fragor, 6)
            p.tillampa(e, date(2026, 11, 8))
        self.assertGreater(e.sammanfattning()["beharskar"], 5)


class Kontroll(unittest.TestCase):
    def test_check_ar_gron(self):
        from matte.kontroll import kontrollera
        self.assertEqual(kontrollera(fron=15, tyst=True), [])


class PunktOchLinje(unittest.TestCase):
    def test_snapp(self):
        from fractions import Fraction
        from matte.rita import Rutnat
        r = Rutnat({"typ": "tallinje", "fran": 0, "till": 1, "steg": "0,1"})
        self.assertEqual(r.snapp(*r.pixel(Fraction(7, 10))), Fraction(7, 10))
        k = Rutnat({"typ": "koordinatsystem", "xmin": -4, "xmax": 4, "ymin": -4, "ymax": 4})
        self.assertEqual(k.snapp(*k.pixel((-3, 2))), (-3, 2))

    def test_bedomning_av_punkt_och_linje(self):
        kort = alla_kort()
        k = kort["linjara-funktioner"]
        m = [m for m in k.mallar if m.svarstyp == "linje"][0]
        u = gor_uppgift(k, m, fro=4)
        self.assertTrue(bedom(u, [str(u.varden["k"]), str(u.varden["m"])]).ratt)
        self.assertTrue(bedom(u, [str(u.varden["m"]), str(u.varden["k"])]).feltanke or u.varden["k"] == u.varden["m"])
        self.assertTrue(bedom(u, ["lodrät", "lodrät"]).otolkbart)
        m = [m for m in k.mallar if m.svarstyp == "punkt"][0]
        u = gor_uppgift(k, m, fro=4)
        self.assertTrue(bedom(u, [str(u.varden["x"]), str(u.varden["y"])]).ratt)
        self.assertTrue(bedom(u, [str(u.varden["y"]), str(u.varden["x"])]).feltanke)


class Scener(unittest.TestCase):
    def test_markera_rattas_pa_vilka_inte_hur_manga(self):
        k = alla_kort()["talkamrater-10"]
        m = [m for m in k.mallar if m.svarstyp == "markera"][0]
        u = gor_uppgift(k, m, fro=1)
        self.assertTrue(bedom(u, "gul,gul,gul").ratt)
        self.assertTrue(bedom(u, "gul, gul ,gul").ratt)
        self.assertFalse(bedom(u, "blå,blå,blå").ratt)              # tre saker – men fel saker
        self.assertIn("2", bedom(u, "gul,gul").text)                 # säger hur många som markerades

    def test_valj(self):
        k = alla_kort()["rakna-antal"]
        m = [m for m in k.mallar if m.svarstyp == "valj"][0]
        u = gor_uppgift(k, m, fro=1)
        self.assertTrue(bedom(u, "höger").ratt)
        self.assertFalse(bedom(u, "mitten").ratt)
        self.assertTrue(bedom(u, "").otolkbart)

    def test_klickytor_ligger_i_bilden(self):
        from matte.rita import celler, figur
        for k in alla_kort().values():
            for m in k.mallar:
                if m.figur and m.figur.get("typ") == "scen":
                    f = figur(m.figur)
                    for x, y, b, h in celler(m.figur):
                        self.assertTrue(0 <= x and x + b <= f.bredd + 1 and 0 <= y and y + h <= f.hojd + 1, k.id)


class Lekläge(unittest.TestCase):
    def test_uppläsning_gör_tecken_till_ord(self):
        from matte.tal import som_ord
        t = som_ord("3 · 4 = ?")
        self.assertIn("gånger", t); self.assertIn("lika med", t)
        self.assertIn("delat med", som_ord("3/4"))

    def test_förskolekorten_har_klickbara_uppgifter(self):
        from matte.fonster_konstanter import INTERAKTIVA
        kort = alla_kort()
        for kid in ("forskola-rakna", "forskola-flest", "forskola-siffror", "forskola-former", "forskola-monster"):
            self.assertTrue(all(m.svarstyp in INTERAKTIVA for m in kort[kid].mallar), kid)

    def test_lekläget_sparas(self):
        import tempfile, os
        with tempfile.TemporaryDirectory() as d:
            os.environ["XDG_DATA_HOME"] = d
            e = Elev("lek"); e.lekläge = True; e.spara()
            self.assertTrue(Elev.ladda("lek").lekläge)
            del os.environ["XDG_DATA_HOME"]


class Anpassning(unittest.TestCase):
    def test_feltanke_tranas_och_slapper(self):
        import random
        from matte import adaptiv
        from matte.uppgifter import feltanke_id
        kort = alla_kort(); k = kort["brak-delar"]
        m0 = k.mallar[0]
        e = Elev("anp")
        for fro in (1, 2):                                  # vänder på bråket två gånger
            u = gor_uppgift(k, m0, fro=fro)
            b = bedom(u, f"{u.varden['n']}/{u.varden['k']}")
            self.assertTrue(b.feltanke)
            e.svarade(k.id, False, feltanke_id(u, b), mall_index=u.mall_index)
        s = e.status(k.id)
        self.assertEqual(adaptiv.feltanke_att_trana(s)[0], 0)
        rnd = random.Random(0)
        valda = [adaptiv.valj_mall(k, s, rnd=rnd)[0] for _ in range(200)]
        self.assertGreater(valda.count(m0) / 200, 0.5)     # mallen med feltanken kommer oftare
        e.svarade(k.id, True, mall_index=0); e.svarade(k.id, True, mall_index=0)
        self.assertIsNone(adaptiv.feltanke_att_trana(e.status(k.id)))   # två rätt – den släpper

    def test_ga_tillbaka(self):
        from matte import adaptiv
        kort = alla_kort(); k = kort["brak-procent-decimal"]
        e = Elev("anp2")
        for r in (0, 1, 0, 0, 1, 0):
            e.svarade(k.id, bool(r))
        self.assertEqual(adaptiv.ga_tillbaka(k, e, kort).id, "brak-delar")
        e2 = Elev("anp3"); e2.svarade(k.id, False)
        self.assertIsNone(adaptiv.ga_tillbaka(k, e2, kort))


class Gymnasiet(unittest.TestCase):
    def test_rotter_potenser_trig_log(self):
        self.assertEqual(tolka("√(9/4)"), F(3, 2))
        self.assertEqual(tolka("16^(3/4)"), 8)
        self.assertEqual(tolka("2^-3"), F(1, 8))
        self.assertEqual(tolka("(-8)^(1/3)"), -2)
        self.assertEqual(tolka("sin(30)"), F(1, 2))
        self.assertEqual(tolka("lg(1000)"), 3)
        self.assertAlmostEqual(float(tolka("sqrt(2)")), 2 ** 0.5, places=8)
        self.assertAlmostEqual(float(tolka("π")), 3.14159265, places=7)

    def test_tolerans_och_oordnade_svar(self):
        kort = alla_kort()
        k = kort["gy-trigonometri"]; m = k.mallar[1]
        u = gor_uppgift(k, m, fro=3)
        v = float(u.facit[0])
        self.assertTrue(bedom(u, f"{v + 0.04:.2f}".replace(".", ",")).ratt)
        self.assertFalse(bedom(u, f"{v + 0.2:.2f}".replace(".", ",")).ratt)
        k = kort["gy-andragradsekvationer"]; m = k.mallar[1]
        u = gor_uppgift(k, m, fro=3)
        a, b = u.facit_text
        self.assertTrue(bedom(u, [a, b]).ratt); self.assertTrue(bedom(u, [b, a]).ratt)

    def test_mallhjalp(self):
        from matte.uppgifter import _tecken, _tusental
        self.assertEqual(_tecken(-3), "− 3"); self.assertEqual(_tecken(4), "+ 4")
        self.assertEqual(_tusental(56000000), "56 000 000")


class LarareOchArbetsblad(unittest.TestCase):
    def test_filnamn(self):
        from matte.framsteg import filnamn
        self.assertEqual(filnamn("Ali S."), "ali-s")
        self.assertEqual(filnamn("Åsa Öberg"), "åsa-öberg")
        self.assertEqual(filnamn("   "), "elev")

    def test_arbetsblad_med_facit_och_samma_nummer_samma_blad(self):
        from matte.arbetsblad import arbetsblad
        kort = alla_kort()
        a = arbetsblad([kort["brak-delar"], kort["gy-trigonometri"]], 3, fro=4711)
        b = arbetsblad([kort["brak-delar"], kort["gy-trigonometri"]], 3, fro=4711)
        self.assertEqual(a, b)
        self.assertIn("Facit – blad 4711", a)
        self.assertEqual(a.count('class="uppg"'), 6)

    def test_oversikt_samlar_feltankar(self):
        import tempfile, os
        from matte.oversikt import sammanstall
        from matte.uppgifter import feltanke_id
        with tempfile.TemporaryDirectory() as d:
            os.environ["XDG_DATA_HOME"] = d
            try:
                kort = alla_kort(); k = kort["brak-delar"]
                for namn in ("ali", "bea"):
                    e = Elev(namn); u = gor_uppgift(k, k.mallar[0], fro=2)
                    b = bedom(u, f"{u.varden['n']}/{u.varden['k']}")
                    e.svarade(k.id, False, feltanke_id(u, b), mall_index=0); e.spara()
                rader, grupp = sammanstall()
                self.assertEqual(len(rader), 2)
                self.assertEqual(len(grupp[0].elever), 2)
                self.assertIn("vänt", grupp[0].text)
                self.assertNotIn("{", grupp[0].text)
            finally:
                del os.environ["XDG_DATA_HOME"]


class WenuyOSRegler(unittest.TestCase):
    def test_data_i_programmets_id_mapp_och_flytt_fran_gammal(self):
        import tempfile, os
        from pathlib import Path
        from matte import framsteg
        with tempfile.TemporaryDirectory() as d:
            os.environ["XDG_DATA_HOME"] = d
            try:
                gammal = Path(d) / "matte"; gammal.mkdir()
                (gammal / "ali.json").write_text('{"namn": "ali", "kort": {}, "regelbok": ["brak-delar"]}', encoding="utf-8")
                e = Elev.ladda("ali")
                self.assertEqual(e.regelbok, ["brak-delar"])
                self.assertTrue((Path(d) / "se.wenuyos.app.matte" / "ali.json").exists())
                self.assertFalse(gammal.exists())
            finally:
                del os.environ["XDG_DATA_HOME"]

    def test_id_ar_samma_overallt(self):
        from pathlib import Path
        import tomllib
        from matte.framsteg import APP_ID
        rot = Path(__file__).resolve().parent.parent
        desk = (rot / "data" / f"{APP_ID}.desktop").read_text(encoding="utf-8")
        self.assertIn(f"StartupWMClass={APP_ID}", desk)
        self.assertIn(f"Icon={APP_ID}", desk)
        self.assertTrue((rot / "data" / f"{APP_ID}.svg").exists())
        self.assertIn(f'APP_ID = "{APP_ID}"', (rot / "matte" / "skal.py").read_text(encoding="utf-8"))
        app = tomllib.loads((rot / "wenuyos-app.toml").read_text(encoding="utf-8"))
        self.assertEqual(app["version"], (rot / "VERSION").read_text().strip())


class LostaExempelOchLosningar(unittest.TestCase):
    """0.4: lösta exempel med lucka (steg), "så här kan man tänka" (losning) och innehållsmåtten."""

    def test_steg_och_losning_fylls_i(self):
        k = alla_kort()["addition-tiotalsovergang"]
        m = next(m for m in k.mallar if m.steg)
        u = gor_uppgift(k, m, fro=5)
        a, b = u.varden["a"], u.varden["b"]
        self.assertIn(f"{a} + {b}", u.steg[0])
        self.assertTrue(u.steg[-1].endswith("?"))
        self.assertTrue(u.losning and str(a + b) in u.losning[-1])
        self.assertFalse(any("{" in s for s in u.steg + u.losning))

    def test_listuttryck_i_mallar_nar_variablerna(self):
        from matte.uppgifter import _fyll
        self.assertEqual(_fyll("{', '.join(str(10*t+i) for i in range(1, 3))}", {"t": 4}), "41, 42")

    def test_misslyckad_platshallare_fangas(self):
        from matte.uppgifter import MISSLYCKADE, _fyll
        MISSLYCKADE.clear()
        _fyll("{finns_inte + 1}", {})
        self.assertEqual(MISSLYCKADE, ["finns_inte + 1"])

    def test_decimaltecken_och_matematik_i_mallar(self):
        from matte.uppgifter import _fyll
        self.assertEqual(_fyll("{t(-2.5)}", {}), "− 2,5")
        self.assertEqual(_fyll("{round(5*sin(30), 2)}", {}), "2,5")
        self.assertEqual(_fyll("{round(atand(1))}", {}), 45)
        self.assertEqual(_fyll("{lg(1000)}", {}), 3)

    def test_falt_fylls_i(self):
        k = alla_kort()["talkamrater-10"]
        m = next(m for m in k.mallar if m.svarstyp == "tabell")
        u = gor_uppgift(k, m, fro=1)
        self.assertTrue(all("{" not in f and "+ ? = 10" in f for f in u.falt))

    def test_alla_kort_har_exempel_och_manga_uppgifter(self):
        from matte.kontroll import innehall
        self.assertEqual(innehall(alla_kort()), [])

    def test_alla_mal_har_kort(self):
        self.assertTrue(all(m.kort for m in las_laroplan()))

    def test_arbetsbladet_visar_steg_och_losning(self):
        from matte.arbetsblad import arbetsblad
        k = alla_kort()["ekvationer-enkla"]
        text = arbetsblad([k], antal=len(k.mallar), fro=3)
        self.assertIn('class="steg"', text)
        self.assertIn('class="losning"', text)
