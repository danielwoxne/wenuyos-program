// WenuyOS räknare – se WenuyCalc.h.
#include "WenuyCalc.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

namespace wenuy::calc {
namespace {
constexpr double PI = 3.14159265358979323846;

struct Parser {
    const std::string& s;
    size_t i = 0;
    bool deg;
    double ans;
    std::string err;
    Parser(const std::string& str, bool d, double a) : s(str), deg(d), ans(a) {}

    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i; }
    bool eat(const char* t) {
        ws();
        const size_t n = strlen(t);
        if (s.compare(i, n, t) == 0) { i += n; return true; }
        return false;
    }
    bool fail(const std::string& e) { if (err.empty()) err = e; return false; }

    // expr := term (('+'|'-') term)*   – "a + b %" = a + a·b/100
    double expr() {
        double a = term();
        for (;;) {
            if (eat("+")) { size_t save = i; double b = term(); if (pctAfter(save)) b = a * b / 100; a += b; }
            else if (eat("-") || eat("−")) { size_t save = i; double b = term(); if (pctAfter(save)) b = a * b / 100; a -= b; }
            else return a;
        }
    }
    bool lastPct = false;
    bool pctAfter(size_t) { bool p = lastPct; lastPct = false; return p; }
    double term() {
        double a = unary();
        for (;;) {
            ws();
            if (eat("*") || eat("×") || eat("·")) a *= unary();
            else if (eat("/") || eat("÷") || eat(":")) { double b = unary(); if (b == 0) fail("division by zero"); a /= b; }
            else if (eat("mod")) a = std::fmod(a, unary());
            else if (i < s.size() && (s[i] == '(' || isalpha((unsigned char)s[i]) || !strncmp(s.c_str() + i, "π", 2))) a *= power();   // 2π, 3(4+1)
            else return a;
        }
    }
    // −2^2 = −4 (potensen först), 2^−1 = 0,5
    double unary() {
        if (eat("-") || eat("−")) return -unary();
        if (eat("+")) return unary();
        return power();
    }
    double power() {
        double a = postfix();
        if (eat("^") || eat("**")) return std::pow(a, unary());
        return a;
    }
    double postfix() {
        double a = primary();
        for (;;) {
            ws();
            if (i < s.size() && s[i] == '!') {
                ++i;
                if (a < 0 || a > 170 || std::floor(a) != a) { fail("factorial"); return 0; }
                double f = 1;
                for (int k = 2; k <= (int)a; ++k) f *= k;
                a = f;
            } else if (i < s.size() && s[i] == '%') {
                ++i;
                lastPct = true;
                // ensam procent: 50 % = 0,5 (i + och − räknas den av föregående tal)
                ws();
                size_t j = i;
                bool before = false;
                for (size_t k = 0; k < j; ++k) if (s[k] == '+' || s[k] == '-') before = true;
                if (!before) { a /= 100; lastPct = false; }
            } else return a;
        }
    }
    double number() {
        ws();
        std::string t;
        while (i < s.size()) {
            const char c = s[i];
            if (isdigit((unsigned char)c)) t += c;
            else if (c == '.' || c == ',') t += '.';
            else if (c == ' ' && i + 3 < s.size() && isdigit((unsigned char)s[i + 1]) && isdigit((unsigned char)s[i + 2]) && isdigit((unsigned char)s[i + 3])
                     && (i + 4 >= s.size() || !isdigit((unsigned char)s[i + 4])) && !t.empty() && t.find('.') == std::string::npos) { /* 1 000 */ }
            else if ((c == 'e' || c == 'E') && !t.empty() && i + 1 < s.size() && (isdigit((unsigned char)s[i + 1]) || s[i + 1] == '-' || s[i + 1] == '+')) {
                t += 'e'; ++i; t += s[i];
            } else break;
            ++i;
        }
        if (t.empty() || std::count(t.begin(), t.end(), '.') > 1) { fail("number"); return 0; }
        // from_chars, inte strtod: strtod följer LC_NUMERIC, och i svenska GTK-program är decimaltecknet komma –
        // då blev "3,5" (som här redan är "3.5") bara 3
        double v = 0;
        std::from_chars(t.data(), t.data() + t.size(), v);
        return v;
    }
    double primary() {
        ws();
        if (eat("(")) { double v = expr(); if (!eat(")")) fail("missing )"); return v; }
        if (eat("π") || eat("pi")) return PI;
        if (i < s.size() && isalpha((unsigned char)s[i])) {
            std::string name;
            while (i < s.size() && (isalnum((unsigned char)s[i]))) name += (char)tolower((unsigned char)s[i++]);
            if (name == "e") return std::exp(1.0);
            if (name == "ans") return ans;
            static const std::map<std::string, double (*)(double)> fn = {
                {"sqrt", [](double x) { return std::sqrt(x); }}, {"cbrt", [](double x) { return std::cbrt(x); }},
                {"ln", [](double x) { return std::log(x); }}, {"log", [](double x) { return std::log10(x); }},
                {"lg", [](double x) { return std::log10(x); }}, {"log2", [](double x) { return std::log2(x); }},
                {"exp", [](double x) { return std::exp(x); }}, {"abs", [](double x) { return std::fabs(x); }},
                {"round", [](double x) { return std::round(x); }}, {"floor", [](double x) { return std::floor(x); }},
                {"ceil", [](double x) { return std::ceil(x); }}, {"sinh", [](double x) { return std::sinh(x); }},
                {"cosh", [](double x) { return std::cosh(x); }}, {"tanh", [](double x) { return std::tanh(x); }},
            };
            const bool trig = name == "sin" || name == "cos" || name == "tan" || name == "asin" || name == "acos" || name == "atan";
            if (!trig && !fn.count(name) && name != "rot") { fail("unknown: " + name); return 0; }
            double x;
            ws();
            if (eat("(")) { x = expr(); if (!eat(")")) fail("missing )"); }
            else x = unary();
            if (name == "rot") return std::sqrt(x);   // svenska: rot(9)
            if (trig) {
                const double k = deg ? PI / 180 : 1;
                if (name == "sin") return clean(std::sin(x * k));
                if (name == "cos") return clean(std::cos(x * k));
                if (name == "tan") return clean(std::tan(x * k));
                if (name == "asin") return std::asin(x) / k;
                if (name == "acos") return std::acos(x) / k;
                return std::atan(x) / k;
            }
            return fn.at(name)(x);
        }
        return number();
    }
    static double clean(double v) { return std::fabs(v) < 1e-12 ? 0 : v; }
};

// enheter: faktor till grundenheten i kategorin (temperatur räknas särskilt)
struct U { const char* id; const char* en; const char* sv; const char* cat; double f; std::vector<const char*> names; };
const std::vector<U>& table() {
    static const std::vector<U> t = {
        {"mm", "millimetre", "millimeter", "length", 0.001, {"mm", "millimeter", "millimetre", "millimeters"}},
        {"cm", "centimetre", "centimeter", "length", 0.01, {"cm", "centimeter", "centimetre", "centimeters"}},
        {"dm", "decimetre", "decimeter", "length", 0.1, {"dm", "decimeter"}},
        {"m", "metre", "meter", "length", 1, {"m", "meter", "metre", "meters"}},
        {"km", "kilometre", "kilometer", "length", 1000, {"km", "kilometer", "kilometre", "kilometers"}},
        {"mil", "Swedish mile (10 km)", "mil (10 km)", "length", 10000, {"mil", "svensk mil"}},
        {"in", "inch", "tum", "length", 0.0254, {"in", "inch", "inches", "tum", "\""}},
        {"ft", "foot", "fot", "length", 0.3048, {"ft", "foot", "feet", "fot"}},
        {"yd", "yard", "yard", "length", 0.9144, {"yd", "yard", "yards"}},
        {"mi", "mile", "engelsk mile", "length", 1609.344, {"mi", "mile", "miles", "engelsk mil"}},
        {"nmi", "nautical mile", "sjömil", "length", 1852, {"nmi", "sjömil", "nautical mile", "distansminut"}},
        {"mg", "milligram", "milligram", "mass", 0.001, {"mg", "milligram"}},
        {"g", "gram", "gram", "mass", 1, {"g", "gram", "grams"}},
        {"hg", "hectogram", "hekto", "mass", 100, {"hg", "hekto", "hektogram"}},
        {"kg", "kilogram", "kilo", "mass", 1000, {"kg", "kilo", "kilogram", "kilograms"}},
        {"t", "tonne", "ton", "mass", 1e6, {"t", "ton", "tonne", "tonnes"}},
        {"oz", "ounce", "uns", "mass", 28.349523125, {"oz", "ounce", "ounces", "uns"}},
        {"lb", "pound", "pund", "mass", 453.59237, {"lb", "lbs", "pound", "pounds", "pund"}},
        {"ml", "millilitre", "milliliter", "volume", 0.001, {"ml", "milliliter", "millilitre"}},
        {"cl", "centilitre", "centiliter", "volume", 0.01, {"cl", "centiliter"}},
        {"dl", "decilitre", "deciliter", "volume", 0.1, {"dl", "deciliter"}},
        {"l", "litre", "liter", "volume", 1, {"l", "liter", "litre", "liters"}},
        {"m3", "cubic metre", "kubikmeter", "volume", 1000, {"m3", "m³", "kubikmeter"}},
        {"krm", "pinch (1 ml)", "kryddmått", "volume", 0.001, {"krm", "kryddmått"}},
        {"tsk", "teaspoon (5 ml)", "tesked", "volume", 0.005, {"tsk", "tesked", "tsp", "teaspoon"}},
        {"msk", "tablespoon (15 ml)", "matsked", "volume", 0.015, {"msk", "matsked", "tbsp", "tablespoon"}},
        {"cup", "cup (US)", "kopp (US)", "volume", 0.2365882365, {"cup", "cups", "kopp", "koppar"}},
        {"floz", "fluid ounce (US)", "fluid ounce (US)", "volume", 0.0295735295625, {"floz", "fl oz", "fluid ounce"}},
        {"pt", "pint (UK)", "pint (UK)", "volume", 0.56826125, {"pt", "pint", "pints"}},
        {"gal", "gallon (US)", "gallon (US)", "volume", 3.785411784, {"gal", "gallon", "gallons"}},
        {"C", "Celsius", "Celsius", "temperature", 0, {"c", "°c", "celsius", "grader"}},
        {"F", "Fahrenheit", "Fahrenheit", "temperature", 0, {"f", "°f", "fahrenheit"}},
        {"K", "Kelvin", "Kelvin", "temperature", 0, {"k", "kelvin"}},
        {"s", "second", "sekund", "time", 1, {"s", "sek", "sekund", "sekunder", "second", "seconds"}},
        {"min", "minute", "minut", "time", 60, {"min", "minut", "minuter", "minute", "minutes"}},
        {"h", "hour", "timme", "time", 3600, {"h", "tim", "timme", "timmar", "hour", "hours"}},
        {"d", "day", "dygn", "time", 86400, {"d", "dag", "dagar", "dygn", "day", "days"}},
        {"wk", "week", "vecka", "time", 604800, {"v", "vecka", "veckor", "week", "weeks"}},
        {"yr", "year (365 d)", "år (365 d)", "time", 31536000, {"år", "year", "years"}},
        {"B", "byte", "byte", "data", 1, {"b", "byte", "bytes"}},
        {"kB", "kilobyte", "kilobyte", "data", 1e3, {"kb", "kilobyte"}},
        {"MB", "megabyte", "megabyte", "data", 1e6, {"mb", "megabyte"}},
        {"GB", "gigabyte", "gigabyte", "data", 1e9, {"gb", "gigabyte"}},
        {"TB", "terabyte", "terabyte", "data", 1e12, {"tb", "terabyte"}},
        {"KiB", "kibibyte", "kibibyte", "data", 1024.0, {"kib"}},
        {"MiB", "mebibyte", "mebibyte", "data", 1048576.0, {"mib"}},
        {"GiB", "gibibyte", "gibibyte", "data", 1073741824.0, {"gib"}},
        {"m/s", "metres per second", "meter per sekund", "speed", 1, {"m/s", "mps"}},
        {"km/h", "kilometres per hour", "kilometer i timmen", "speed", 1 / 3.6, {"km/h", "kmh", "kph", "km/t"}},
        {"mph", "miles per hour", "miles per timme", "speed", 0.44704, {"mph"}},
        {"kn", "knot", "knop", "speed", 0.514444444, {"kn", "knop", "knot", "knots"}},
        {"m2", "square metre", "kvadratmeter", "area", 1, {"m2", "m²", "kvm", "kvadratmeter"}},
        {"km2", "square kilometre", "kvadratkilometer", "area", 1e6, {"km2", "km²"}},
        {"ha", "hectare", "hektar", "area", 1e4, {"ha", "hektar", "hectare"}},
        {"ac", "acre", "acre", "area", 4046.8564224, {"ac", "acre", "acres"}},
        {"ft2", "square foot", "kvadratfot", "area", 0.09290304, {"ft2", "ft²", "sq ft"}},
        {"J", "joule", "joule", "energy", 1, {"j", "joule"}},
        {"kJ", "kilojoule", "kilojoule", "energy", 1000, {"kj", "kilojoule"}},
        {"cal", "calorie", "kalori", "energy", 4.184, {"cal", "kalori"}},
        {"kcal", "kilocalorie", "kilokalori", "energy", 4184, {"kcal", "kilokalori"}},
        {"Wh", "watt-hour", "wattimme", "energy", 3600, {"wh"}},
        {"kWh", "kilowatt-hour", "kilowattimme", "energy", 3.6e6, {"kwh"}},
    };
    return t;
}
const U* findUnit(std::string n) {
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return (char)tolower(c); });
    while (!n.empty() && (n.back() == '.' || n.back() == ' ')) n.pop_back();
    for (auto& u : table()) {
        std::string id = u.id;
        std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) { return (char)tolower(c); });
        if (id == n) return &u;
    }
    for (auto& u : table()) for (const char* a : u.names) if (n == a) return &u;
    return nullptr;
}
}  // namespace

