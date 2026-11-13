// Kärntesterna (utan GTK): cmake --build build --target matte-test && ./build/matte-test tester
#include "Algebra.h"
#include <glib/gstdio.h>
#include "Bedom.h"
#include "Data.h"
#include "Penntryck.h"
#include "PapperLagring.h"
#include "VisuellNiva.h"
#include "Framsteg.h"
#include "Json.h"
#include "Rita.h"
#include "Tal.h"
#include "Utskrift.h"
#include "Flytt.h"
#include "Framsteg.h"
#include "Logg.h"

#include <cmath>
#include <cstring>
#include <map>
#include <utility>
#include <cstdio>
#include <algorithm>
#include <set>
#include <sstream>
#include <string>

using namespace matte;

static int fel = 0, ok = 0;
#define KOLLA(villkor, ...) do { if (villkor) ++ok; else { ++fel; fprintf(stderr, "FEL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

// Samma värden som Python-versionen för alla facit och svarsalternativ (tester/fall.json)
static void testaTolkning(const std::string& mapp) {
    Json fall = Json::lasFil(mapp + "/fall.json");
    KOLLA(fall.storlek() > 100, "fall.json saknas eller är tom");
    int skillnad = 0;
    for (auto& f : fall.element()) {
        std::string in = f["in"].str();
        std::string tal, poly;
        try { Brak b = tolka(in); tal = std::to_string(b.t()) + "/" + std::to_string(b.n()); } catch (const Fel&) {}
        try { poly = skriv(tolkaUttryck(in)); } catch (const Fel&) {}
        std::string vt = f["tal"].null() ? "" : f["tal"].str(), vp = f["poly"].null() ? "" : f["poly"].str();
        // tal som Python räknade ungefärligt (π, sin 20°, lg 2) får skilja i sista siffrorna
        bool tal_ok = tal == vt;
        if (!tal_ok && !tal.empty() && !vt.empty()) {
            auto d = [](const std::string& s) { auto p = s.find('/'); return std::stod(s.substr(0, p)) / std::stod(s.substr(p + 1)); };
            tal_ok = std::fabs(d(tal) - d(vt)) < 1e-8;
        }
        if (!tal_ok || poly != vp) {
            if (++skillnad <= 15) fprintf(stderr, "  '%s': tal %s/%s  poly '%s'/'%s'\n", in.c_str(), tal.c_str(), vt.c_str(), poly.c_str(), vp.c_str());
        }
    }
    KOLLA(skillnad == 0, "%d av %zu texter tolkas annorlunda än i Python", skillnad, fall.storlek());
}

// Pen pressure must never generate NaN, negative widths, or thick mouse strokes.
static void testaPenntryck() {
    KOLLA(penntryck(.25, true) == .25, "stylus pressure");
    KOLLA(penntryck(-2, true) == 0 && penntryck(4, true) == 1, "pressure clamp");
    KOLLA(penntryck(NAN, true) == 1, "invalid pressure fallback");
    KOLLA(pennbredd(3, .5, false) == 3, "mouse and touch retain constant width");
    KOLLA(pennbredd(3, 0, true) > 0 && pennbredd(3, 1, true) > pennbredd(3, 0, true),
          "pressure controls visual stroke thickness");
}

