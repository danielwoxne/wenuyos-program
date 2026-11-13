#include "Algebra.h"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace matte {

namespace {

struct Tok {
    enum S { Tal, Var, Op, Slut } s = Slut;
    std::string v;
};

std::vector<Tok> tokenisera(const std::string& text) {
    std::vector<Tok> ut;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char c = text[i];
        if (c == ' ' || c == '\t') { ++i; continue; }
        if (isdigit(c)) {
            std::string t;
            while (i < text.size() && isdigit((unsigned char)text[i])) t += text[i++];
            if (i + 1 < text.size() && (text[i] == ',' || text[i] == '.') && isdigit((unsigned char)text[i + 1])) {
                t += '.'; ++i;
                while (i < text.size() && isdigit((unsigned char)text[i])) t += text[i++];
            }
            ut.push_back({Tok::Tal, t});
            continue;
        }
        if (isalpha(c)) {   // en bokstav, ev. en siffra direkt efter (x1)
            std::string v(1, (char)c); ++i;
            if (i < text.size() && isdigit((unsigned char)text[i])) v += text[i++];
            ut.push_back({Tok::Var, v});
            continue;
        }
        auto ar = [&](const char* s) { size_t n = strlen(s); return text.compare(i, n, s) == 0 ? n : 0; };
        if (size_t n = ar("−")) { ut.push_back({Tok::Op, "-"}); i += n; continue; }
        if (size_t n = ar("·")) { ut.push_back({Tok::Op, "*"}); i += n; continue; }
        if (size_t n = ar("×")) { ut.push_back({Tok::Op, "*"}); i += n; continue; }
        if (size_t n = ar("²")) { ut.push_back({Tok::Op, "^"}); ut.push_back({Tok::Tal, "2"}); i += n; continue; }
        if (size_t n = ar("³")) { ut.push_back({Tok::Op, "^"}); ut.push_back({Tok::Tal, "3"}); i += n; continue; }
        if (strchr("+-*/^()", c)) { ut.push_back({Tok::Op, std::string(1, (char)c)}); ++i; continue; }
        throw Fel("Förstår inte tecknet");
    }
    return ut;
}

Polynom konst(const Brak& c) { Polynom p; if (c.t() != 0) p[{}] = c; return p; }
Polynom var(const std::string& n) { Polynom p; p[{{n, 1}}] = Brak(1); return p; }

Polynom add(const Polynom& a, const Polynom& b, int tecken) {
    Polynom r = a;
    for (auto& [m, c] : b) {
        Brak ny = (r.count(m) ? r[m] : Brak(0)) + (tecken > 0 ? c : -c);
        if (ny.t() == 0) r.erase(m); else r[m] = ny;
    }
    return r;
}

Monom mulMonom(const Monom& a, const Monom& b) {
    std::map<std::string, int> d;
    for (auto& [v, e] : a) d[v] += e;
    for (auto& [v, e] : b) d[v] += e;
    Monom r;
    for (auto& [v, e] : d) if (e) r.push_back({v, e});
    return r;
}

Polynom mul(const Polynom& a, const Polynom& b) {
    Polynom r;
    for (auto& [m1, c1] : a)
        for (auto& [m2, c2] : b) {
            Monom m = mulMonom(m1, m2);
            r[m] = (r.count(m) ? r[m] : Brak(0)) + c1 * c2;
        }
    for (auto it = r.begin(); it != r.end();) it = it->second.t() == 0 ? r.erase(it) : std::next(it);
    return r;
}

struct Tolk {
    std::vector<Tok> t;
    size_t i = 0;
    const Tok& kika() const { static Tok slut; return i < t.size() ? t[i] : slut; }
    bool op(const char* v) const { return kika().s == Tok::Op && kika().v == v; }
    Tok ta() { Tok k = kika(); ++i; return k; }

