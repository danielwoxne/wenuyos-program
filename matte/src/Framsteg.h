// Framsteg – vad eleven behärskar och när det är dags att repetera, plus anpassningen
// (vilken uppgift som kommer härnäst) och placeringstestet.
//
// Tre lägen per kort: 0 Lär · 1 Övar · 2 Behärskar. Behärskade kort repeteras med växande mellanrum
// (1, 3, 7, 14, 30, 60, 120 dagar); fel kortar mellanrummet men nollställer inte.
// Sparas som JSON i ~/.local/share/se.wenuyos.app.matte/<elev>.json – samma format som
// Python-versionen, så elevernas framsteg följer med till C++-versionen.
#pragma once
#include "Data.h"

#include <map>
#include <random>
#include <string>
#include <vector>

namespace matte {

enum Lage { LAR = 0, OVAR = 1, BEHARSKAR = 2 };

struct Kortstatus {
    int lage = LAR, rattIRad = 0, steg = 0, antal = 0, antalRatt = 0;
    std::string nasta, senast;                       // ISO-datum
    std::vector<int> historik;                       // de senaste svaren, 1 = rätt (högst 10)
    std::map<std::string, int> feltankeAntal;        // feltanke-id → hur ofta nyligen
    std::map<std::string, int> feltankeTotalt;       // för läraren
    std::vector<std::string> gamlaFeltankar;         // äldre format – läses och skrivs tillbaka orört
};

struct Ritning {                                     // fri rityta – sparas, rättas inte
    std::string kort, fil, fraga, tid;               // fil: namnet i ritmapp(elev), tid: "2026-11-10 14:05"
};

struct Elev {
    std::string namn = "elev", visningsnamn;
    std::map<std::string, Kortstatus> kort;
    std::vector<std::string> regelbok;               // korten i den ordning de mötts
    bool placerad = false, leklage = false;
    bool lattlast = false;
    std::string visuellStil = "automatisk";  // separat från lekläge/lättläst, per elev
    double zoom = 1.0;                               // textstorlek i övningen (Ctrl+plus/minus), 0.8–1.6
    bool sifferknappar = false;                      // knappar för siffror på skärmen (svara utan tangentbord)                           // lättläst: större text, luft, uppläsning, en mening per rad
    std::vector<Ritning> ritningar;                  // nyaste sist, högst 200
    std::string dagensKlar;                          // datum då "Dagens tre" senast blev klara
    int dagensSvit = 0;                              // dagar i rad

    int stjarnor(const std::string& kortId) const;   // 0–3: ett rätt svar · behärskar · sitter (repeterat)
    int allaStjarnor() const;
    std::vector<std::string> dagensKort(const std::string& dag = "") const;   // tre kort att blanda, samma hela dagen
    void dagensGjorda(const std::string& dag = "");

    static Elev ladda(const std::string& namn);
    bool spara() const;

    Kortstatus& status(const std::string& kortId) { return kort[kortId]; }
    int lage(const std::string& kortId) const { auto f = kort.find(kortId); return f == kort.end() ? LAR : f->second.lage; }
    void settKort(const std::string& kortId);
    void svarade(const std::string& kortId, bool ratt, const std::string& feltankeId, int mallIndex,
                 const std::string& idag = "");
    std::vector<std::string> attRepetera(const std::string& idag = "") const;
    std::map<int, int> sammanfattning() const;       // läge → antal kort
};

std::string datamapp();                              // ~/.local/share/se.wenuyos.app.matte
std::string ritmapp(const std::string& elev);        // datamapp()/ritningar/<elev>
std::string filnamn(const std::string& namn);        // "Ali S." → "ali-s"
std::vector<std::string> elever();
std::string senasteElev();
void sattSenasteElev(const std::string& namn);
std::string idag();
std::string plusDagar(const std::string& datum, int dagar);

// ---- anpassning -------------------------------------------------------------------------------
int nivaFor(const Kortstatus& s);
// Väljer nästa uppgift. tranar = en feltanke tränas (visa ledtråden från början).
Uppgift valjUppgift(const Data& d, const Kort& k, const Kortstatus& s, bool baraInteraktiva,
                    std::mt19937& rnd, bool* tranar = nullptr);
const Kort* gaTillbaka(const Data& d, const Kort& k, Elev& e);   // förkunskapskort om eleven fastnat

// ---- placering --------------------------------------------------------------------------------
struct Placering {
    std::string omrade;
    std::vector<std::string> kortIds;
    int lo = 0, hi = -1, fragor = 0, aktuell = 0;
    bool klar() const { return lo > hi || kortIds.empty(); }
    Uppgift nasta(const Data& d, std::mt19937& rnd);
    void svara(bool ratt);
    void tillampa(Elev& e) const;
};
std::vector<Placering> placeraAlla(const Data& d);

}  // namespace matte
