// Etapp 2: kryssa i rutor, dra linjer mellan par, måla ytor, fri rityta (se SvarNy.cpp)
#pragma once
#include "Svar.h"

namespace matte {

Svarsyta kryssaYta(const Data& d, const Variant& v, std::function<void(std::vector<std::string>)> vidSvar);
Svarsyta paraYta(const Data& d, const Variant& v, std::function<void(std::vector<std::string>)> vidSvar);
Svarsyta malaYta(const Data& d, const Variant& v, std::function<void(std::vector<std::string>)> vidSvar);
// kompakt = ritrutan i en skiss-uppgift (utan Klar-knapp)
Svarsyta ritYta(const Data& d, const Variant& v, bool kompakt, std::function<void(std::vector<std::string>)> vidSvar);

}  // namespace matte
