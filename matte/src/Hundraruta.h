// Matte 1.3, etapp 5: förhållandet mellan bråk, procent och hundraruta.
// Ren C++17, helt fristående från rättning och elevdata.
#pragma once
#include "Brakmodell.h"
#include <string>

namespace matte13 {
struct Hundraruta {
    int fyllda = 0;                 // 0..100, avrundat till närmaste ruta
    bool exakt = false;            // Sant bara när bråket exakt kan visas med 100 rutor
    Brak utgang;                    // Bråket som visades när laborationen öppnades
};
Hundraruta hundrarutaFranBrak(Brak b);
bool giltigHundraruta(int fyllda);
std::string hundrarutaForklaringSv(const Hundraruta& ruta);
std::string hundrarutaForklaringEn(const Hundraruta& ruta);
// 0..100 motsvarar exakt detta antal rutor när eleven målar manuellt.
std::string hundrarutaEgenTextSv(int fyllda);
std::string hundrarutaEgenTextEn(int fyllda);
// Kolumn/ rad anges med 0..9; klick på ruta n färgar de första n+1 rutorna.
int rutorTillOchMed(int kolumn, int rad);
}
