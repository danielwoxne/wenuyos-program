// Skrivplatta: begränsa stylustryck till ett stabilt pennstreck.
// Ingen grafik-/drivrutinskoppling här: modellen kan testas utan display.
#pragma once
#include <algorithm>
#include <cmath>
namespace matte {
inline double penntryck(double tryck, bool harTryck) {
    if (!harTryck || !std::isfinite(tryck)) return 1.0;
    return std::clamp(tryck, 0.0, 1.0);
}
inline double pennbredd(double bas, double tryck, bool harTryck) {
    return bas * (harTryck ? (0.45 + 1.1 * penntryck(tryck, true)) : 1.0);
}
}  // namespace matte
