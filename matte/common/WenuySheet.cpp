#include "WenuySheet.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iomanip>
#include <locale>
#include <sstream>

namespace wenuy {

// Tal läses och skrivs alltid med punkt internt, oberoende av systemets språk (strtod/printf
// följer annars locale – på svenska skulle "3.5" inte gå att läsa).
static bool readDouble(const char* s, double* out, size_t* used) {
    std::istringstream is(s);
    is.imbue(std::locale::classic());
    double d;
    if (!(is >> d)) return false;
    *out = d;
    *used = is.eof() ? std::strlen(s) : (size_t)is.tellg();
    return true;
}

static std::string fmtDouble(double d, int decimals = -1) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    if (decimals >= 0) os << std::fixed << std::setprecision(decimals) << d;
    else os << std::setprecision(10) << d;
    return os.str();
}

// ================================================================ namn
std::string colName(int col) {
    std::string s;
    for (++col; col > 0; col = (col - 1) / 26) s.insert(s.begin(), char('A' + (col - 1) % 26));
    return s;
}

std::string cellName(CellRef c) { return colName(c.col) + std::to_string(c.row + 1); }

// Läs [$]KOL[$]RAD från p; returnerar antal tecken (0 = ingen referens)
static size_t readRef(const char* p, CellRef* out, bool* absCol = nullptr, bool* absRow = nullptr) {
    const char* s = p;
    bool ac = false, ar = false;
    if (*p == '$') { ac = true; ++p; }
    int col = 0, letters = 0;
    while (std::isalpha((unsigned char)*p) && letters < 3) { col = col * 26 + (std::toupper((unsigned char)*p) - 'A' + 1); ++p; ++letters; }
    if (!letters || std::isalpha((unsigned char)*p)) return 0;
    if (*p == '$') { ar = true; ++p; }
    if (!std::isdigit((unsigned char)*p)) return 0;
    long row = 0;
    while (std::isdigit((unsigned char)*p) && row < 10000000) { row = row * 10 + (*p - '0'); ++p; }
    if (row < 1 || std::isalnum((unsigned char)*p) || *p == '(') return 0;
    if (out) *out = {col - 1, (int)row - 1};
    if (absCol) *absCol = ac;
    if (absRow) *absRow = ar;
    return (size_t)(p - s);
}

bool parseCellName(const std::string& s, CellRef* out) {
    return !s.empty() && readRef(s.c_str(), out) == s.size();
}

static bool swedishLocale() {
    for (const char* v : {"LC_ALL", "LC_NUMERIC", "LANG"}) {
        const char* l = std::getenv(v);
        if (l && *l) return std::strncmp(l, "sv", 2) == 0;
    }
    return false;
}

std::string Value::display(int decimals) const {
    switch (kind) {
    case Empty: return "";
    case Text: case Error: return text;
    case Bool: return num ? "TRUE" : "FALSE";
    case Number: {
        std::string s = fmtDouble(std::fabs(num) < 1e-12 ? 0.0 : num, decimals);
        if (swedishLocale()) std::replace(s.begin(), s.end(), '.', ',');
        return s;
    }
    }
    return "";
}

// Tal som användaren skriver: "12", "-3.5", "3,5" (svenskt), "1 000", "25%"
static bool parseNumber(const std::string& in, double* out) {
    std::string s;
    for (char c : in) if (c != ' ' && c != '\xa0') s += (c == ',' ? '.' : c);
    if (s.empty()) return false;
    bool pct = false;
    if (s.back() == '%') { pct = true; s.pop_back(); }
    double d;
    size_t used = 0;
    if (!std::isdigit((unsigned char)s[0]) && s[0] != '-' && s[0] != '+' && s[0] != '.') return false;
    if (!readDouble(s.c_str(), &d, &used) || used != s.size()) return false;
    *out = pct ? d / 100 : d;
    return true;
}

// ================================================================ utvärdering
struct Arg {
    Value v;
    std::vector<Value> range;
    bool isRange = false;
};

struct Evaluator {
    const Sheet& sh;
    const char* p;
    Value err;   // första fel