static void testaVisuellNiva() {
    KOLLA(automatiskVisuellNiva("forskola") == VisuellNiva::Lekfull, "förskola illustrerad");
    KOLLA(automatiskVisuellNiva("lgr22.1-3") == VisuellNiva::Lekfull, "åk 1–3 illustrerad");
    KOLLA(automatiskVisuellNiva("lgr22.4-6") == VisuellNiva::Lugn, "mellanstadiet lugnt");
    KOLLA(automatiskVisuellNiva("lgr22.7-9") == VisuellNiva::Akademisk, "högstadiet avskalat");
    KOLLA(automatiskVisuellNiva("gy25.1") == VisuellNiva::Professionell &&
          automatiskVisuellNiva("gy25.fortsattning") == VisuellNiva::Professionell,
          "gymnasiet professionellt");
    KOLLA(automatiskVisuellNiva("okand") == VisuellNiva::Lugn, "okänt stadium får säker reserv");
    KOLLA(visuellNiva("gy25.2", "illustrerad") == VisuellNiva::Lugn,
          "bildstöd på gymnasiet betyder inte maskotläge");
    KOLLA(visuellNiva("lgr22.1-3", "avskalad") == VisuellNiva::Akademisk,
          "elevens val kan ge avskalad stil på lågstadiet");
    KOLLA(visuellNiva("lgr22.7-9", "automatisk") == VisuellNiva::Akademisk,
          "automatiskt högstadieutseende");
    KOLLA(normaliseraVisuellStil("främmande") == "automatisk" &&
          visuellStilIndex("främmande") == 0, "oväntat profilvärde återgår till automatisk");
    KOLLA(std::string(visuellStilVarde(1)) == "illustrerad" &&
          std::string(visuellStilVarde(2)) == "avskalad" &&
          std::string(visuellStilVarde(200)) == "automatisk", "stilkoderna är säkra");
    KOLLA(std::string(visuellNivaKlass(VisuellNiva::Professionell)) ==
          "matte-visuell-professionell", "CSS-klass för gymnasiet");
}

// Själva handskriftens form, sparning och återläsning provas utan GTK och riktig elevdata.
static void testaPapperLagring() {
    gchar* tmp = g_dir_make_tmp("matte-papper-XXXXXX", nullptr);
    KOLLA(tmp != nullptr, "tillfällig datamapp för räknepapper");
    if (!tmp) return;
    g_setenv("XDG_DATA_HOME", tmp, TRUE);
    PapperData p;
    p.id = nyttPapperId(); p.kort = "brak-delar";
    p.fraga = "Hur stor del är 3/4?";
    p.mallIndex = 2; p.variantIndex = 4;
    p.c[std::make_pair(1, 2)] = {"3", "1"};
    p.streck.push_back({2, 1, 4});
    p.drag.push_back({{1.125, 2.5, .25, true}, {2.75, 3.0, .95, true}});
    std::string fil, err;
    KOLLA(giltigtPapperId(p.id) && !giltigtPapperId("../andra") &&
          papperFil("a", "../andra").empty(), "säker UUID och stoppad sökväg");
    KOLLA(papperTillJson(p, fil, &err), "JSON från giltigt papper: %s", err.c_str());
    PapperData d;
    KOLLA(papperFranJson(fil, d, &err) &&
          d.id == p.id && d.c[std::make_pair(1, 2)].t == "3" && d.c[std::make_pair(1, 2)].liten == "1" &&
          d.streck.size() == 1 && d.drag.size() == 1 &&
          d.drag[0][1].tryckFinns && d.drag[0][1].tryck > .94,
          "tryck, minnessiffra och streck överlever JSON-rundgång");
    KOLLA(papperSpara("forsta-elev", p, &err), "atomär lokal sparning: %s", err.c_str());
    PapperData igen;
    KOLLA(papperLadda("forsta-elev", p.id, igen, &err) &&
          igen.fraga == p.fraga && igen.c[std::make_pair(1, 2)].liten == "1" &&
          igen.drag[0][1].first == 2.75, "återställ efter omstart");
    KOLLA(!papperLadda("andra-elev", p.id, d, &err),
          "elev B får inte läsa elev A:s papper");
    PapperData tidigare = igen;
    KOLLA(!papperFranJson("{trasig", igen, &err) && igen.id == tidigare.id,
          "skadad JSON får inte förstöra befintligt papper");
    KOLLA(!papperFranJson(std::string(PAPPER_MAX_BYTE + 1, 'x'), igen, &err),
          "för stor fil stoppas före JSON-parsning");
    PapperData felData = p;
    felData.drag[0][0].tryck = NAN;
    KOLLA(!papperKontroll(felData), "NaN-tryck stoppas");
    felData = p; felData.drag[0][0].first = 1e50;
    KOLLA(!papperKontroll(felData), "ogiltig koordinat stoppas");
    felData = p; felData.id = "../annat";
    KOLLA(!papperSpara("forsta-elev", felData), "farligt filnamn skrivs aldrig");
    const std::string path = papperFil("forsta-elev", p.id);
    const std::string mapp = papperMapp("forsta-elev");
    g_remove(path.c_str()); g_rmdir(mapp.c_str());
    g_rmdir((datamapp() + "/raknepapper").c_str());
    g_rmdir(datamapp().c_str()); g_rmdir(tmp);
    g_unsetenv("XDG_DATA_HOME");
    g_free(tmp);
}

