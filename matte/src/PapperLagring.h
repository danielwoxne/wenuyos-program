// Varaktigt räknepapper, en liten GTK-fri modell för 2 GB-datorer.
// Sparas enbart lokalt i XDG-data; aldrig i programmets installationsmapp.
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace matte {

struct PapperCell { std::string t, liten; };
struct PapperStreck { int rad = 0, fran = 0, till = 0; };
struct PapperPunkt {
    double first = 0, second = 0, tryck = 1.0;
    bool tryckFinns = false;
};

struct PapperData {
    std::map<std::pair<int,int>, PapperCell> c;
    std::vector<PapperStreck> streck;
    std::vector<std::vector<PapperPunkt>> drag;
    int rad = 1, kol = 2, start = 2, lage = 0;
    bool liten = false, vanster = false;
    std::string id, kort, fraga;
    int mallIndex = -1, variantIndex = -1;
};

// Alla gränser verifieras innan data sparas eller läses (inklusive penna/mus).
constexpr size_t PAPPER_MAX_BYTE = 2U * 1024U * 1024U;
constexpr size_t PAPPER_MAX_PUNKTER = 50000;
constexpr size_t PAPPER_MAX_STRECK = 250;

bool giltigtPapperId(const std::string& id);
std::string nyttPapperId();
std::string papperMapp(const std::string& elev);
std::string papperFil(const std::string& elev, const std::string& id);
bool papperKontroll(const PapperData& p, std::string* fel = nullptr);
bool papperTillJson(const PapperData& p, std::string& ut, std::string* fel = nullptr);
bool papperFranJson(const std::string& text, PapperData& ut, std::string* fel = nullptr);
bool papperSpara(const std::string& elev, const PapperData& p, std::string* fel = nullptr);
bool papperLadda(const std::string& elev, const std::string& id, PapperData& ut, std::string* fel = nullptr);

} // namespace matte
