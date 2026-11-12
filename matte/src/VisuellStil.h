#pragma once
// Matte 1.3 etapp 8: färger och geometri för vektorbilder, utan GTK4 beroende.
// Kontrast: färg + mörk kontur + mönster. Färger får aldrig ensamma bära betydelse.
#include <algorithm>
#include <cmath>

namespace matte13 {
struct RGB { double r, g, b; };
struct IllustrationsPalett { RGB papper, aktiv, tom, kontur, morkText, struktur; };

inline IllustrationsPalett illustrationsPalett() {
    return {{0.985, 0.989, 1.0}, {0.265, 0.295, 0.675},
            {0.936, 0.946, 0.975}, {0.155, 0.185, 0.29},
            {0.12, 0.16, 0.24}, {0.97, 0.98, 1.0}};
}
inline double luminans(RGB color) {
    auto linear = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c+.055)/1.055, 2.4); };
    return .2126*linear(color.r) + .7152*linear(color.g) + .0722*linear(color.b);
}
inline double kontrastforhallande(RGB a, RGB b) {
    const double x = luminans(a), y = luminans(b);
    return (std::max(x,y)+.05)/(std::min(x,y)+.05);
}

struct BrakPlacering {
    double cirkelX, cirkelY, radie, stapelX, stapelY, stapelBredd, stapelHojd;
    bool vertikal;
};
inline BrakPlacering brakPlacering(int bredd, int hojd) {
    const double w = std::max(160,bredd), h = std::max(140,hojd);
    const bool staplad = w < 400;
    if (staplad) {
        const double radie = std::min({w*.22,h*.22,70.0});
        const double bw = std::max(50.0,w-40.0);
        return {w*.5,h*.30,radie, (w-bw)*.5,h*.71,bw, std::min(43.0,h*.18),true};
    }
    const double content = std::min(760.0,w-30.0), left = (w-content)*.5;
    const double radie = std::min({content*.17,h*.34,84.0});
    const double bw = content*.49;
    return {left+content*.23,h*.50,radie,left+content*.48,h*.50-26.0,bw,52.0,false};
}
} // namespace matte13
