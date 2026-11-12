// Tal – exakta bråk och tolkning av elevens svar.
//
// Allt räknas med bråk så att 1/2, 0,5, 50 % och 2/4 är *samma* tal – grunden för att programmet
// bedömer om ett svar är likvärdigt med facit i stället för att kräva en viss form.
// Tolkar: heltal, decimaltal (komma eller punkt), a/b, blandad form "1 1/2", procent, + - · * / : ^,
// parenteser, negativa tal, √9, sqrt(2), 8^(1/3), 2^-1, sin/cos/tan (grader), lg, ln, π.
// Rötter och potenser räknas exakt när svaret är rationellt; annars ett bråk inom 10⁻⁹ (uppgifter
// med sådana svar har en tolerans). Samma regler som Python-versionens uttryck.py.
#pragma once
#include "Text.h"   // gemener, trim
#include <cstdint>
#include <stdexcept>
#include <string>

namespace matte {

struct Fel : std::runtime_error {   // svaret gick inte att tolka – texten visas för eleven
    using std::runtime_error::runtime_error;
};

class Brak {
public:
    Brak(int64_t t = 0, int64_t n = 1);
    static Brak ur(__int128 t, __int128 n);          // förkortar, kastar Fel vid för stora tal
    static Brak ungefar(double x);                   // närmaste bråk med nämnare ≤ 10⁹
    static Brak decimal(const std::string& s);       // "3.25" → 13/4

    int64_t t() const { return t_; }
    int64_t n() const { return n_; }
    double tal() const { return double(t_) / double(n_); }
    bool heltal() const { return n_ == 1; }

    Brak operator+(const Brak& o) const;
    Brak operator-(const Brak& o) const;
    Brak operator*(const Brak& o) const;
    Brak operator/(const Brak& o) const;
    Brak operator-() const { return Brak(-t_, n_); }
    bool operator==(const Brak& o) const { return t_ == o.t_ && n_ == o.n_; }
    bool operator!=(const Brak& o) const { return !(*this == o); }
    bool operator<(const Brak& o) const { return (__int128)t_ * o.n_ < (__int128)o.t_ * n_; }

    std::string nyckel() const;                      // "t:3/4" – samma som byggsteget skriver
    std::string text(const std::string& form = "auto") const;   // brak · decimal · procent · auto

private:
    struct Ra {};
    Brak(Ra, int64_t t, int64_t n) : t_(t), n_(n) {}   // redan förkortat
    int64_t t_, n_;
};

Brak potens(const Brak& bas, const Brak& e);
Brak tolka(const std::string& text);                 // kastar Fel
bool okantOrd(const std::string& text);              // innehåller bokstäver som inte är sin, lg … (→ algebra)

// Små hjälpare för UTF-8-text

}  // namespace matte
