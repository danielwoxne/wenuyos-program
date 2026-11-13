// Bedömning av elevens svar – samma regler som Python-versionen:
// rätt → kort bekräftelse · känd feltanke → dess förklaring · annars ledtråden.
// Alla former av samma tal godtas (1/2 = 0,5 = 50 %), likvärdiga uttryck likaså (2x + 3x = 5x).
#pragma once
#include "Data.h"

#include <string>
#include <vector>

namespace matte {

struct Bedomning {
    bool ratt = false;
    std::string text;              // vad eleven får läsa
    bool feltanke = false;
    bool otolkbart = false;        // svaret gick inte att förstå – räknas inte som fel
    std::vector<int> felFalt;      // vilka fält som var fel (tabell)
    std::string feltankeId;        // "<mall>|<felsvar>" om en känd feltanke
    bool omkastad = false;         // rätt siffror i fel ordning (21 i stället för 12) – vanligt vid dyslexi
};

std::string svarsnyckel(const std::string& svar);          // "0,5" → t:1/2, "2x+3x" → p:5x, "Blå" → o:blå
std::string mangd(const std::string& text);                 // "gul, blå,gul" → "blå,gul,gul"
std::string textnorm(const std::string& s);                 // utan mellanslag, komma = punkt, gemener
// kryssa · para · mala · rita – svaret är en sträng per sak (se innehall/matte/uppgifter.py)
bool nyTyp(const std::string& typ);
int redigeringsavstand(const std::string& a, const std::string& b);
std::string nyNyckel(const std::string& typ, const std::vector<std::string>& svar, const std::string& facit0 = "");
extern const char* const FARGER[6];          // färgpennorna: röd blå gul grön orange lila
Bedomning bedom(const Mall& m, const Variant& v, const std::vector<std::string>& svar);

}  // namespace matte
