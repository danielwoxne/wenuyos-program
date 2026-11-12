#include "Mattekarta.h"

namespace matte13 {

KartaFilter kartaFilter(int valt) {
    switch (valt) {
        case 1: return KartaFilter::PaGang;
        case 2: return KartaFilter::Repetition;
        case 3: return KartaFilter::Beharskar;
        default: return KartaFilter::Alla;
    }
}

bool synligtKort(const KartaKort& kort, KartaFilter filter) {
    switch (filter) {
        case KartaFilter::Alla: return true;
        case KartaFilter::PaGang: return kort.nasta || kort.lage == KartaLage::Ovar;
        case KartaFilter::Repetition: return kort.repetition;
        case KartaFilter::Beharskar: return kort.lage == KartaLage::Beharskar;
    }
    return true;
}

KartaSummering summeraKarta(const std::vector<KartaKort>& kort) {
    KartaSummering s;
    for (const auto& k : kort) {
        ++s.totalt;
        switch (k.lage) {
            case KartaLage::Lar: ++s.nya; break;
            case KartaLage::Ovar: ++s.ovar; break;
            case KartaLage::Beharskar: ++s.beharskar; break;
        }
        if (k.repetition) ++s.repetition;
        if (k.nasta) ++s.nasta;
    }
    // Ingen division med noll och inga syntetiska framsteg vid nystart.
    s.andel = s.totalt ? static_cast<double>(s.beharskar) / s.totalt : 0.0;
    return s;
}

int antalSynliga(const std::vector<KartaKort>& kort, KartaFilter filter) {
    int result = 0;
    for (const auto& k : kort) if (synligtKort(k, filter)) ++result;
    return result;
}

} // namespace matte13
