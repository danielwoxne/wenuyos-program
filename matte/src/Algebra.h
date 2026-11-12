// Algebra – uttryck med variabler (åk 7–9 och gymnasiet).
//
// Ett uttryck tolkas till ett polynom: monom (sorterade variabel–exponent-par) → koefficient.
// Två uttryck är likvärdiga om de ger samma polynom: 2x + 3x = 5x = x·5, (x+1)(x+1) = x² + 2x + 1.
// Variabler: en bokstav, ev. med en siffra (x, y, a, x1). Implicit multiplikation (2x, 3(x+1), xy).
// Division bara med tal. Samma regler som Python-versionens algebra.py.
#pragma once
#include "Tal.h"

#include <map>
#include <string>
#include <vector>

namespace matte {

using Monom = std::vector<std::pair<std::string, int>>;   // sorterat på variabelnamn
using Polynom = std::map<Monom, Brak>;                     // inga nollkoefficienter

Polynom tolkaUttryck(const std::string& text);             // kastar Fel
std::string skriv(const Polynom& p);                       // skolans skrivsätt: 3x² + 2x − 5
bool arKonstant(const Polynom& p);
double varde(const Polynom& p, const std::map<std::string, double>& v);   // för grafer

}  // namespace matte
