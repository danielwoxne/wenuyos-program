#include "Bedom.h"

#include "Algebra.h"
#include "Tal.h"

#include <glib.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace matte {

namespace {

bool baraBokstaver(const std::string& s) {   // som Pythons s.replace(" ", "").isalpha()
    bool nagon = false;
    for (const char* p = s.c_str(); *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (c == ' ') continue;
        if (!g_unichar_isalpha(c)) return false;
        nagon = true;
    }
    return nagon;
}

Brak brakUr(const std::string& nyckel) {     // "t:3/4" → 3/4
    auto s = nyckel.find('/');
    return Brak::ur(std::stoll(nyckel.substr(2, s - 2)), std::stoll(nyckel.substr(s + 1)));
}

std::vector<std::string> dela(const std::string& s, char c) {
    std::vector<std::string> ut;
    std::string del;
    std::istringstream in(s);
    while (std::getline(in, del, c)) ut.push_back(del);
    if (!s.empty() && s.back() == c) ut.push_back("");
    return ut;
}

// "c:blå:1|röd:2" – hur många av varje färg
std::string rakning(const std::vector<std::string>& farger) {
    std::map<std::string, int> n;
    for (auto& f0 : farger) {
        std::string f = gemener(trim(f0));
        if (f.empty()) continue;
        auto k = f.find(':');
        if (k != std::string::npos) n[trim(f.substr(0, k))] += atoi(f.c_str() + k + 1);
        else n[f] += 1;
    }
    std::string r = "c:";
    bool forsta = true;
    for (auto& [namn, antal] : n) if (antal > 0) { r += (forsta ? "" : "|") + namn + ":" + std::to_string(antal); forsta = false; }
    return r;
}

}  // namespace

// Hur många tecken som måste ändras för att a ska bli b (Levenshtein, räknat i tecken – inte byte)
int redigeringsavstand(const std::string& a, const std::string& b) {
    std::vector<gunichar> x, y;
    for (const char* p = a.c_str(); *p; p = g_utf8_next_char(p)) x.push_back(g_utf8_get_char(p));
    for (const char* p = b.c_str(); *p; p = g_utf8_next_char(p)) y.push_back(g_utf8_get_char(p));
    std::vector<int> rad(y.size() + 1);
    for (size_t j = 0; j <= y.size(); ++j) rad[j] = (int)j;
    for (size_t i = 1; i <= x.size(); ++i) {
        int forra = rad[0];
        rad[0] = (int)i;
        for (size_t j = 1; j <= y.size(); ++j) {
            int tmp = rad[j];
            rad[j] = std::min({rad[j] + 1, rad[j - 1] + 1, forra + (x[i - 1] == y[j - 1] ? 0 : 1)});
            forra = tmp;
        }
    }
    return rad[y.size()];
}

const char* const FARGER[6] = {"röd", "blå", "gul", "grön", "orange", "lila"};

bool nyTyp(const std::string& t) { return t == "kryssa" || t == "para" || t == "mala" || t == "rita"; }

std::string nyNyckel(const std::string& typ, const std::vector<std::string>& svar, const std::string& facit0) {
    if (typ == "kryssa") {
        std::set<std::string> m;
        for (auto& s : svar) if (!trim(s).empty()) m.insert(textnorm(s));
        std::string r = "k:";
        bool forsta = true;
        for (auto& x : m) { r += (forsta ? "" : "|") + x; forsta = false; }
        return r;
    }
    if (typ == "para") {
        std::string r = "q:";
        for (size_t i = 0; i < svar.size(); ++i) r += (i ? "|" : "") + textnorm(svar[i]);
        return r;
    }
    if (typ == "mala") {
        if (facit0.rfind("c:", 0) == 0) return rakning(svar);
        std::string r = "m:";
        for (size_t i = 0; i < svar.size(); ++i) r += (i ? "|" : "") + gemener(trim(svar[i]));
        return r;
    }
    return "";
}

