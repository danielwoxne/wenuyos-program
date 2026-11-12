// Programmets innehåll: kort, läroplan och färdiga uppgifter (share/se.wenuyos.app.matte/).
// Datan skrivs av verktyg/expandera.py ur korten i innehall/data/kort/*.toml.
#pragma once
#include "Json.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace matte {

struct Rad {
    std::string sager, kommentar;
    Json figur;
    std::vector<std::string> skriver;
};

struct Exempel {
    std::string rubrik, text;
    Json figur;
    std::vector<std::string> steg;
};

struct Kort {
    std::string id, titel, omrade, ikon, bild, bildtext, sager, galler, galler_inte, kalla;
    std::vector<Rad> rader;
    std::vector<Exempel> exempel;
    std::vector<std::string> forkunskap, ateruppta;
    std::map<std::string, std::string> feltankar;   // feltanke-id → förklaring utan tal (lärarens översikt)
};

struct Mal {
    std::string id, stadium, omrade, text;
    std::vector<std::string> kort;
};

struct Feltanke {
    std::string nyckel;        // värdets nyckel: t:3/4, p:x² + 1, o:blå, flera fält med ;
    std::string textnyckel;    // för flerval: alternativet som det står (x:…)
    std::string text;          // förklaringen eleven får läsa
    std::string id;            // "<mall>|<felsvar>" – samma tankefel får samma id
    std::string felsvar;       // felsvaret som text (bara för testerna)
};

struct Variant {               // en färdig uppgift
    std::string fraga, ledtrad;
    std::vector<std::string> steg, losning, alternativ, falt, facit, facitText;
    std::vector<std::string> facitSvar;   // kryssa/para/mala: facit som elevens svar (en sträng per sak)
    Json figur;
    std::vector<Feltanke> fel;
};

struct Mall {
    int niva = 2;
    std::string typ = "text", form = "auto";
    double tolerans = 0;
    bool oordnad = false;
    std::vector<Variant> varianter;
};

// Ett ord i ordlistan (data/ordlista.toml): matteorden förklarade med bild
struct Ord {
    std::string ord, forklaring, exempel;
    std::vector<std::string> former, se;
    Json figur;
};

class Data {
public:
    bool las(const std::string& mapp, std::string* fel = nullptr);
    static std::string hittaMapp();          // bredvid programfilen: <bin>/../share/se.wenuyos.app.matte

    const std::string& mapp() const { return mapp_; }
    std::string bild(const std::string& namn) const { return mapp_ + "/bilder/" + namn; }
    std::string ikon(const std::string& namn) const;

    const std::vector<Kort>& kort() const { return kort_; }
    const Kort* kort(const std::string& id) const;
    const std::vector<Mal>& laroplan() const { return mal_; }
    const std::vector<std::pair<std::string, std::string>>& stadier() const { return stadier_; }
    std::string omradeNamn(const std::string& id) const;
    const std::vector<std::string>& omradeOrdning() const { return omradeOrdning_; }

    const std::vector<Mall>& mallar(const std::string& kortId) const;   // läses första gången kortet övas

    const std::vector<Ord>& ordlista() const { return ord_; }
    int ordFor(const std::string& form) const;   // "täljaren" → index i ordlistan, -1 om ordet inte finns
    int ordMedNamn(const std::string& ord) const;

private:
    std::string mapp_;
    std::vector<Kort> kort_;
    std::map<std::string, size_t> index_;
    std::vector<Mal> mal_;
    std::vector<std::pair<std::string, std::string>> stadier_;
    std::map<std::string, std::string> omraden_;
    std::vector<std::string> omradeOrdning_;
    mutable std::map<std::string, std::vector<Mall>> mallar_;
    std::vector<Ord> ord_;
    std::map<std::string, int> former_;
};

// En uppgift att visa: vilket kort, vilken mall, vilken variant
struct Uppgift {
    const Kort* kort = nullptr;
    const Mall* mall = nullptr;
    const Variant* v = nullptr;
    int mallIndex = 0;
};

}  // namespace matte
