// Kursanpassat visuellt uttryck – oberoende av GTK och grafikdrivrutin.
// Pedagogiskt stöd, lättläst och svarsrättning påverkas aldrig av stilvalet.
#pragma once

#include <string>

namespace matte {

enum class VisuellNiva { Lekfull, Lugn, Akademisk, Professionell };

inline std::string normaliseraVisuellStil(const std::string& val) {
    if (val == "illustrerad" || val == "avskalad") return val;
    return "automatisk";  // äldre profiler och oväntade värden faller tillbaka säkert
}

inline unsigned visuellStilIndex(const std::string& val) {
    const auto stil = normaliseraVisuellStil(val);
    return stil == "illustrerad" ? 1U : stil == "avskalad" ? 2U : 0U;
}

inline const char* visuellStilVarde(unsigned index) {
    return index == 1 ? "illustrerad" : index == 2 ? "avskalad" : "automatisk";
}

inline VisuellNiva automatiskVisuellNiva(const std::string& stadium) {
    if (stadium == "forskola" || stadium == "lgr22.1-3") return VisuellNiva::Lekfull;
    if (stadium == "lgr22.4-6") return VisuellNiva::Lugn;
    if (stadium == "lgr22.7-9") return VisuellNiva::Akademisk;
    if (stadium.rfind("gy25.", 0) == 0) return VisuellNiva::Professionell;
    return VisuellNiva::Lugn;  // okänt stadium får ett neutralt gränssnitt
}

inline VisuellNiva visuellNiva(const std::string& stadium, const std::string& val) {
    const auto grund = automatiskVisuellNiva(stadium);
    const auto stil = normaliseraVisuellStil(val);
    if (stil == "avskalad") {
        return grund == VisuellNiva::Professionell ? grund : VisuellNiva::Akademisk;
    }
    if (stil == "illustrerad") {
        // Även gymnasieelever ska få mer bildstöd utan att få ett barnsligt tema.
        return grund == VisuellNiva::Akademisk || grund == VisuellNiva::Professionell
                   ? VisuellNiva::Lugn : VisuellNiva::Lekfull;
    }
    return grund;
}

inline const char* visuellNivaKlass(VisuellNiva niva) {
    switch (niva) {
        case VisuellNiva::Lekfull: return "matte-visuell-lekfull";
        case VisuellNiva::Lugn: return "matte-visuell-lugn";
        case VisuellNiva::Akademisk: return "matte-visuell-akademisk";
        case VisuellNiva::Professionell: return "matte-visuell-professionell";
    }
    return "matte-visuell-lugn";
}

} // namespace matte