// kryssa, para, mala, rita – samma regler och texter som _bedom_ny i innehall/matte/uppgifter.py
static Bedomning bedomNy(const Mall& m, const Variant& v, const std::vector<std::string>& svar) {
    Bedomning b;
    if (m.typ == "rita") { b.ratt = true; b.text = "Sparad."; return b; }
    std::string fore = v.ledtrad.empty() ? "" : v.ledtrad + " ";
    const std::string facit0 = v.facit.empty() ? "" : v.facit[0];
    auto feltanke = [&](const std::string& nyckel) -> const Feltanke* {
        for (auto& f : v.fel) if (f.nyckel == nyckel) return &f;
        return nullptr;
    };
    auto satt = [&](const Feltanke* f) { b.feltanke = true; b.text = f->text; b.feltankeId = f->id; };
    if (m.typ == "kryssa") {
        std::vector<std::string> valda;
        for (auto& s : svar) if (!trim(s).empty()) valda.push_back(s);
        if (valda.empty()) { b.otolkbart = true; b.text = "Kryssa i minst en ruta."; return b; }
        if (nyNyckel("kryssa", valda) == facit0) { b.ratt = true; b.text = "Rätt."; return b; }
        std::set<std::string> ratt, valdaN;
        for (auto& x : v.facitSvar) ratt.insert(textnorm(x));
        for (auto& x : valda) valdaN.insert(textnorm(x));
        for (size_t i = 0; i < v.alternativ.size(); ++i)
            if (valdaN.count(textnorm(v.alternativ[i])) && !ratt.count(textnorm(v.alternativ[i]))) b.felFalt.push_back((int)i);
        for (int i : b.felFalt) if (auto* f = feltanke("x:" + textnorm(v.alternativ[i]))) { satt(f); return b; }
        if (!b.felFalt.empty()) {
            b.text = fore + (b.felFalt.size() == 1 ? "Ett kryss är fel." : std::to_string(b.felFalt.size()) + " kryss är fel.");
            return b;
        }
        int hittade = 0;
        for (auto& x : valdaN) hittade += ratt.count(x);
        b.text = fore + "Det finns fler som stämmer – du har hittat " + std::to_string(hittade) + " av " + std::to_string(ratt.size()) + ".";
        return b;
    }
    if (m.typ == "para") {
        bool tom = svar.size() != v.falt.size();
        for (auto& s : svar) tom |= trim(s).empty();
        if (tom) { b.otolkbart = true; b.text = "Dra en linje från varje sak till vänster."; return b; }
        if (nyNyckel("para", svar) == facit0) { b.ratt = true; b.text = "Rätt."; return b; }
        for (size_t i = 0; i < svar.size() && i < v.facitSvar.size(); ++i)
            if (textnorm(svar[i]) != textnorm(v.facitSvar[i])) b.felFalt.push_back((int)i);
        for (int i : b.felFalt) if (auto* f = feltanke("x:" + textnorm(v.falt[i]) + "→" + textnorm(svar[i]))) { satt(f); return b; }
        b.text = fore + (b.felFalt.size() == 1 ? "En linje är fel." : std::to_string(b.felFalt.size()) + " linjer är fel.");
        return b;
    }
    // mala
    bool nagot = false;
    for (auto& s : svar) nagot |= !trim(s).empty();
    if (!nagot) { b.otolkbart = true; b.text = "Måla något först."; return b; }
    std::string k = nyNyckel("mala", svar, facit0);
    if (k == facit0) { b.ratt = true; b.text = "Rätt."; return b; }
    if (auto* f = feltanke(k)) { satt(f); return b; }
    if (facit0.rfind("c:", 0) == 0) { b.text = fore + "Räkna hur många delar varje färg ska ha."; return b; }
    for (size_t i = 0; i < v.facitSvar.size(); ++i)
        if (i >= svar.size() || gemener(trim(svar[i])) != v.facitSvar[i]) b.felFalt.push_back((int)i);
    b.text = fore + (b.felFalt.size() == 1 ? "En del har fel färg." : std::to_string(b.felFalt.size()) + " delar har fel färg.");
    return b;
}

std::string svarsnyckel(const std::string& svar) {
    std::string s = trim(svar);
    if (g_utf8_strlen(s.c_str(), -1) >= 2 && baraBokstaver(s)) return "o:" + gemener(s);   // ett ord: ja, större, triangel
    try { return tolka(s).nyckel(); } catch (const Fel&) {}
    try {
        Polynom p = tolkaUttryck(s);
        if (arKonstant(p)) return (p.count({}) ? p.at({}) : Brak(0)).nyckel();
        return "p:" + skriv(p);
    } catch (const Fel&) {}
    return "o:" + gemener(s);
}

std::string mangd(const std::string& text) {
    std::vector<std::string> d;
    for (auto& x : dela(text, ',')) { std::string t = gemener(trim(x)); if (!t.empty()) d.push_back(t); }
    std::sort(d.begin(), d.end());
    std::string r;
    for (size_t i = 0; i < d.size(); ++i) r += (i ? "," : "") + d[i];
    return r;
}

