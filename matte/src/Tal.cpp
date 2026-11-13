#include "Tal.h"

#include <glib.h>

#include <cmath>
#include <algorithm>
#include <map>
#include <optional>
#include <vector>

namespace matte {

namespace {
__int128 gcd128(__int128 a, __int128 b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { __int128 r = a % b; a = b; b = r; }
    return a;
}
constexpr __int128 GRANS = (__int128)1 << 62;
}  // namespace

Brak Brak::ur(__int128 t, __int128 n) {
    if (n == 0) throw Fel("Division med noll");
    if (n < 0) { t = -t; n = -n; }
    __int128 g = gcd128(t, n);
    if (g > 1) { t /= g; n /= g; }
    if (t >= GRANS || t <= -GRANS || n >= GRANS) throw Fel("Talet blev för stort");
    return Brak(Ra{}, (int64_t)t, (int64_t)n);
}

Brak::Brak(int64_t t, int64_t n) : t_(0), n_(1) {
    *this = ur(t, n);
}

Brak Brak::operator+(const Brak& o) const { return ur((__int128)t_ * o.n_ + (__int128)o.t_ * n_, (__int128)n_ * o.n_); }
Brak Brak::operator-(const Brak& o) const { return ur((__int128)t_ * o.n_ - (__int128)o.t_ * n_, (__int128)n_ * o.n_); }
Brak Brak::operator*(const Brak& o) const { return ur((__int128)t_ * o.t_, (__int128)n_ * o.n_); }
Brak Brak::operator/(const Brak& o) const {
    if (o.t_ == 0) throw Fel("Division med noll");
    return ur((__int128)t_ * o.n_, (__int128)n_ * o.t_);
}

Brak Brak::ungefar(double x) {
    if (!std::isfinite(x)) throw Fel("Talet går inte att räkna ut");
    // Kedjebråk med nämnare ≤ 10⁹ – som Fraction(x).limit_denominator(10**9)
    const int64_t maxn = 1000000000;
    bool neg = x < 0; if (neg) x = -x;
    int64_t p0 = 0, q0 = 1, p1 = 1, q1 = 0;
    double r = x;
    for (int i = 0; i < 64; ++i) {
        double a = std::floor(r);
        if (a > 9e15) break;
        int64_t ai = (int64_t)a;
        int64_t q2 = q0 + ai * q1;
        if (q2 > maxn) break;
        int64_t p2 = p0 + ai * p1;
        p0 = p1; q0 = q1; p1 = p2; q1 = q2;
        double frac = r - a;
        if (frac < 1e-15) break;
        r = 1.0 / frac;
    }
    if (q1 == 0) { p1 = (int64_t)std::llround(x); q1 = 1; }
    return Brak(neg ? -p1 : p1, q1);
}

Brak Brak::decimal(const std::string& s) {
    std::string heltal = s, dec;
    auto p = s.find('.');
    if (p != std::string::npos) { heltal = s.substr(0, p); dec = s.substr(p + 1); }
    if (heltal.size() + dec.size() > 17) throw Fel("Talet har för många siffror");
    __int128 t = 0, n = 1;
    for (char c : heltal + dec) t = t * 10 + (c - '0');
    for (size_t i = 0; i < dec.size(); ++i) n *= 10;
    return ur(t, n);
}

std::string Brak::nyckel() const { return "t:" + std::to_string(t_) + "/" + std::to_string(n_); }

namespace {
bool exaktDecimal(int64_t n) {
    while (n % 2 == 0) n /= 2;
    while (n % 5 == 0) n /= 5;
    return n == 1;
}
std::string decimaltext(const Brak& v) {
    char buf[64];
    snprintf(buf, sizeof buf, exaktDecimal(v.n()) ? "%.10f" : "%.3f", v.tal());
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}
}  // namespace

std::string Brak::text(const std::string& form) const {
    if (form == "procent") {
        Brak p = *this * Brak(100);
        return (p.heltal() ? std::to_string(p.t()) : decimaltext(p)) + " %";
    }
    if (form == "brak" || form == "brak-enklast") return heltal() ? std::to_string(t_) : std::to_string(t_) + "/" + std::to_string(n_);
    if (form == "decimal") return decimaltext(*this);
    if (heltal()) return std::to_string(t_);
    return exaktDecimal(n_) ? decimaltext(*this) : std::to_string(t_) + "/" + std::to_string(n_);
}

// ---- tolkning -----------------------------------------------------------------------------------

namespace {

struct Token {
    enum Slag { Tal, Op, Proc, Namn, Slut } slag = Slut;
    std::string v;
};

const char* FUNKTIONER[] = {"sin", "cos", "tan", "lg", "log", "ln", "sqrt", "rot"};

bool arFunktion(const std::string& n) {
    for (auto* f : FUNKTIONER) if (n == f) return true;
    return false;
}

// Läser ett UTF-8-tecken vid i, returnerar det och flyttar i.
std::string tecken(const std::string& s, size_t& i) {
    unsigned char c = s[i];
    size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
    std::string t = s.substr(i, len);
    i += len;
    return t;
}

std::vector<Token> tokenisera(const std::string& text) {
    std::vector<Token> ut;
    size_t i = 0;
    while (i < text.size()) {
        char c = text[i];
        if (c == ' ' || c == '\t' || c == '\n') { ++i; continue; }
        if (c >= '0' && c <= '9') {
            std::string t;
            while (i < text.size() && isdigit((unsigned char)text[i])) t += text[i++];
            if (i + 1 < text.size() && (text[i] == ',' || text[i] == '.') && isdigit((unsigned char)text[i + 1])) {
                t += '.'; ++i;
                while (i < text.size() && isdigit((unsigned char)text[i])) t += text[i++];
            }
            ut.push_back({Token::Tal, t});
            continue;
        }
        if (isalpha((unsigned char)c)) {
            std::string n;
            while (i < text.size() && isalpha((unsigned char)text[i])) n += (char)tolower(text[i++]);
            if (n == "pi") { ut.push_back({Token::Namn, "pi"}); continue; }
            if (!arFunktion(n)) throw Fel("Okänt ord '" + n + "'");
            ut.push_back({Token::Namn, n});
            continue;
        }
        size_t start = i;
        std::string t = tecken(text, i);
        static const std::map<std::string, std::string> OP = {
            {"+", "+"}, {"-", "-"}, {"−", "-"}, {"–", "-"}, {"*", "*"}, {"·", "*"}, {"×", "*"}, {"⋅", "*"},
            {"/", "/"}, {":", "/"}, {"÷", "/"}, {"^", "^"}, {"(", "("}, {")", ")"}, {"√", "√"}};
        if (auto f = OP.find(t); f != OP.end()) { ut.push_back({Token::Op, f->second}); continue; }
        if (t == "%") { ut.push_back({Token::Proc, "%"}); continue; }
        if (t == "π") { ut.push_back({Token::Namn, "pi"}); continue; }
        if (t == "²") { ut.push_back({Token::Op, "^"}); ut.push_back({Token::Tal, "2"}); continue; }
        if (t == "³") { ut.push_back({Token::Op, "^"}); ut.push_back({Token::Tal, "3"}); continue; }
        throw Fel("Förstår inte tecknet '" + text.substr(start, i - start) + "'");
    }
    return ut;
}

std::optional<int64_t> heltalsrot(int64_t n, int64_t q) {
    if (n < 0) return std::nullopt;
    int64_t r = (int64_t)std::llround(std::pow((double)n, 1.0 / (double)q));
    for (int64_t k = std::max<int64_t>(0, r - 1); k <= r + 1; ++k) {
        __int128 p = 1;
        for (int64_t j = 0; j < q && p <= n; ++j) p *= k;
        if (p == n) return k;
    }
    return std::nullopt;
}

}  // namespace

Brak potens(const Brak& bas, const Brak& e) {
    if (e.heltal()) {
        int64_t k = e.t();
        if (bas.t() == 0 && k < 0) throw Fel("Division med noll");
        if (std::llabs(k) > 200) throw Fel("Exponenten är för stor");
        Brak r(1), b = k < 0 ? Brak(1) / bas : bas;
        for (int64_t i = 0; i < std::llabs(k); ++i) r = r * b;
        return r;
    }
    int64_t q = e.n();
    if (bas.t() < 0) {
        if (q % 2 == 1) return -potens(-bas, e);
        throw Fel("Jämn rot ur ett negativt tal");
    }
    auto t = heltalsrot(bas.t(), q), n = heltalsrot(bas.n(), q);
    if (t && n) return potens(Brak(*t, *n), Brak(e.t()));
    return Brak::ungefar(std::pow(bas.tal(), e.tal()));
}

namespace {

Brak funktion(const std::string& namn, const Brak& v) {
    if (namn == "sqrt" || namn == "rot") return potens(v, Brak(1, 2));
    if (namn == "sin" || namn == "cos" || namn == "tan") {
        // exakta värden i grader (som Python-versionen)
        if (v.heltal()) {
            int64_t g = ((v.t() % 360) + 360) % 360;
            static const std::map<std::pair<std::string, int64_t>, Brak> EXAKT = {
                {{"sin", 0}, Brak(0)}, {{"sin", 30}, Brak(1, 2)}, {{"sin", 90}, Brak(1)}, {{"sin", 150}, Brak(1, 2)},
                {{"sin", 180}, Brak(0)}, {{"sin", 270}, Brak(-1)}, {{"cos", 0}, Brak(1)}, {{"cos", 60}, Brak(1, 2)},
                {{"cos", 90}, Brak(0)}, {{"cos", 180}, Brak(-1)}, {{"cos", 270}, Brak(0)}, {{"cos", 300}, Brak(1, 2)},
                {{"tan", 0}, Brak(0)}, {{"tan", 45}, Brak(1)}, {{"tan", 180}, Brak(0)}, {{"tan", 225}, Brak(1)}};
            if (auto f = EXAKT.find({namn, g}); f != EXAKT.end()) return f->second;
        }
        double r = v.tal() * M_PI / 180.0;
        return Brak::ungefar(namn == "sin" ? std::sin(r) : namn == "cos" ? std::cos(r) : std::tan(r));
    }
    if (v.t() <= 0) throw Fel("Logaritmen finns bara för positiva tal");
    if (namn == "lg" || namn == "log") {
        double l = std::log10(v.tal());
        int64_t h = std::llround(l);
        if (h >= -15 && h <= 15 && potens(Brak(10), Brak(h)) == v) return Brak(h);
        return Brak::ungefar(l);
    }
    return Brak::ungefar(std::log(v.tal()));
}

// Rekursiv nedstigning: uttryck → term → potens → primär
struct Tolk {
    std::vector<Token> t;
    size_t i = 0;
    const Token& kika() const { static Token slut; return i < t.size() ? t[i] : slut; }
    bool ar(Token::Slag s, const char* v) const { return kika().slag == s && kika().v == v; }
    Token ta() { Token k = kika(); ++i; return k; }

