#include "FramstegOversikt.h"
#include <algorithm>
#include <limits>

namespace matte13 {
FramstegOversikt gorFramstegOversikt(int beharskar, int ovar, int lar,
                                     int repetition, int stjarnor) {
    FramstegOversikt o;
    o.beharskar = std::max(0, beharskar);
    o.ovar = std::max(0, ovar);
    o.lar = std::max(0, lar);
    o.repetition = std::max(0, repetition);
    o.stjarnor = std::max(0, stjarnor);
    const long long summa = static_cast<long long>(o.beharskar) + o.ovar + o.lar;
    o.besokta = static_cast<int>(std::min(summa, static_cast<long long>(std::numeric_limits<int>::max())));
    o.andel = summa > 0 ? static_cast<double>(o.beharskar) / summa : 0.0;
    return o;
}
}