std::string textnorm(const std::string& s) {
    std::string r;
    for (char c : s) { if (c == ' ') continue; r += c == ',' ? '.' : c; }
    return gemener(r);
}

Bedomning bedom(const Mall& m, const Variant& v, const std::vector<std::string>& svarIn) {
    if (nyTyp(m.typ)) return bedomNy(m, v, svarIn);
    Bedomning b;
    std::vector<std::string> svar = svarIn;
    auto ledtradEller = [&](const char* std_) { return v.ledtrad.empty() ? std::string(std_) : v.ledtrad; };
    // Först alternativet exakt som det står, sedan samma värde – avrundade svar (gymnasiet) räknas
    // som samma feltanke om de ligger inom uppgiftens tolerans.
    auto hittaFel = [&](const std::string& nyckel, const std::string& text = "") -> const Feltanke* {
        if (!text.empty())
            for (auto& f : v.fel) if (!f.textnyckel.empty() && f.textnyckel == text) return &f;
        for (auto& f : v.fel) if (f.nyckel == nyckel) return &f;
        if (nyckel.size() > 2 && nyckel[0] == 't' && nyckel.find(';') == std::string::npos) {
            double a = brakUr(nyckel).tal(), grans = std::max(m.tolerans, 1e-9 * std::max(1.0, std::fabs(a)));
            for (auto& f : v.fel)
                if (f.nyckel.size() > 2 && f.nyckel[0] == 't' && f.nyckel.find(';') == std::string::npos &&
                    std::fabs(brakUr(f.nyckel).tal() - a) <= grans + 1e-12) return &f;
        }
        return nullptr;
    };
    auto feltanke = [&](const Feltanke* f, std::vector<int> falt) {
        b.ratt = false; b.feltanke = true; b.text = f->text; b.feltankeId = f->id; b.felFalt = std::move(falt);
        return b;
    };

    if (m.typ == "linje" && std::find(svar.begin(), svar.end(), "lodrät") != svar.end()) {
        b.otolkbart = true;
        b.text = "Den linjen är lodrät – den har inget k. Linjen y = kx + m lutar, välj två punkter med olika x.";
        return b;
    }
    if (m.typ == "valj" || m.typ == "markera") {   // klick i en målning eller ikonrad: jämför namnen
        std::string forsta = svar.empty() ? "" : svar[0];
        std::string nyckel = m.typ == "markera" ? mangd(forsta) : gemener(trim(forsta));
        if (nyckel.empty()) {
            b.otolkbart = true;
            b.text = m.typ == "markera" ? "Klicka på det du vill markera först." : "Klicka på något i bilden först.";
            return b;
        }
        if (!v.facit.empty() && "o:" + nyckel == v.facit[0]) { b.ratt = true; b.text = "Rätt."; return b; }
        if (auto* f = hittaFel("o:" + nyckel)) return feltanke(f, {0});
        if (m.typ == "markera" && !v.facit.empty()) {
            size_t ratt = dela(v.facit[0].substr(2), ',').size(), antal = dela(nyckel, ',').size();
            if (ratt != antal) {
                b.text = (v.ledtrad.empty() ? "" : v.ledtrad + " ") + "Du markerade " + std::to_string(antal) +
                         " – det ska vara " + std::to_string(ratt) + ".";
                b.felFalt = {0};
                return b;
            }
        }
        b.text = ledtradEller("Inte riktigt. Titta på bilden igen.");
        b.felFalt = {0};
        return b;
    }
    if (m.typ == "val") {   // flerval: jämför med alternativet som det står
        std::string val = svar.empty() ? "" : svar[0];
        if (!v.facitText.empty() && textnorm(val) == textnorm(v.facitText[0])) { b.ratt = true; b.text = "Rätt."; return b; }
        if (auto* f = hittaFel(svarsnyckel(val), "x:" + textnorm(val))) return feltanke(f, {0});
        b.text = ledtradEller("Inte riktigt. Titta på kortet och försök igen.");
        b.felFalt = {0};
        return b;
    }
    if (svar.size() != v.facit.size()) { b.otolkbart = true; b.text = "Fyll i alla fält."; return b; }

    std::vector<std::string> nycklar;
    for (size_t i = 0; i < svar.size(); ++i) {
        std::string s = trim(svar[i]);
        if (s.empty()) { b.otolkbart = true; b.text = "Skriv ett svar."; return b; }
        std::string k = svarsnyckel(s);
        const std::string& f = v.facit[i];
        if ((f[0] == 't' || f[0] == 'p') && k[0] == 'o') {
            b.otolkbart = true;
            b.text = "Jag förstår inte svaret '" + s + "'. Skriv t.ex. " + (f[0] == 'p' ? "2x + 3." : "3/4, 0,75 eller 75 %.");
            return b;
        }
        nycklar.push_back(k);
    }
    auto lika = [&](const std::string& svarK, const std::string& facitK) {
        if (m.tolerans > 0 && svarK[0] == 't' && facitK[0] == 't')
            return std::fabs(brakUr(svarK).tal() - brakUr(facitK).tal()) <= m.tolerans + 1e-12;
        return svarK == facitK;
    };
    std::vector<int> fel;
    if (m.oordnad && nycklar.size() > 1) {   // x₁ och x₂ i vilken ordning som helst
        std::vector<std::string> kvar = v.facit;
        for (size_t i = 0; i < nycklar.size(); ++i) {
            auto it = std::find_if(kvar.begin(), kvar.end(), [&](const std::string& f) { return lika(nycklar[i], f); });
            if (it == kvar.end()) fel.push_back((int)i); else kvar.erase(it);
        }
    } else {
        for (size_t i = 0; i < nycklar.size(); ++i) if (!lika(nycklar[i], v.facit[i])) fel.push_back((int)i);
    }
    if (fel.empty()) {
        b.ratt = true; b.text = "Rätt.";
        std::string skrivet = trim(svar[0]);
        if (nycklar.size() == 1 && v.facit[0][0] == 't' && !v.facitText.empty() && skrivet != v.facitText[0] &&
            textnorm(skrivet) != textnorm(v.facitText[0]))
            b.text = "Rätt – " + skrivet + " är samma tal som " + v.facitText[0] + ".";
        return b;
    }
    // Ett ord som är nästan rätt stavat räknas – det här är matte, inte stavning ("kvadratt" → kvadrat)
    if (nycklar.size() == 1 && v.facit[0][0] == 'o' && nycklar[0][0] == 'o' && !v.facitText.empty() && !hittaFel(nycklar[0])) {
        std::string a = nycklar[0].substr(2), f = v.facit[0].substr(2);
        long la = g_utf8_strlen(f.c_str(), -1);
        if (la >= 5 && redigeringsavstand(a, f) <= (la >= 8 ? 2 : 1)) {   // korta ord måste stämma (udda/jämn)
            b.ratt = true;
            b.text = "Rätt! Det stavas " + v.facitText[0] + ".";
            return b;
        }
    }
    // Siffrorna i omvänd ordning: 21 i stället för 12, 153 i stället för 135 är inte samma sak men 351 är
    if (nycklar.size() == 1 && v.facit[0][0] == 't' && nycklar[0][0] == 't' && !v.facitText.empty() && !hittaFel(nycklar[0])) {
        std::string skrivet = trim(svar[0]), facitText = v.facitText[0];
        std::string rs(skrivet.rbegin(), skrivet.rend());
        bool siffror = skrivet.size() >= 2 && std::all_of(skrivet.begin(), skrivet.end(), ::isdigit) &&
                       std::all_of(facitText.begin(), facitText.end(), ::isdigit);
        if (siffror && rs == facitText && skrivet != facitText) {
            b.omkastad = true;
            b.felFalt = {0};
            b.text = "Siffrorna har bytt plats. Titta en gång till: vilken siffra kommer först?";
            return b;
        }
    }
    if (nycklar.size() > 1) {
        std::string tupel;
        for (size_t i = 0; i < nycklar.size(); ++i) tupel += (i ? ";" : "") + nycklar[i];
        if (auto* f = hittaFel(tupel)) return feltanke(f, fel);
    }
    if (fel[0] == 0) if (auto* f = hittaFel(nycklar[0])) return feltanke(f, fel);
    b.felFalt = fel;
    if (v.facit.size() > 1)
        b.text = ledtradEller(("Fel i " + std::to_string(fel.size()) + " av " + std::to_string(v.facit.size()) + " fält. Titta på kortet.").c_str());
    else
        b.text = ledtradEller("Inte riktigt. Titta på kortet och försök igen.");
    return b;
}

}  // namespace matte