bool evaluate(const std::string& expr, double& out, std::string* error, bool degrees, double ans) {
    std::string e = expr;
    for (const char* bad : {"=", "\n"}) { auto p = e.find(bad); if (p != std::string::npos) e = e.substr(0, p); }
    Parser p(e, degrees, ans);
    out = p.expr();
    p.ws();
    if (p.err.empty() && p.i < e.size()) p.err = "unexpected: " + e.substr(p.i);
    if (p.err.empty() && (std::isnan(out) || std::isinf(out))) p.err = "not a number";
    if (error) *error = p.err;
    return p.err.empty();
}

std::string format(double v, bool swedish, int digits) {
    if (std::fabs(v) < 1e-15) v = 0;
    char b[64];
    auto r = std::to_chars(b, b + sizeof b - 1, v, std::chars_format::general, digits);   // alltid punkt, oavsett språk
    *r.ptr = 0;
    std::string s = b;
    if (s.find('e') == std::string::npos && s.find('.') != std::string::npos) {   // 0.30000000000000004 → 0.3
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    // tusentalsavgränsare för stora heltalsdelar (svenska: mellanslag)
    const size_t dot = s.find('.');
    std::string ip = s.substr(0, dot == std::string::npos ? s.size() : dot), fp = dot == std::string::npos ? "" : s.substr(dot + 1);
    if (s.find('e') == std::string::npos) {
        const bool neg = !ip.empty() && ip[0] == '-';
        std::string digitsOnly = neg ? ip.substr(1) : ip, g;
        for (size_t k = 0; k < digitsOnly.size(); ++k) {
            if (k && (digitsOnly.size() - k) % 3 == 0 && digitsOnly.size() > 4) g += swedish ? " " : ",";
            g += digitsOnly[k];
        }
        s = (neg ? "-" : "") + g + (fp.empty() ? "" : (swedish ? "," : ".") + fp);
    } else if (swedish) std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

const std::vector<Unit>& units() {
    static std::vector<Unit> u;
    if (u.empty()) for (auto& x : table()) u.push_back({x.id, x.en, x.sv, x.cat});
    return u;
}
std::vector<std::string> categories() { return {"length", "mass", "volume", "temperature", "time", "speed", "area", "data", "energy"}; }
const char* categoryName(const std::string& c, bool sv) {
    static const std::map<std::string, std::pair<const char*, const char*>> n = {
        {"length", {"Length", "Längd"}}, {"mass", {"Weight", "Vikt"}}, {"volume", {"Volume", "Volym"}}, {"temperature", {"Temperature", "Temperatur"}},
        {"time", {"Time", "Tid"}}, {"speed", {"Speed", "Hastighet"}}, {"area", {"Area", "Area"}}, {"data", {"Data", "Data"}}, {"energy", {"Energy", "Energi"}},
    };
    auto it = n.find(c);
    return it == n.end() ? c.c_str() : (sv ? it->second.second : it->second.first);
}
bool convertValue(double v, const std::string& from, const std::string& to, double& out) {
    const U *a = nullptr, *b = nullptr;
    for (auto& u : table()) { if (from == u.id) a = &u; if (to == u.id) b = &u; }
    if (!a) a = findUnit(from);
    if (!b) b = findUnit(to);
    if (!a || !b || strcmp(a->cat, b->cat)) return false;
    if (!strcmp(a->cat, "temperature")) {
        double c = a->id[0] == 'C' ? v : a->id[0] == 'F' ? (v - 32) * 5 / 9 : v - 273.15;
        out = b->id[0] == 'C' ? c : b->id[0] == 'F' ? c * 9 / 5 + 32 : c + 273.15;
        return true;
    }
    out = v * a->f / b->f;
    return true;
}
bool convert(const std::string& q, std::string& out, bool sv) {
    // "<tal> <enhet> (i|till|in|to|=|->) <enhet>"
    std::string s = q;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : (char)c; });
    size_t sep = std::string::npos, seplen = 0;
    for (const char* w : {" i ", " till ", " in ", " to ", " = ", " -> ", " → ", " som "}) {
        auto p = s.rfind(w);
        if (p != std::string::npos && p > 0) { sep = p; seplen = strlen(w); break; }
    }
    if (sep == std::string::npos) return false;
    std::string left = s.substr(0, sep), right = s.substr(sep + seplen);
    // talet först, sedan enheten
    size_t k = 0;
    while (k < left.size() && (isdigit((unsigned char)left[k]) || left[k] == '.' || left[k] == ',' || left[k] == '-' || left[k] == ' ' || left[k] == '+' || left[k] == '*' || left[k] == '/' || left[k] == '(' || left[k] == ')')) ++k;
    std::string num = left.substr(0, k), unit = left.substr(k);
    while (!num.empty() && num.back() == ' ') num.pop_back();
    while (!unit.empty() && unit.front() == ' ') unit.erase(0, 1);
    if (num.empty() || unit.empty()) return false;
    double v;
    if (!evaluate(num, v)) return false;
    const U* a = findUnit(unit);
    const U* b = findUnit(right);
    if (!a || !b) return false;
    double r;
    if (!convertValue(v, a->id, b->id, r)) return false;
    auto sym = [](const U* u) {
        std::string i = u->id;
        if (!strcmp(u->cat, "temperature")) return std::string(i == "K" ? "K" : "°" + i);
        return i;
    };
    out = format(v, sv, 10) + " " + sym(a) + " = " + format(r, sv, 6) + " " + sym(b);
    return true;
}
}  // namespace wenuy::calc