    void ws() { while (*p == ' ') ++p; }
    bool eat(const char* tok) {
        ws();
        const size_t n = std::strlen(tok);
        if (std::strncmp(p, tok, n) == 0) { p += n; return true; }
        return false;
    }

    static bool truthy(const Value& v) {
        if (v.kind == Value::Number || v.kind == Value::Bool) return v.num != 0;
        if (v.kind == Value::Text) return !v.text.empty();
        return false;
    }
    static bool toNum(const Value& v, double* d) {
        if (v.kind == Value::Number || v.kind == Value::Bool) { *d = v.num; return true; }
        if (v.kind == Value::Empty) { *d = 0; return true; }
        if (v.kind == Value::Text) return parseNumber(v.text, d);
        return false;
    }
    static std::string toText(const Value& v) {
        if (v.kind == Value::Number) return fmtDouble(v.num);
        return v.display();
    }

    Value scalar(Arg a) {
        if (!a.isRange) return a.v;
        return a.range.size() == 1 ? a.range[0] : Value::error("#VALUE!");
    }

    Value expr() { return scalar(comparison()); }

    Arg comparison() {
        Arg l = concat();
        for (;;) {
            ws();
            const char* ops[] = {"<>", "<=", ">=", "=", "<", ">"};
            const char* op = nullptr;
            for (const char* o : ops) if (std::strncmp(p, o, std::strlen(o)) == 0) { op = o; break; }
            if (!op) return l;
            p += std::strlen(op);
            Value a = scalar(l), b = scalar(concat());
            if (a.kind == Value::Error) return {a};
            if (b.kind == Value::Error) return {b};
            int cmp;
            double x, y;
            if (toNum(a, &x) && toNum(b, &y) && a.kind != Value::Text && b.kind != Value::Text) cmp = x < y ? -1 : x > y ? 1 : 0;
            else {
                std::string s = toText(a), t = toText(b);
                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                std::transform(t.begin(), t.end(), t.begin(), ::tolower);
                cmp = s.compare(t);
                cmp = cmp < 0 ? -1 : cmp > 0 ? 1 : 0;
            }
            bool r = !std::strcmp(op, "=") ? cmp == 0 : !std::strcmp(op, "<>") ? cmp != 0 : !std::strcmp(op, "<") ? cmp < 0
                   : !std::strcmp(op, ">") ? cmp > 0 : !std::strcmp(op, "<=") ? cmp <= 0 : cmp >= 0;
            l = {Value::boolean(r)};
        }
    }

    Arg concat() {
        Arg l = additive();
        while (eat("&")) {
            Value a = scalar(l), b = scalar(additive());
            if (a.kind == Value::Error) return {a};
            if (b.kind == Value::Error) return {b};
            l = {Value::str(toText(a) + toText(b))};
        }
        return l;
    }

    Value arith(Value a, Value b, char op) {
        if (a.kind == Value::Error) return a;
        if (b.kind == Value::Error) return b;
        double x, y;
        if (!toNum(a, &x) || !toNum(b, &y)) return Value::error("#VALUE!");
        switch (op) {
        case '+': return Value::number(x + y);
        case '-': return Value::number(x - y);
        case '*': return Value::number(x * y);
        case '/': return y == 0 ? Value::error("#DIV/0!") : Value::number(x / y);
        case '^': return Value::number(std::pow(x, y));
        }
        return Value::error("#VALUE!");
    }

    Arg additive() {
        Arg l = term();
        for (;;) {
            ws();
            if (*p == '+' || *p == '-') { char op = *p++; l = {arith(scalar(l), scalar(term()), op)}; }
            else return l;
        }
    }
    Arg term() {
        Arg l = power();
        for (;;) {
            ws();
            if (*p == '*' || *p == '/') { char op = *p++; l = {arith(scalar(l), scalar(power()), op)}; }
            else return l;
        }
    }
    Arg power() {
        Arg l = unary();
        while (eat("^")) l = {arith(scalar(l), scalar(unary()), '^')};
        return l;
    }
    Arg unary() {
        ws();
        if (*p == '-') { ++p; return {arith(Value::number(0), scalar(unary()), '-')}; }
        if (*p == '+') { ++p; return unary(); }
        Arg a = primary();
        while (eat("%")) a = {arith(scalar(a), Value::number(100), '/')};
        return a;
    }

