// Utskrift som PDF (cairo): arbetsblad med facit, och regelboken.
// Ingen GTK här – samma kod används av fönstret, kommandoraden (matte --arbetsblad …) och testerna.
#pragma once
#include "Data.h"

#include <string>
#include <vector>

namespace matte {

// Ett arbetsblad: `antal` uppgifter per kort, från lätt till svår, och facit med lösningar på egen sida.
// Bladnumret bestämmer uppgifterna – samma nummer ger samma blad igen (läraren delar ut blad 4711 och
// rättar mot facit 4711). blad = 0 → ett nytt nummer. Returnerar bladnumret, 0 om det inte gick.
// lattlast: större text med mer luft mellan bokstäver och rader, ingen kursiv (stöd vid dyslexi)
int arbetsbladPdf(const Data& d, const std::vector<const Kort*>& kort, const std::string& fil, int blad = 0,
                  int antal = 8, std::string* fel = nullptr, bool lattlast = false);

// Frågorna på bladet (för testerna: samma nummer ska ge samma blad)
std::vector<std::string> arbetsbladFragor(const Data& d, const std::vector<const Kort*>& kort, int blad, int antal = 8);

// Regelboken: en sida (eller fler) per kort, med försättsblad och innehållsförteckning.
// grupper (valfri, en per kort): rubriken i innehållsförteckningen – stadiet, eller området.
bool regelbokPdf(const Data& d, const std::vector<const Kort*>& kort, const std::string& titel,
                 const std::string& undertitel, const std::string& fil, std::string* fel = nullptr,
                 const std::vector<std::string>* grupper = nullptr);

// Alla kort i läroplanens ordning (förskola → gymnasium); stadium (ut) = stadiets namn för varje kort
std::vector<const Kort*> kortILaroplansordning(const Data& d, std::vector<std::string>* stadium = nullptr);

// ~/Dokument/Matte (användarens dokumentmapp) – skapas vid behov
std::string utskriftsmapp();
// Ett filnamn utan tecken som ställer till det: "Bråk – delar av en hel" → "Bråk – delar av en hel" (/ och : bort)
std::string sakertFilnamn(const std::string& namn);

}  // namespace matte
