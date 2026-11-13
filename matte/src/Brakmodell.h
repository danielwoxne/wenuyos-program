#pragma once
#include <string>

namespace matte13 {
struct Brak { int taljare = 3; int namnare = 4; };

// Begränsad undervisningsmodell: 0 <= täljare <= nämnare, 2..12 delar.
// Påverkar inte bedömning av elevsvar eller elevens sparade data.
bool hittaBrak(const std::string& lektion, const std::string& fraga, Brak& ut);
bool giltigt(Brak b);
std::string procentText(Brak b);   // svensk decimal med komma, ex. 3/4 = 75 %
std::string beskrivningSv(Brak b);
}
