// Uppläsning – frågorna läses upp för barn som inte läser själva än (lekläget).
// Använder en fri svensk röst på datorn: Piper (sv_SE-*.onnx) om den finns, annars espeak-ng.
// Matematiska tecken blir ord först ("3 · 4" → "3 gånger 4"). Allt körs lokalt.
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace matte {

std::string somOrd(const std::string& text);
extern int TALTAKT;                   // ord per minut för espeak (140; lättläst 115)
bool las(const std::string& text);    // false om ingen röst finns
// Läser upp orden och anropar vidOrd(i) när ord i börjar läsas, vidOrd(-1) när det är klart eller avbrutet –
// så att ordet kan markeras i texten. false om ingen röst finns.
bool lasOrd(const std::vector<std::string>& ord, std::function<void(int)> vidOrd);
void tyst();
std::string rostNamn();               // "Piper sv_SE-….onnx", "espeak-ng" eller "" (ingen röst)

}  // namespace matte