static void testaBrak() {
    KOLLA(tolka("1/2") == tolka("0,5") && tolka("50 %") == tolka("2/4"), "former av en halv");
    KOLLA(tolka("1 1/2") == Brak(3, 2), "blandad form");
    KOLLA(tolka("√(9/4)") == Brak(3, 2), "exakt rot");
    KOLLA(Brak(3, 4).text("procent") == "75 %", "procent: %s", Brak(3, 4).text("procent").c_str());
    KOLLA(Brak(1, 3).text("decimal") == "0,333", "decimal");
    KOLLA(Brak(7, 4).text() == "1,75", "auto");
    KOLLA(skriv(tolkaUttryck("(x+1)(x+1)")) == "x² + 2x + 1", "kvadrat: %s", skriv(tolkaUttryck("(x+1)(x+1)")).c_str());
    bool kastade = false;
    try { tolka("1/0"); } catch (const Fel&) { kastade = true; }
    KOLLA(kastade, "division med noll");
}

static void testaJson() {
    Json j = Json::las("{\"a\": [1, 2.5, \"x\\u00e5\"], \"b\": {\"c\": true}, \"d\": null}");
    KOLLA(j["a"][1].tal() == 2.5 && j["a"][2].str() == "xå" && j["b"]["c"].sant() && j["d"].null(), "json läses");
    KOLLA(Json::las(j.skriv())["a"][2].str() == "xå", "json skrivs och läses tillbaka");
}

// Lättläst-stödet i rättningen: små stavfel förlåts, omkastade siffror känns igen
static void testaStavningOchSiffror() {
    KOLLA(redigeringsavstand("kvadratt", "kvadrat") == 1 && redigeringsavstand("triangle", "triangel") == 2 &&
          redigeringsavstand("jämn", "jamn") == 1, "redigeringsavstånd");
    Mall ord; ord.typ = "text";
    Variant v; v.facit = {"o:kvadrat"}; v.facitText = {"kvadrat"};
    Bedomning b = bedom(ord, v, {"kvadratt"});
    KOLLA(b.ratt && b.text.find("stavas") != std::string::npos, "nästan rätt stavat ord räknas: %s", b.text.c_str());
    b = bedom(ord, v, {"kvarts"});
    KOLLA(!b.ratt, "ett annat ord räknas inte");
    Variant k; k.facit = {"o:udda"}; k.facitText = {"udda"};
    KOLLA(!bedom(ord, k, {"uddaa"}).ratt, "korta ord (under fem bokstäver) måste stämma: %s", "udda");
    Mall tal; tal.typ = "text";
    Variant t; t.facit = {svarsnyckel("12")}; t.facitText = {"12"};
    b = bedom(tal, t, {"21"});
    KOLLA(!b.ratt && b.omkastad, "21 i stället för 12 känns igen som omkastat");
    KOLLA(!bedom(tal, t, {"13"}).omkastad, "13 är inte omkastat");
    Variant p; p.facit = {svarsnyckel("11")}; p.facitText = {"11"};
    KOLLA(!bedom(tal, p, {"11"}).omkastad && bedom(tal, p, {"11"}).ratt, "11 är rätt, inte omkastat");
}

