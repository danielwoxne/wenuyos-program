#include "Hundraruta.h"
#include <algorithm>

namespace matte13 {
Hundraruta hundrarutaFranBrak(Brak b) {
    if (!giltigt(b)) return Hundraruta{0, false, Brak{0, 0}};
    // Nämnaren är högst 12. Multiplikationen ryms i int.
    Hundraruta r;
    r.utgang = b;
    const int d = b.taljare * 100;
    r.fyllda = std::clamp((d + b.namnare / 2) / b.namnare, 0, 100);
    r.exakt = d % b.namnare == 0;
    return r;
}
bool giltigHundraruta(int fyllda) { return fyllda >= 0 && fyllda <= 100; }
int rutorTillOchMed(int kolumn, int rad) {
    if (kolumn < 0 || kolumn > 9 || rad < 0 || rad > 9) return -1;
    return rad * 10 + kolumn + 1;
}
std::string hundrarutaForklaringSv(const Hundraruta& r) {
    if (!giltigt(r.utgang)) return "Inget giltigt bråk att visa.";
    const auto b = r.utgang;
    std::string text = std::to_string(b.taljare) + "/" + std::to_string(b.namnare) +
        " motsvarar " + procentText(b) + ". ";
    text += std::to_string(r.fyllda) + " av 100 rutor är färgade.";
    if (!r.exakt) text += " Rutmönstret är avrundat: 100 rutor kan inte visa detta bråk exakt.";
    return text;
}
std::string hundrarutaForklaringEn(const Hundraruta& r) {
    if (!giltigt(r.utgang)) return "No valid fraction to show.";
    const auto b = r.utgang;
    std::string text = std::to_string(b.taljare) + "/" + std::to_string(b.namnare) +
        " corresponds to " + procentText(b) + ". ";
    text += std::to_string(r.fyllda) + " out of 100 squares are filled.";
    if (!r.exakt) text += " The grid is rounded: 100 squares cannot represent this fraction exactly.";
    return text;
}
std::string hundrarutaEgenTextSv(int n) {
    if (!giltigHundraruta(n)) return "Ogiltigt antal rutor.";
    return std::to_string(n) + " av 100 rutor = " + std::to_string(n) + " %.";
}
std::string hundrarutaEgenTextEn(int n) {
    if (!giltigHundraruta(n)) return "Invalid number of squares.";
    return std::to_string(n) + " out of 100 squares = " + std::to_string(n) + " %.";
}
}
