// Matte 1.3, etapp 7. Ren C++17-modell för elevens lärandekarta.
// Ingen GTK, ingen filåtkomst, ingen nätverksanslutning och ingen ändring av elevdata.
#pragma once
#include <vector>

namespace matte13 {

enum class KartaFilter : int { Alla = 0, PaGang = 1, Repetition = 2, Beharskar = 3 };
enum class KartaLage : int { Lar = 0, Ovar = 1, Beharskar = 2 };

struct KartaKort {
    KartaLage lage = KartaLage::Lar;
    bool nasta = false;
    bool repetition = false;
};

struct KartaSummering {
    int totalt = 0;
    int nya = 0;
    int ovar = 0;
    int beharskar = 0;
    int repetition = 0;
    int nasta = 0;
    double andel = 0.0; // andel av alla tillgängliga regelkort i valt stadium
};

KartaFilter kartaFilter(int valt);
bool synligtKort(const KartaKort& kort, KartaFilter filter);
KartaSummering summeraKarta(const std::vector<KartaKort>& kort);
int antalSynliga(const std::vector<KartaKort>& kort, KartaFilter filter);

} // namespace matte13