    Brak uttryck() {
        Brak v = term();
        while (ar(Token::Op, "+") || ar(Token::Op, "-")) {
            bool plus = ta().v == "+";
            Brak h = term();
            v = plus ? v + h : v - h;
        }
        return v;
    }
    Brak term() {
        Brak v = pot();
        while (ar(Token::Op, "*") || ar(Token::Op, "/")) {
            bool gange = ta().v == "*";
            Brak h = pot();
            v = gange ? v * h : v / h;
        }
        return v;
    }
    Brak pot() {
        Brak v = primar();
        if (ar(Token::Op, "^")) { ta(); v = potens(v, potHoger()); }
        return v;
    }
    Brak potHoger() {   // exponenten: tecken direkt (2^-1), högerassociativ
        if (ar(Token::Op, "-")) { ta(); return -potHoger(); }
        return pot();
    }
    Brak efter(Brak v) {
        if (kika().slag == Token::Proc) { ta(); return v / Brak(100); }
        return v;
    }
    Brak primar() {
        const Token k = kika();
        if (k.slag == Token::Op && k.v == "√") { ta(); return potens(primar(), Brak(1, 2)); }
        if (k.slag == Token::Namn) {
            ta();
            if (k.v == "pi") return efter(Brak::ungefar(M_PI));
            return efter(funktion(k.v, primar()));
        }
        if (k.slag == Token::Op && k.v == "-") { ta(); return -primar(); }
        if (k.slag == Token::Op && k.v == "+") { ta(); return primar(); }
        if (k.slag == Token::Op && k.v == "(") {
            ta();
            Brak inre = uttryck();
            if (!(kika().slag == Token::Op && kika().v == ")")) throw Fel("Saknar )");
            ta();
            return efter(inre);
        }
        if (k.slag == Token::Tal) {
            ta();
            Brak tal = Brak::decimal(k.v);
            // blandad form "1 1/2"
            if (kika().slag == Token::Tal && i + 2 < t.size() + 1 && i + 1 < t.size() && t[i + 1].slag == Token::Op &&
                t[i + 1].v == "/" && i + 2 < t.size() && t[i + 2].slag == Token::Tal && k.v.find('.') == std::string::npos) {
                Brak talj = Brak::decimal(ta().v); ta(); Brak nam = Brak::decimal(ta().v);
                if (nam.t() == 0) throw Fel("Nämnaren kan inte vara noll");
                tal = tal + talj / nam;
            }
            return efter(tal);
        }
        throw Fel("Väntade ett tal");
    }
};

}  // namespace

Brak tolka(const std::string& text) {
    if (trim(text).empty()) throw Fel("Inget svar");
    Tolk tolk{tokenisera(text)};
    Brak v = tolk.uttryck();
    if (tolk.i != tolk.t.size()) throw Fel("Något blev över i uttrycket");
    return v;
}

bool okantOrd(const std::string& text) {
    try { tokenisera(text); return false; } catch (const Fel& e) { return std::string(e.what()).rfind("Okänt ord", 0) == 0; }
}

}  // namespace matte