// Exportera en elev och importera igen i en tom datamapp: allt ska följa med, inget farligt ska komma in
static void testaFlytt() {
    gchar* tmp = g_dir_make_tmp("matte-flytt-XXXXXX", nullptr);
    std::string a = std::string(tmp) + "/a", b = std::string(tmp) + "/b";
    g_setenv("XDG_DATA_HOME", a.c_str(), TRUE);
    Elev e = Elev::ladda("test-elev");
    e.visningsnamn = "Test Elev";
    e.svarade("brak-delar", true, "", 0);
    e.ritningar.push_back({"brak-delar", "1.png", "Rita", "2026-11-10 10:00"});
    PapperData papper;
    papper.id = nyttPapperId();
    papper.kort = "brak-delar"; papper.fraga = "1/2 + 1/4";
    papper.mallIndex = 0; papper.variantIndex = 3;
    papper.c[std::make_pair(3, 2)] = {"7", ""};
    papper.drag.push_back({{1.25, 2.5, .8, true}});
    e.papperId = papper.id; e.papperKort = papper.kort;
    e.papperFraga = papper.fraga;
    e.papperMall = papper.mallIndex; e.papperVariant = papper.variantIndex;
    KOLLA(papperSpara("test-elev", papper), "testpapperet sparas före export");
    e.spara();
    logga("test-elev", "svar", Json::objekt().satt("kort", "brak-delar"));
    g_mkdir_with_parents(ritmapp("test-elev").c_str(), 0700);
    const char png[] = "\x89PNG\r\n\x1a\nfejk";
    g_file_set_contents((ritmapp("test-elev") + "/1.png").c_str(), png, sizeof png - 1, nullptr);
    std::string fil = std::string(tmp) + "/elev.json", f;
    KOLLA(exporteraElev("test-elev", fil, &f), "export: %s", f.c_str());
    g_setenv("XDG_DATA_HOME", b.c_str(), TRUE);
    bool fanns = true;
    std::string visn, namn = importeraElev(fil, false, &f, &fanns, &visn);
    KOLLA(namn == "test-elev" && !fanns && visn == "Test Elev", "import: %s (%s)", namn.c_str(), f.c_str());
    Elev igen = Elev::ladda("test-elev");
    KOLLA(igen.status("brak-delar").antalRatt == 1 &&
          igen.ritningar.size() == 1 && igen.papperId == papper.id &&
          igen.papperVariant == 3, "framstegen och aktiva uppgiften följer med");
    PapperData papperIgen;
    KOLLA(papperLadda("test-elev", papper.id, papperIgen) &&
          papperIgen.c[std::make_pair(3, 2)].t == "7" &&
          papperIgen.drag.size() == 1 &&
          papperIgen.drag[0][0].tryck > .79,
          "handskrift och tryck följer med elevexport/import");
    KOLLA(lasLogg("test-elev").size() == 1, "loggen följer med");
    KOLLA(g_file_test((ritmapp("test-elev") + "/1.png").c_str(), G_FILE_TEST_EXISTS), "teckningen följer med");
    importeraElev(fil, false, &f, &fanns);
    KOLLA(fanns, "en elev som redan finns skrivs inte över utan att fråga");
    // en elakt gjord fil: teckningsnamn med ../ ska inte kunna skriva utanför mappen
    std::string elak = std::string(tmp) + "/elak.json";
    std::string innehall = R"({"format":"se.wenuyos.lara.elev","program":"se.wenuyos.app.matte","namn":"../../x","elev":{"namn":"x"},"ritningar":{"../../ut.png":"iVBORw0KGgo="}})";
    g_file_set_contents(elak.c_str(), innehall.c_str(), -1, nullptr);
    namn = importeraElev(elak, true, &f);
    KOLLA(namn.find('/') == std::string::npos && !g_file_test((b + "/ut.png").c_str(), G_FILE_TEST_EXISTS) &&
          !g_file_test((std::string(tmp) + "/ut.png").c_str(), G_FILE_TEST_EXISTS), "farliga sökvägar i en importfil stoppas");
    std::string elaktPapper = std::string(tmp) + "/elakt-papper.json";
    const std::string attack = R"JSON({"format":"se.wenuyos.lara.elev","program":"se.wenuyos.app.matte","namn":"test-elev","elev":{"namn":"test-elev"},"raknepapper":{"../../stulen.json":"{}"}})JSON";
    g_file_set_contents(elaktPapper.c_str(), attack.c_str(), -1, nullptr);
    std::string felPapper;
    KOLLA(importeraElev(elaktPapper, true, &felPapper).empty() &&
          !g_file_test((b + "/stulen.json").c_str(), G_FILE_TEST_EXISTS),
          "path traversal i räknepapper blockeras före profilsparning");
    g_unsetenv("XDG_DATA_HOME");
    std::string rm = std::string("rm -rf '") + tmp + "'";
    if (system(rm.c_str()) != 0) fprintf(stderr, "kunde inte städa %s\n", tmp);
    g_free(tmp);
}