    Arg primary() {
        ws();
        if (*p == '(') {
            ++p;
            Value v = expr();
            if (!eat(")")) return {Value::error("#VALUE!")};
            return {v};
        }
        if (std::strncmp(p, "#REF!", 5) == 0) { p += 5; return {Value::error("#REF!")}; }
        if (*p == '"') {
            std::string s;
            for (++p; *p; ++p) {
                if (*p == '"') { if (p[1] == '"') { s += '"'; ++p; continue; } ++p; break; }
                s += *p;
            }
            return {Value::str(s)};
        }
        if (std::isdigit((unsigned char)*p) || *p == '.') {
            double d;
            size_t used = 0;
            if (!readDouble(p, &d, &used)) return {Value::error("#VALUE!")};
            p += used;
            return {Value::number(d)};
        }
        CellRef a;
        if (size_t n = readRef(p, &a)) {
            p += n;
            if (*p == ':') {
                CellRef b;
                if (size_t m = readRef(p + 1, &b)) {
                    p += 1 + m;
                    Arg r;
                    r.isRange = true;
                    const int c0 = std::min(a.col, b.col), c1 = std::max(a.col, b.col);
                    const int r0 = std::min(a.row, b.row), r1 = std::max(a.row, b.row);
                    if ((long)(c1 - c0 + 1) * (r1 - r0 + 1) > 1000000) return {Value::error("#REF!")};
                    for (int row = r0; row <= r1; ++row)
                        for (int col = c0; col <= c1; ++col) r.range.push_back(sh.value({col, row}));
                    return r;
                }
                return {Value::error("#REF!")};
            }
            return {sh.value(a)};
        }
        if (std::isalpha((unsigned char)*p)) {
            std::string name;
            while (std::isalnum((unsigned char)*p) || *p == '_' || *p == '.' || (unsigned char)*p >= 0x80) name += (char)std::toupper((unsigned char)*p++);
            if (name == "TRUE" || name == "SANT") return {Value::boolean(true)};
            if (name == "FALSE" || name == "FALSKT") return {Value::boolean(false)};
            if (!eat("(")) return {Value::error("#NAME?")};
            return call(name);
        }
        return {Value::error("#VALUE!")};
    }