    Polynom uttryck() {
        Polynom v = term();
        while (op("+") || op("-")) {
            int tecken = ta().v == "+" ? 1 : -1;
            v = add(v, term(), tecken);
        }
        return v;
    }
    Polynom term() {
        Polynom v = potens();
        while (true) {
            if (op("*")) { ta(); v = mul(v, potens()); }
            else if (op("/")) {
                ta();
                Polynom h = potens();
                if (!arKonstant(h)) throw Fel("Kan bara dela med tal");
                Brak k = h.count({}) ? h.at({}) : Brak(0);
                if (k.t() == 0) throw Fel("Division med noll");
                v = mul(v, konst(Brak(1) / k));
            } else if (kika().s == Tok::Var || op("(")) v = mul(v, potens());   // implicit: 2x, 3(x+1), xy
            else return v;
        }
    }
    Polynom potens() {
        Polynom v = primar();
        if (op("^")) {
            ta();
            Polynom e = primar();
            Brak k = e.count({}) ? e.at({}) : Brak(0);
            if (!arKonstant(e) || !k.heltal() || k.t() < 0 || k.t() > 20) throw Fel("Bara positiva heltalsexponenter");
            Polynom r = konst(Brak(1));
            for (int64_t j = 0; j < k.t(); ++j) r = mul(r, v);
            v = r;
        }
        return v;
    }
    Polynom primar() {
        if (op("-")) { ta(); return mul(konst(Brak(-1)), potens()); }
        if (op("+")) { ta(); return potens(); }
        if (op("(")) {
            ta();
            Polynom v = uttryck();
            if (!op(")")) throw Fel("Saknar )");
            ta();
            return v;
        }
        if (kika().s == Tok::Tal) return konst(Brak::decimal(ta().v));
        if (kika().s == Tok::Var) return var(ta().v);
        throw Fel("Väntade ett tal eller en variabel");
    }
};

}  // namespace

bool arKonstant(const Polynom& p) {
    for (auto& [m, c] : p) if (!m.empty()) return false;
    return true;
}

Polynom tolkaUttryck(const std::string& text) {
    if (trim(text).empty()) throw Fel("Inget svar");
    Tolk t{tokenisera(text)};
    Polynom v = t.uttryck();
    if (t.i != t.t.size()) throw Fel("Något blev över i uttrycket");
    return v;
}

std::string skriv(const Polynom& p) {
    if (p.empty()) return "0";
    auto grad = [](const Monom& m) { int g = 0; for (auto& [v, e] : m) g += e; return g; };
    std::vector<Monom> ordning;
    for (auto& [m, c] : p) ordning.push_back(m);
    // samma ordning som Python: högst grad först, sedan monomet (tupler jämförs element för element)
    std::sort(ordning.begin(), ordning.end(), [&](const Monom& a, const Monom& b) {
        if (grad(a) != grad(b)) return grad(a) > grad(b);
        return a < b;
    });
    std::string s;
    bool forsta = true;
    for (auto& m : ordning) {
        const Brak& c = p.at(m);
        std::string vars;
        for (auto& [v, e] : m) vars += v + (e == 1 ? "" : e == 2 ? "²" : e == 3 ? "³" : "^" + std::to_string(e));
        int64_t at = std::llabs(c.t());
        std::string k = c.heltal() ? std::to_string(at) : std::to_string(at) + "/" + std::to_string(c.n());
        if (!vars.empty() && k == "1") k = "";
        std::string del = k + vars;
        if (del.empty()) del = "0";
        bool minus = c.t() < 0;
        if (forsta) s = (minus ? "−" : "") + del;
        else s += std::string(minus ? " − " : " + ") + del;
        forsta = false;
    }
    return s;
}

double varde(const Polynom& p, const std::map<std::string, double>& v) {
    double tot = 0;
    for (auto& [m, c] : p) {
        double t = c.tal();
        for (auto& [n, e] : m) { auto f = v.find(n); t *= std::pow(f == v.end() ? 0.0 : f->second, e); }
        tot += t;
    }
    return tot;
}

}  // namespace matte
