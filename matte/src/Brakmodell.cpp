#include "Brakmodell.h"
#include <algorithm>
#include <iomanip>
#include <regex>
#include <sstream>

namespace matte13 {
bool giltigt(Brak b) {
    return b.namnare >= 2 && b.namnare <= 12 && b.taljare >= 0 && b.taljare <= b.namnare;
}

bool hittaBrak(const std::string& lektion, const std::string& fraga, Brak& ut) {
    // Läs inte tal som datum, division eller decimaler utanför en bråklektion.
    const bool braklektion = lektion.find("Bråk") != std::string::npos ||
                            lektion.find("bråk") != std::string::npos ||
                            lektion.find("Fraction") != std::string::npos ||
                            lektion.find("fraction") != std::string::npos;
    if (!braklektion) return false;
    const std::regex form(R"((^|[^0-9])([0-9]{1,2})[ \t]*/[ \t]*([0-9]{1,2})([^0-9]|$))");
    for (auto pos = std::sregex_iterator(fraga.begin(), fraga.end(), form);
         pos != std::sregex_iterator(); ++pos) {
        Brak b{std::stoi((*pos)[2].str()), std::stoi((*pos)[3].str())};
        if (giltigt(b)) { ut = b; return true; }
    }
    return false;
}

std::string procentText(Brak b) {
    if (!giltigt(b)) return "";
    const int hundradelar = b.taljare * 100;
    if (hundradelar % b.namnare == 0)
        return std::to_string(hundradelar / b.namnare) + " %";
    // En decimal räcker när den är exakt, annars två eller avrundat en decimal.
    const int tiondelar = b.taljare * 1000;
    const int hundradelarAvProcent = b.taljare * 10000;
    const bool enDecimalExakt = tiondelar % b.namnare == 0;
    const bool tvaDecimalerExakt = hundradelarAvProcent % b.namnare == 0;
    std::ostringstream o;
    if (enDecimalExakt) o << std::fixed << std::setprecision(1);
    else if (tvaDecimalerExakt) o << std::fixed << std::setprecision(2);
    else o << std::fixed << std::setprecision(1);
    o << (100.0 * b.taljare / b.namnare);
    std::string t = o.str();
    std::replace(t.begin(), t.end(), '.', ',');
    return (enDecimalExakt || tvaDecimalerExakt ? "" : "≈ ") + t + " %";
}

std::string beskrivningSv(Brak b) {
    if (!giltigt(b)) return "";
    return std::to_string(b.taljare) + " av " + std::to_string(b.namnare) +
           " lika stora delar är färgade. Det motsvarar " + procentText(b) + ".";
}
}