    // IF utvärderar bara den gren som väljs
    Arg call(std::string fn) {
        static const std::map<std::string, std::string> sv = {
            {"SUMMA", "SUM"}, {"MEDEL", "AVERAGE"}, {"ANTAL", "COUNT"}, {"ANTALV", "COUNTA"}, {"OM", "IF"},
            {"OCH", "AND"}, {"ELLER", "OR"}, {"ICKE", "NOT"}, {"AVRUNDA", "ROUND"}, {"ROT", "SQRT"},
            {"UPPHÖJT.TILL", "POWER"}, {"LÄNGD", "LEN"}, {"VERSALER", "UPPER"}, {"GEMENER", "LOWER"},
            {"SAMMAN", "CONCAT"}, {"IDAG", "TODAY"}, {"NU", "NOW"}, {"ABSOLUT", "ABS"}};
        if (auto it = sv.find(fn); it != sv.end()) fn = it->second;
        std::vector<Arg> args;
        auto sep = [&]() { return eat(",") || eat(";"); };
        if (fn == "IF") {
            Value c = expr();
            if (!sep()) return {Value::error("#VALUE!")};
            Value r;
            if (c.kind == Value::Error) r = c;
            if (truthy(c)) { r = expr(); if (sep()) skipArg(); }
            else { skipArg(); r = sep() ? expr() : Value::boolean(false); }
            if (!eat(")")) return {Value::error("#VALUE!")};
            return {c.kind == Value::Error ? c : r};
        }
        ws();
        if (!eat(")")) {
            do args.push_back(comparison()); while (sep());
            if (!eat(")")) return {Value::error("#VALUE!")};
        }
        // platta ut alla värden (områden + enskilda)
        std::vector<Value> flat;
        for (auto& a : args) {
            if (a.isRange) flat.insert(flat.end(), a.range.begin(), a.range.end());
            else flat.push_back(a.v);
        }
        for (auto& v : flat) if (v.kind == Value::Error) return {v};
        auto nums = [&](bool countText) {
            std::vector<double> out;
            for (size_t i = 0; i < flat.size(); ++i) {
                const Value& v = flat[i];
                double d;
                if (v.kind == Value::Number) out.push_back(v.num);
                else if (countText && v.kind == Value::Text && toNum(v, &d)) out.push_back(d);
            }
            return out;
        };
        auto argNum = [&](size_t i, double* d) { return i < args.size() && toNum(scalar(args[i]), d); };
        if (fn == "SUM") { double s = 0; for (double d : nums(false)) s += d; return {Value::number(s)}; }
        if (fn == "AVERAGE") {
            auto n = nums(false);
            if (n.empty()) return {Value::error("#DIV/0!")};
            double s = 0; for (double d : n) s += d;
            return {Value::number(s / n.size())};
        }
        if (fn == "MIN" || fn == "MAX") {
            auto n = nums(false);
            if (n.empty()) return {Value::number(0)};
            return {Value::number(fn == "MIN" ? *std::min_element(n.begin(), n.end()) : *std::max_element(n.begin(), n.end()))};
        }
        if (fn == "COUNT") return {Value::number((double)nums(false).size())};
        if (fn == "COUNTA") { int c = 0; for (auto& v : flat) if (v.kind != Value::Empty) ++c; return {Value::number(c)}; }
        if (fn == "AND" || fn == "OR") {
            bool r = fn == "AND";
            for (auto& v : flat) { if (v.kind == Value::Empty) continue; if (fn == "AND") r = r && truthy(v); else r = r || truthy(v); }
            return {Value::boolean(r)};
        }
        if (fn == "NOT") return {Value::boolean(flat.empty() || !truthy(flat[0]))};
        if (fn == "ROUND") {
            double x, d = 0;
            if (!argNum(0, &x)) return {Value::error("#VALUE!")};
            argNum(1, &d);
            const double f = std::pow(10.0, std::floor(d));
            return {Value::number(std::round(x * f) / f)};
        }
        if (fn == "ABS" || fn == "SQRT") {
            double x;
            if (!argNum(0, &x)) return {Value::error("#VALUE!")};
            if (fn == "SQRT" && x < 0) return {Value::error("#NUM!")};
            return {Value::number(fn == "ABS" ? std::fabs(x) : std::sqrt(x))};
        }
        if (fn == "POWER") { double x, y; if (!argNum(0, &x) || !argNum(1, &y)) return {Value::error("#VALUE!")}; return {Value::number(std::pow(x, y))}; }
        if (fn == "LEN") return {Value::number(flat.empty() ? 0 : (double)g_utf8_len(toText(flat[0])))};
        if (fn == "UPPER" || fn == "LOWER") {
            std::string s = flat.empty() ? "" : toText(flat[0]);
            std::transform(s.begin(), s.end(), s.begin(), fn == "UPPER" ? ::toupper : ::tolower);
            return {Value::str(s)};
        }
        if (fn == "CONCAT") { std::string s; for (auto& v : flat) s += toText(v); return {Value::str(s)}; }
        if (fn == "TODAY" || fn == "NOW") {   // dagar sedan 1899-12-30 (som andra kalkylprogram)
            const double t = (double)std::time(nullptr) / 86400.0 + 25569.0;
            return {Value::number(fn == "TODAY" ? std::floor(t) : t)};
        }
        return {Value::error("#NAME?")};
    }

    static size_t g_utf8_len(const std::string& s) {
        size_t n = 0;
        for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
        return n;
    }