// Ladda en elevprofil i det redan publicerade 1.2-formatet, utan att starta GTK.
// Ändra aldrig originalprofilen vid provet: allt sker i en tillfällig XDG-datamapp.
static void testaUppgraderingFran12() {
    gchar* temp = g_dir_make_tmp("matte-upgrade-XXXXXX", nullptr);
    KOLLA(temp != nullptr, "tillfällig datamapp för uppgraderingsprov");
    if (!temp) return;
    std::string root = temp;
    std::string appDir = root + "/se.wenuyos.app.matte";
    g_mkdir_with_parents(appDir.c_str(), 0700);
    const std::string profil = R"JSON({
      "namn":"elev12","visningsnamn":"Elev från 1.2",
      "placerad":true,"lekläge":false,"lättläst":true,"sifferknappar":true,"zoom":1.4,
      "kort":{"brak-delar":{"lage":2,"ratt_i_rad":4,"steg":3,
        "nasta":"2026-11-20","antal":16,"antal_ratt":12,"senast":"2026-11-10",
        "historik":[0,1,1],"feltankar":["gammal-feltanke"],
        "feltanke_antal":{"0|fel":2},"feltanke_totalt":{"0|fel":3}}},
      "regelbok":["brak-delar"],
      "dagens":{"klar":"2026-11-10","svit":4},
      "ritningar":[{"kort":"brak-delar","fil":"skiss.png","fraga":"Rita bråket","tid":"2026-11-10 12:00"}]
    })JSON";
    bool written = g_file_set_contents((appDir + "/elev12.json").c_str(), profil.c_str(), -1, nullptr);
    KOLLA(written, "profil 1.2 kan skapas för uppgraderingsprov");
    if (written) {
        g_setenv("XDG_DATA_HOME", root.c_str(), TRUE);
        Elev e = Elev::ladda("elev12");
        auto it = e.kort.find("brak-delar");
        bool loaded = it != e.kort.end() && it->second.lage == BEHARSKAR &&
            it->second.antal == 16 && it->second.antalRatt == 12 &&
            it->second.nasta == "2026-11-20" && it->second.feltankeTotalt["0|fel"] == 3 &&
            e.lattlast && e.sifferknappar && e.zoom > 1.39 && e.zoom < 1.41 &&
            e.dagensSvit == 4 && e.regelbok.size() == 1 && e.ritningar.size() == 1 &&
            e.visuellStil == "automatisk";
        KOLLA(loaded, "1.2: framsteg, fellogik, repetition, lässtöd, textstorlek och ritning läses");
        e.visuellStil = "avskalad";
        bool saved = e.spara();
        Elev again = Elev::ladda("elev12");
        auto second = again.kort.find("brak-delar");
        KOLLA(saved && second != again.kort.end() &&
            second->second.antalRatt == 12 && second->second.steg == 3 &&
            second->second.feltankeTotalt["0|fel"] == 3 &&
            again.lattlast && again.sifferknappar && again.regelbok.size() == 1 &&
            again.ritningar.size() == 1 && again.dagensKlar == "2026-11-10" &&
            again.visuellStil == "avskalad",
            "1.2 -> 1.3: sparning/återläsning bevarar elevens historik och stöd");
        g_unsetenv("XDG_DATA_HOME");
    }
    g_remove((appDir + "/elev12.json").c_str());
    g_rmdir(appDir.c_str());
    g_rmdir(root.c_str());
    g_free(temp);
}

