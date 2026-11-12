// En liten JSON-läsare och -skrivare – för programmets data (kort, uppgifter) och elevernas framsteg.
// Inga beroenden utöver standardbiblioteket. Objekt behåller ordningen (skrivs i samma ordning).
#pragma once
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace matte {

class Json {
public:
    enum Typ { Null, Bool, Tal, Text, Lista, Objekt };

    Json() = default;
    Json(bool b) : typ_(Bool), b_(b) {}
    Json(int v) : typ_(Tal), d_(v) {}
    Json(long v) : typ_(Tal), d_((double)v) {}
    Json(long long v) : typ_(Tal), d_((double)v) {}
    Json(double v) : typ_(Tal), d_(v) {}
    Json(const char* s) : typ_(Text), s_(s) {}
    Json(std::string s) : typ_(Text), s_(std::move(s)) {}
    static Json lista() { Json j; j.typ_ = Lista; return j; }
    static Json objekt() { Json j; j.typ_ = Objekt; return j; }

    static Json las(const std::string& text);          // kastar std::runtime_error
    static Json lasFil(const std::string& sokvag);     // tom Json (Null) om filen saknas
    std::string skriv(bool snyggt = false) const;

    Typ typ() const { return typ_; }
    bool null() const { return typ_ == Null; }
    bool arText() const { return typ_ == Text; }
    bool arTal() const { return typ_ == Tal; }
    bool arLista() const { return typ_ == Lista; }
    bool arObjekt() const { return typ_ == Objekt; }

    // Läsning med standardvärden – trasig eller saknad data ska aldrig krascha programmet
    std::string str(const std::string& std_ = "") const;   // tal skrivs som text
    double tal(double std_ = 0) const;
    int heltal(int std_ = 0) const { return (int)tal(std_); }
    bool sant(bool std_ = false) const { return typ_ == Bool ? b_ : std_; }

    const Json& operator[](const std::string& nyckel) const;
    const Json& operator[](size_t i) const;
    Json& satt(const std::string& nyckel, Json v);
    void lagg(Json v) { typ_ = Lista; l_.push_back(std::move(v)); }
    bool har(const std::string& nyckel) const;
    size_t storlek() const { return typ_ == Lista ? l_.size() : typ_ == Objekt ? o_.size() : 0; }
    const std::vector<Json>& element() const { return l_; }
    const std::vector<std::pair<std::string, Json>>& falt() const { return o_; }
    std::vector<std::string> strangar() const;              // ["a","b"] → {"a","b"}

private:
    Typ typ_ = Null;
    bool b_ = false;
    double d_ = 0;
    std::string s_;
    std::vector<Json> l_;
    std::vector<std::pair<std::string, Json>> o_;
    friend struct JsonLasare;
};

}  // namespace matte