    // hoppa över ett argument utan att räkna (för IF)
    void skipArg() {
        int depth = 0;
        bool str = false;
        for (; *p; ++p) {
            if (str) { if (*p == '"') str = false; continue; }
            if (*p == '"') str = true;
            else if (*p == '(') ++depth;
            else if (*p == ')') { if (depth == 0) return; --depth; }
            else if ((*p == ',' || *p == ';') && depth == 0) return;
        }
    }
};

// ================================================================ blad
void Sheet::set(CellRef c, const std::string& in) {
    if (in.empty()) cells_.erase(c);
    else cells_[c] = in;
    cache_.clear();   // enkelt och korrekt: allt räknas om vid behov
}

std::string Sheet::input(CellRef c) const {
    auto it = cells_.find(c);
    return it == cells_.end() ? std::string() : it->second;
}

bool Sheet::isFormula(CellRef c) const {
    auto it = cells_.find(c);
    return it != cells_.end() && !it->second.empty() && it->second[0] == '=';
}

Value Sheet::value(CellRef c) const {
    if (auto it = cache_.find(c); it != cache_.end()) return it->second;
    auto it = cells_.find(c);
    if (it == cells_.end()) return {};
    const std::string& in = it->second;
    Value v;
    if (in[0] == '=') {
        if (std::find(evalStack_.begin(), evalStack_.end(), c) != evalStack_.end()) return Value::error("#CIRC!");
        if (evalStack_.size() > 2000) return Value::error("#CIRC!");
        evalStack_.push_back(c);
        Evaluator ev{*this, in.c_str() + 1, {}};
        v = ev.expr();
        ev.ws();
        if (*ev.p && v.kind != Value::Error) v = Value::error("#VALUE!");
        evalStack_.pop_back();
    } else if (in[0] == '\'') {
        v = Value::str(in.substr(1));   // 'text = tvinga text
    } else {
        double d;
        v = parseNumber(in, &d) ? Value::number(d) : Value::str(in);
    }
    cache_[c] = v;
    return v;
}

int Sheet::usedRows() const { int m = 0; for (auto& [c, _] : cells_) m = std::max(m, c.row + 1); return m; }
int Sheet::usedCols() const { int m = 0; for (auto& [c, _] : cells_) m = std::max(m, c.col + 1); return m; }

// ---------------------------------------------------------------- CSV
std::string Sheet::toCsv(char sep) const {
    std::string out;
    const int R = usedRows(), C = usedCols();
    for (int r = 0; r < R; ++r) {
        for (int c = 0; c < C; ++c) {
            if (c) out += sep;
            Value v = value({c, r});
            std::string s = v.kind == Value::Number ? Evaluator::toText(v) : v.display();
            if (sep == ';' && v.kind == Value::Number) std::replace(s.begin(), s.end(), '.', ',');
            if (s.find_first_of(std::string("\"\n") + sep) != std::string::npos) {
                std::string q = "\"";
                for (char ch : s) { if (ch == '"') q += '"'; q += ch; }
                s = q + "\"";
            }
            out += s;
        }
        out += '\n';
    }
    return out;
}

bool Sheet::fromCsv(const std::string& csv, char sep) {
    if (!sep) {   // gissa avgränsare från första raden
        const std::string first = csv.substr(0, csv.find('\n'));
        const auto n = [&](char ch) { return std::count(first.begin(), first.end(), ch); };
        sep = n('\t') > 0 ? '\t' : n(';') > n(',') ? ';' : ',';
    }
    clear();
    int r = 0, c = 0;
    std::string f;
    bool q = false, any = false;
    auto put = [&]() { if (!f.empty()) cells_[{c, r}] = f; f.clear(); any = true; };
    for (size_t i = 0; i < csv.size(); ++i) {
        const char ch = csv[i];
        if (q) {
            if (ch == '"') { if (i + 1 < csv.size() && csv[i + 1] == '"') { f += '"'; ++i; } else q = false; }
            else f += ch;
        } else if (ch == '"') q = true;
        else if (ch == sep) { put(); ++c; }
        else if (ch == '\n') { put(); ++r; c = 0; }
        else if (ch != '\r') f += ch;
    }
    if (!f.empty()) put();
    return any;
}

