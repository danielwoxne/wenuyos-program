#include "LarandeUtforska.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace matte13 {
bool giltig(Tallinje l) {
    return std::isfinite(l.min) && std::isfinite(l.max) && std::isfinite(l.steg) &&
           std::isfinite(l.markerat) && l.max > l.min && l.steg > 0 &&
           (l.max - l.min) / l.steg <= 200;
}
double snappa(Tallinje l, double tal) {
    if (!giltig(l) || !std::isfinite(tal)) return l.min;
    double n = std::round((std::clamp(tal, l.min, l.max) - l.min) / l.steg);
    const double r = std::clamp(l.min + n * l.steg, l.min, l.max);
    return std::abs(r) < 1e-9 ? 0.0 : r;
}
double andel(Tallinje l, double tal) {
    if (!giltig(l) || !std::isfinite(tal)) return 0.0;
    return std::clamp((tal - l.min) / (l.max - l.min), 0.0, 1.0);
}
std::string svTal(double v) {
    if (!std::isfinite(v)) return "?";
    if (std::abs(v) < 0.0000001) v = 0;
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << v;
    std::string s = out.str();
    while (s.find('.') != std::string::npos && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    std::replace(s.begin(), s.end(), '.', ',');
    return s;
}
bool hittaTallinje(const std::string& lektion, const std::string& fraga, Tallinje& ut) {
    // Visa bara när kopplingen till en tallinje är tydlig. Allt är en frivillig illustration.
    auto har = [](const std::string& s, const std::string& word) { return s.find(word) != std::string::npos; };
    const bool linje = har(lektion, "tallinj") || har(lektion, "Tallinj") ||
                       har(fraga, "tallinj") || har(fraga, "Tallinj") ||
                       har(lektion, "Number line") || har(fraga, "number line");
    const bool negativ = har(lektion, "egativa tal") || har(fraga, "egativa tal") ||
                         har(lektion, "negative number") || har(fraga, "negative number");
    if (!linje && !negativ) return false;
    if (negativ) ut = Tallinje{-10.0, 10.0, 1.0, -3.0};
    else if (har(lektion, "ecimal") || har(fraga, "ecimal")) ut = Tallinje{0.0, 5.0, 0.5, 2.5};
    else ut = Tallinje{0.0, 20.0, 1.0, 5.0};
    return true;
}
bool giltig(Geometri g) {
    return std::isfinite(g.bredd) && std::isfinite(g.hojd) &&
           g.bredd >= 1 && g.bredd <= 20 && g.hojd >= 1 && g.hojd <= 20;
}
double area(Geometri g) {
    if (!giltig(g)) return 0;
    return g.bredd * g.hojd * (g.form == Geometriform::RatvinkligTriangel ? 0.5 : 1.0);
}
double omkrets(Geometri g) {
    if (!giltig(g)) return 0;
    return g.form == Geometriform::Rektangel ? 2 * (g.bredd + g.hojd)
             : g.bredd + g.hojd + std::hypot(g.bredd, g.hojd);
}
bool arGeometrilektion(const std::string& lektion) {
    return lektion.find("eometr") != std::string::npos ||
           lektion.find("eometry") != std::string::npos ||
           lektion.find("mkrets") != std::string::npos ||
           lektion.find("rea") != std::string::npos ||
           lektion.find("ektangel") != std::string::npos ||
           lektion.find("riangel") != std::string::npos ||
           lektion.find("erimeter") != std::string::npos;
}
} // namespace matte13