int main(int argc, char** argv) {
    std::string mapp = argc > 1 ? argv[1] : "tester";
    testaFlytt();
    testaUppgraderingFran12();
    testaStavningOchSiffror();
    testaBrak();
    testaPenntryck();
    testaPapperLagring();
    testaVisuellNiva();
    testaJson();
    testaTolkning(mapp);
    if (argc > 2) {   // data-mappen: alla uppgifter bedöms rätt med sitt eget facit
        extern int testaData(const std::string&, int&, int&);
        testaData(argv[2], ok, fel);
    }
    printf("%d ok, %d fel\n", ok, fel);
    return fel ? 1 : 0;
}

// Varje färdig uppgift ska bedömas rätt med sitt eget facit – precis som `matte check` i Python-versionen.
// En feltanke i kryssa/para/måla: bygg elevens fel svar ur felsvaret och kolla att förklaringen kommer
static void testaNyFeltankar(const std::string& kid, size_t mi, const Mall& m, const Variant& v, const Data& d, int& testade, int& fel) {
    for (auto& f : v.fel) {
        if (!v.facit.empty() && f.nyckel == v.facit[0]) continue;
        std::vector<std::string> svar;
        if (m.typ == "kryssa") {
            if (std::find(v.alternativ.begin(), v.alternativ.end(), f.felsvar) == v.alternativ.end()) continue;
            svar = v.facitSvar; svar.push_back(f.felsvar);
        } else if (m.typ == "para") {
            auto p = f.felsvar.find("→");
            if (p == std::string::npos) continue;
            std::string vv = trim(f.felsvar.substr(0, p)), hh = trim(f.felsvar.substr(p + strlen("→")));
            auto vi = std::find_if(v.falt.begin(), v.falt.end(), [&](const std::string& x) { return textnorm(x) == textnorm(vv); });
            auto hi = std::find_if(v.alternativ.begin(), v.alternativ.end(), [&](const std::string& x) { return textnorm(x) == textnorm(hh); });
            if (vi == v.falt.end() || hi == v.alternativ.end()) continue;
            svar = v.facitSvar;
            svar[vi - v.falt.begin()] = *hi;
        } else if (m.typ == "mala") {
            std::string del; std::stringstream in(f.felsvar);
            while (std::getline(in, del, '|')) {
                auto k = del.find(':');
                if (k == std::string::npos) svar.push_back(trim(del));
                else for (int i = 0; i < atoi(del.c_str() + k + 1); ++i) svar.push_back(trim(del.substr(0, k)));
            }
            if (!f.felsvar.empty() && f.felsvar.back() == '|') svar.push_back("");
            size_t n = celler(d, v.figur).size();
            if (svar.size() > n) continue;
            svar.resize(n);
        } else continue;
        Bedomning b = bedom(m, v, svar);
        if (b.ratt || b.otolkbart) continue;
        ++testade;
        if (b.text != f.text && ++fel <= 10)
            fprintf(stderr, "  %s uppgift %zu: felsvaret '%s' gav '%s'\n", kid.c_str(), mi + 1, f.felsvar.c_str(), b.text.c_str());
    }
}