// ---------------------------------------------------------------- eget format
static std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '\\') o += "\\\\"; else if (c == '\n') o += "\\n"; else if (c == '\t') o += "\\t"; else o += c; }
    return o;
}

static std::string unesc(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) { char n = s[++i]; o += n == 'n' ? '\n' : n == 't' ? '\t' : n; }
        else o += s[i];
    }
    return o;
}

std::string Sheet::serialize() const {
    std::string out = "wsheet 1\n";
    for (auto& [c, in] : cells_) out += cellName(c) + "\t" + esc(in) + "\n";
    return out;
}

bool Sheet::deserialize(const std::string& data) {
    std::istringstream is(data);
    std::string line;
    if (!std::getline(is, line) || line.rfind("wsheet", 0) != 0) return false;
    clear();
    while (std::getline(is, line)) {
        const auto tab = line.find('\t');
        CellRef c;
        if (tab == std::string::npos || !parseCellName(line.substr(0, tab), &c)) continue;
        cells_[c] = unesc(line.substr(tab + 1));
    }
    return true;
}

// ---------------------------------------------------------------- rader/kolumner
void Sheet::shift(bool rows, int at, int n) {   // n > 0 infoga, n < 0 ta bort
    auto moveRef = [&](CellRef r, bool* gone) {
        int& v = rows ? r.row : r.col;
        if (n < 0 && v >= at && v < at - n) *gone = true;
        else if (v >= at) v += n;
        return r;
    };
    std::map<CellRef, std::string> next;
    for (auto& [c, in] : cells_) {
        bool gone = false;
        CellRef nc = moveRef(c, &gone);
        if (gone) continue;
        std::string s = in;
        if (!s.empty() && s[0] == '=') {   // skriv om referenser i formeln
            std::string o = "=";
            bool str = false;
            for (const char* p = s.c_str() + 1; *p;) {
                if (*p == '"') { str = !str; o += *p++; continue; }
                CellRef r;
                bool ac, ar;
                size_t k = (!str && (p == s.c_str() + 1 || !std::isalnum((unsigned char)p[-1]))) ? readRef(p, &r, &ac, &ar) : 0;
                if (!k) { o += *p++; continue; }
                auto name = [](CellRef m, bool ac, bool ar) {
                    return std::string(ac ? "$" : "") + colName(m.col) + (ar ? "$" : "") + std::to_string(m.row + 1);
                };
                CellRef r2;
                bool ac2, ar2;
                size_t k2 = p[k] == ':' ? readRef(p + k + 1, &r2, &ac2, &ar2) : 0;
                if (k2) {   // område: krymper/växer med raderna i stället för att gå sönder
                    int& lo = rows ? r.row : r.col;
                    int& hi = rows ? r2.row : r2.col;
                    if (lo > hi) std::swap(lo, hi);
                    if (n > 0) { if (lo >= at) lo += n; if (hi >= at) hi += n; }
                    else {
                        const int end = at - n;   // borttaget: [at, end)
                        auto mv = [&](int v, bool isHi) { return v >= end ? v + n : v >= at ? (isHi ? at - 1 : at) : v; };
                        const int nlo = mv(lo, false), nhi = mv(hi, true);
                        if (nhi < nlo) { o += "#REF!"; p += k + 1 + k2; continue; }
                        lo = nlo; hi = nhi;
                    }
                    o += name(r, ac, ar) + ":" + name(r2, ac2, ar2);
                    p += k + 1 + k2;
                    continue;
                }
                bool g = false;
                CellRef m = moveRef(r, &g);
                if (g) o += "#REF!";
                else o += name(m, ac, ar);
                p += k;
            }
            s = o;
        }
        next[nc] = s;
    }
    cells_.swap(next);
    cache_.clear();
}

void Sheet::insertRows(int at, int n) { shift(true, at, n); }
void Sheet::insertCols(int at, int n) { shift(false, at, n); }
void Sheet::removeRows(int at, int n) { shift(true, at, -n); }
void Sheet::removeCols(int at, int n) { shift(false, at, -n); }

}  // namespace wenuy
