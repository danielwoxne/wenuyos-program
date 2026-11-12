// Figurerna – bråkstaplar, tallinjer, positionstabeller, klockor, koordinatsystem, målningar …
// Beskrivs som JSON (från korten: {"typ": "stapel", "delar": 4, "fyllda": 1}) och ritas med cairo.
// Samma figurer, mått och klickytor som Python-versionen (rita.py).
#pragma once
#include "Json.h"

#include <cairo.h>

#include <functional>
#include <string>
#include <vector>

namespace matte {

class Data;

namespace tema {
extern const char* TEXT;
extern const char* TEXT_SVAG;
extern const char* LINJE;
extern const char* VIT;
extern const char* ACCENT;
extern const char* ACCENT_SVAG;
extern const char* ROD;
extern const char* GRON;
std::string farg(const std::string& namn);     // färgpenna ("gul", "himmel") eller #hex
void satt(cairo_t* cr, const std::string& hex, double alfa = 1.0);
}  // namespace tema

struct Figur {
    double bredd = 0, hojd = 0;
    std::function<void(cairo_t*, double, double)> rita;
};

struct Ruta { double x, y, b, h; };

Figur figur(const Data& d, const Json& beskrivning);                 // kastar std::runtime_error
bool figurFinns(const std::string& typ);
std::vector<Ruta> celler(const Data& d, const Json& beskrivning);     // klickbara rutor (stapel, prickar, scen …)
std::vector<std::string> cellnamn(const Data& d, const Json& beskrivning);
Json medFyllda(const Json& beskrivning, int fyllda, const std::string& nyckel = "fyllda");

// Pricka i tallinje (ett tal) eller koordinatsystem (x, y)
struct Rutnat {
    std::string typ;
    double fran = 0, till = 10, steg = 1, bredd = 420;
    int xmin = 0, xmax = 6, ymin = 0, ymax = 6, ruta = 32, marg = 24;
    explicit Rutnat(const Json& beskrivning);
    bool giltig() const { return typ == "tallinje" || typ == "koordinatsystem"; }
    std::pair<double, double> snapp(double x, double y) const;   // pixel → värde (tallinje: first)
    std::pair<double, double> pixel(double vx, double vy = 0) const;
};

// Ritverktyg för text och bilder (används också av fönstret)
void text(cairo_t* cr, double x, double y, const std::string& s, double storlek = 14, bool fet = false,
          const char* farg = nullptr, char ankare = 's', bool lutande = false);
extern bool UTSKRIFT;                                                  // PDF: bilderna i full upplösning
void bild(cairo_t* cr, double x, double y, double b, double h, const std::string& sokvag);
void bildstorlek(const std::string& sokvag, int& b, int& h);
std::string talText(double v);                                         // 2.5 → "2,5"

// En sak i kryssa/para/måla: text, "form:triangel", "ikon:apple" eller "prickar:4" – centrerad i rutan.
// fyll = färgpennans namn ("röd") – målar formen, eller bakgrunden bakom text och ikoner.
void sak(cairo_t* cr, const Data& d, double x, double y, double b, double h, const std::string& s, const std::string& fyll = "");
std::string pennfarg(const std::string& namn);                        // "röd" → #e5484d

}  // namespace matte