int testaData(const std::string& mapp, int& ok_, int& fel_) {
    std::map<std::string, int> nya;
    Data d;
    std::string err;
    if (!d.las(mapp, &err)) { fprintf(stderr, "FEL: data: %s\n", err.c_str()); ++fel_; return 1; }
    int antal = 0, felaktiga = 0, feltankarTestade = 0, feltankarFel = 0;
    for (auto& k : d.kort()) {
        const auto& mallar = d.mallar(k.id);
        if (mallar.empty()) { fprintf(stderr, "FEL: %s saknar uppgifter\n", k.id.c_str()); ++fel_; }
        for (size_t mi = 0; mi < mallar.size(); ++mi) {
            const Mall& m = mallar[mi];
            for (auto& v : m.varianter) {
                ++antal;
                std::vector<std::string> svar;
                bool flera = (m.typ == "tabell" || m.typ == "punkt" || m.typ == "linje") && v.facitText.size() > 1;
                if (flera) svar = v.facitText; else svar = {v.facitText.empty() ? "" : v.facitText[0]};
                if (nyTyp(m.typ)) {   // kryssa, para, mala, rita: facit som elevens svar
                    svar = v.facitSvar;
                    if (m.typ == "mala") svar.resize(std::max(svar.size(), celler(d, v.figur).size()));
                    ++nya[m.typ];
                    testaNyFeltankar(k.id, mi, m, v, d, feltankarTestade, feltankarFel);
                }
                Bedomning b = bedom(m, v, svar);
                // varje känd feltanke ska ge sin egen förklaring (om felsvaret inte råkar vara facit)
                for (auto& f : v.fel) {
                    if (nyTyp(m.typ)) break;
                    if (f.felsvar.empty() || (!v.facit.empty() && f.nyckel == v.facit[0])) continue;
                    if (m.typ == "val" && std::find(v.alternativ.begin(), v.alternativ.end(), f.felsvar) == v.alternativ.end()) continue;
                    if (m.typ == "valj" || m.typ == "markera" || m.typ == "klick" || m.typ == "farga" || m.typ == "dra") continue;
                    std::vector<std::string> fs;
                    if (flera) { std::string del; std::stringstream in(f.felsvar); while (std::getline(in, del, ';')) fs.push_back(del); }
                    else fs = {f.felsvar};
                    if (fs.size() != v.facit.size()) continue;
                    Bedomning bf = bedom(m, v, fs);
                    if (bf.ratt || bf.otolkbart) continue;   // samma tal som facit för den här varianten, eller ingen siffra
                    ++feltankarTestade;
                    bool tvetydig = std::count_if(v.fel.begin(), v.fel.end(), [&](const Feltanke& g) { return g.nyckel == f.nyckel; }) > 1;
                    if (bf.text != f.text && !tvetydig && ++feltankarFel <= 10)
                        fprintf(stderr, "  %s uppgift %zu: felsvaret '%s' gav '%s'\n", k.id.c_str(), mi + 1, f.felsvar.c_str(), bf.text.c_str());
                }
                if (!b.ratt) {
                    if (++felaktiga <= 20)
                        fprintf(stderr, "  %s uppgift %zu: facit '%s' bedöms fel: %s\n", k.id.c_str(), mi + 1,
                                svar[0].c_str(), b.text.c_str());
                }
            }
        }
    }
    // Alla figurer går att rita: korten och första varianten av varje mall, på ett kontaktark
    int figurer = 0, figurfel = 0;
    std::vector<std::pair<std::string, Json>> alla;
    for (auto& k : d.kort()) {
        for (auto& r : k.rader) if (r.figur.arObjekt()) alla.push_back({k.id, r.figur});
        for (auto& e : k.exempel) if (e.figur.arObjekt()) alla.push_back({k.id, e.figur});
        for (auto& m : d.mallar(k.id)) for (auto& v : m.varianter) if (v.figur.arObjekt()) alla.push_back({k.id, v.figur});
    }
    cairo_surface_t* ark = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 1400, 1800);
    cairo_t* cr = cairo_create(ark);
    cairo_set_source_rgb(cr, 1, 1, 1); cairo_paint(cr);
    double ax = 10, ay = 10, radh = 0;
    std::set<std::string> visade;
    for (auto& [kid, fj] : alla) {
        try {
            Figur f = figur(d, fj);
            cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, std::max(1, (int)f.bredd), std::max(1, (int)f.hojd));
            cairo_t* c2 = cairo_create(s);
            f.rita(c2, 0, 0);
            cairo_destroy(c2); cairo_surface_destroy(s);
            ++figurer;
            std::string typ = fj["typ"].str();
            if (visade.insert(typ).second && f.bredd < 700) {   // en av varje sort på kontaktarket
                if (ax + f.bredd > 1390) { ax = 10; ay += radh + 30; radh = 0; }
                if (ay + f.hojd < 1790) {
                    f.rita(cr, ax, ay + 16);
                    text(cr, ax, ay + 10, typ, 11, true, "#d1403a");
                    ax += f.bredd + 20; radh = std::max(radh, f.hojd + 16);
                }
            }
        } catch (const std::exception& e) {
            if (++figurfel <= 10) fprintf(stderr, "  %s: figuren går inte att rita: %s\n", kid.c_str(), e.what());
        }
    }
    cairo_surface_write_to_png(ark, "figurer.png");
    for (auto& [typ, n] : nya) printf("%s: %d uppgifter\n", typ.c_str(), n);

    // Utskrift: ett arbetsblad för varje kort, samma nummer = samma blad, och hela regelboken
    int blad = 0, bladfel = 0;
    for (auto& k : d.kort()) {
        std::string fel, fil = "/tmp/matte-test-" + k.id + ".pdf";
        if (arbetsbladPdf(d, {&k}, fil, 4711, 8, &fel) != 4711) { if (++bladfel <= 5) fprintf(stderr, "  %s: arbetsbladet: %s\n", k.id.c_str(), fel.c_str()); }
        else ++blad;
        if (arbetsbladFragor(d, {&k}, 4711) != arbetsbladFragor(d, {&k}, 4711)) { ++bladfel; fprintf(stderr, "  %s: blad 4711 blev olika\n", k.id.c_str()); }
        g_remove(fil.c_str());
    }
    std::vector<std::string> grupper;
    auto bok = kortILaroplansordning(d, &grupper);
    std::string fel;
    if (bok.size() != d.kort().size()) { ++bladfel; fprintf(stderr, "  läroplansordningen tappar kort\n"); }
    if (!regelbokPdf(d, bok, "Regelboken", "test", "/tmp/matte-test-regelbok.pdf", &fel, &grupper)) { ++bladfel; fprintf(stderr, "  regelboken: %s\n", fel.c_str()); }
    g_remove("/tmp/matte-test-regelbok.pdf");
    printf("%d arbetsblad och regelboken som PDF\n", blad);

    // Ordlistan: finns, varje form leder till sitt ord, figurerna går att rita
    int ordfel = 0;
    for (size_t i = 0; i < d.ordlista().size(); ++i) {
        const Ord& o = d.ordlista()[i];
        for (auto& f : o.former) if (d.ordFor(f) != (int)i && ++ordfel <= 5) fprintf(stderr, "  ordlistan: '%s' leder fel\n", f.c_str());
        if (o.figur.arObjekt()) try { figur(d, o.figur); } catch (const std::exception& e) { if (++ordfel <= 5) fprintf(stderr, "  ordlistan: %s: %s\n", o.ord.c_str(), e.what()); }
    }
    if (d.ordlista().size() < 40) { ++ordfel; fprintf(stderr, "  ordlistan har bara %zu ord\n", d.ordlista().size()); }
    printf("%zu ord i ordlistan\n", d.ordlista().size());
    if (ordfel) { fprintf(stderr, "FEL: %d fel i ordlistan\n", ordfel); ++fel_; } else ++ok_;
    if (bladfel) { fprintf(stderr, "FEL: %d fel i utskriften\n", bladfel); ++fel_; } else ++ok_;
    cairo_destroy(cr); cairo_surface_destroy(ark);
    if (figurfel) { ++fel_; fprintf(stderr, "FEL: %d figurer går inte att rita\n", figurfel); } else ++ok_;
    printf("%d figurer ritade (kontaktark: figurer.png)\n", figurer);
    printf("%zu kort, %d färdiga uppgifter, %d feltankar provade\n", d.kort().size(), antal, feltankarTestade);
    if (feltankarFel) { ++fel_; fprintf(stderr, "FEL: %d feltankar känns inte igen\n", feltankarFel); } else ++ok_;
    if (felaktiga) { ++fel_; fprintf(stderr, "FEL: %d uppgifter bedöms fel med sitt eget facit\n", felaktiga); }
    else ++ok_;
    return 0;
}
