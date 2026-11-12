#include "Json.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace matte {

struct JsonLasare {
    const std::string& s;
    size_t i = 0;

    void blank() { while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i; }
    [[noreturn]] void fel(const char* vad) { throw std::runtime_error(std::string("JSON: ") + vad + " vid " + std::to_string(i)); }

    void utf8(std::string& ut, unsigned cp) {
        if (cp < 0x80) ut += (char)cp;
        else if (cp < 0x800) { ut += (char)(0xC0 | (cp >> 6)); ut += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { ut += (char)(0xE0 | (cp >> 12)); ut += (char)(0x80 | ((cp >> 6) & 0x3F)); ut += (char)(0x80 | (cp & 0x3F)); }
        else { ut += (char)(0xF0 | (cp >> 18)); ut += (char)(0x80 | ((cp >> 12) & 0x3F)); ut += (char)(0x80 | ((cp >> 6) & 0x3F)); ut += (char)(0x80 | (cp & 0x3F)); }
    }
    unsigned hex4() {
        if (i + 4 > s.size()) fel("kort \\u");
        unsigned v = std::stoul(s.substr(i, 4), nullptr, 16);
        i += 4;
        return v;
    }
    std::string text() {
        std::string ut;
        ++i;   // "
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c != '\\') { ut += c; continue; }
            if (i >= s.size()) fel("slut i text");
            char e = s[i++];
            switch (e) {
                case 'n': ut += '\n'; break;
                case 't': ut += '\t'; break;
                case 'r': ut += '\r'; break;
                case 'b': ut += '\b'; break;
                case 'f': ut += '\f'; break;
                case 'u': {
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                        i += 2;
                        unsigned lo = hex4();
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    utf8(ut, cp);
                    break;
                }
                default: ut += e;
            }
        }
        if (i >= s.size()) fel("text utan slut");
        ++i;
        return ut;
    }
    Json varde() {
        blank();
        if (i >= s.size()) fel("väntade ett värde");
        char c = s[i];
        if (c == '{') {
            Json o = Json::objekt();
            ++i; blank();
            if (s[i] == '}') { ++i; return o; }
            while (true) {
                blank();
                if (s[i] != '"') fel("väntade nyckel");
                std::string k = text();
                blank();
                if (s[i] != ':') fel("väntade :");
                ++i;
                o.o_.emplace_back(std::move(k), varde());
                blank();
                if (s[i] == ',') { ++i; continue; }
                if (s[i] == '}') { ++i; return o; }
                fel("väntade , eller }");
            }
        }
        if (c == '[') {
            Json l = Json::lista();
            ++i; blank();
            if (s[i] == ']') { ++i; return l; }
            while (true) {
                l.l_.push_back(varde());
                blank();
                if (s[i] == ',') { ++i; continue; }
                if (s[i] == ']') { ++i; return l; }
                fel("väntade , eller ]");
            }
        }
        if (c == '"') return Json(text());
        if (s.compare(i, 4, "true") == 0) { i += 4; return Json(true); }
        if (s.compare(i, 5, "false") == 0) { i += 5; return Json(false); }
        if (s.compare(i, 4, "null") == 0) { i += 4; return Json(); }
        size_t start = i;
        while (i < s.size() && (isdigit((unsigned char)s[i]) || strchr("+-.eE", s[i]))) ++i;
        if (start == i) fel("okänt tecken");
        return Json(std::strtod(s.substr(start, i - start).c_str(), nullptr));
    }
};

Json Json::las(const std::string& text) {
    JsonLasare l{text};
    Json v = l.varde();
    return v;
}

Json Json::lasFil(const std::string& sokvag) {
    std::ifstream f(sokvag, std::ios::binary);
    if (!f) return Json();
    std::stringstream ss;
    ss << f.rdbuf();
    return las(ss.str());
}

namespace {
void skrivText(std::string& ut, const std::string& s) {
    ut += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': ut += "\\\""; break;
            case '\\': ut += "\\\\"; break;
            case '\n': ut += "\\n"; break;
            case '\t': ut += "\\t"; break;
            case '\r': ut += "\\r"; break;
            default:
                if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); ut += b; }
                else ut += (char)c;
        }
    }
    ut += '"';
}
void skrivVarde(std::string& ut, const Json& j, bool snyggt, int djup) {
    std::string in = snyggt ? "\n" + std::string((djup + 1) * 1, ' ') : "";
    std::string ut_in = snyggt ? "\n" + std::string(djup * 1, ' ') : "";
    switch (j.typ()) {
        case Json::Null: ut += "null"; break;
        case Json::Bool: ut += j.sant() ? "true" : "false"; break;
        case Json::Tal: ut += j.str(); break;
        case Json::Text: skrivText(ut, j.str()); break;
        case Json::Lista: {
            ut += '[';
            bool f = true;
            for (auto& e : j.element()) { if (!f) ut += ','; f = false; ut += in; skrivVarde(ut, e, snyggt, djup + 1); }
            if (!j.element().empty()) ut += ut_in;
            ut += ']';
            break;
        }
        case Json::Objekt: {
            ut += '{';
            bool f = true;
            for (auto& [k, v] : j.falt()) {
                if (!f) ut += ',';
                f = false;
                ut += in;
                skrivText(ut, k);
                ut += snyggt ? ": " : ":";
                skrivVarde(ut, v, snyggt, djup + 1);
            }
            if (!j.falt().empty()) ut += ut_in;
            ut += '}';
            break;
        }
    }
}
}  // namespace

std::string Json::skriv(bool snyggt) const {
    std::string ut;
    skrivVarde(ut, *this, snyggt, 0);
    return ut;
}

std::string Json::str(const std::string& std_) const {
    if (typ_ == Text) return s_;
    if (typ_ == Tal) {
        if (std::floor(d_) == d_ && std::fabs(d_) < 1e15) return std::to_string((long long)d_);
        char b[32]; snprintf(b, sizeof b, "%.10g", d_);
        return b;
    }
    if (typ_ == Bool) return b_ ? "true" : "false";
    return std_;
}

double Json::tal(double std_) const {
    if (typ_ == Tal) return d_;
    if (typ_ == Text) { try { return std::stod(s_); } catch (...) { return std_; } }
    if (typ_ == Bool) return b_ ? 1 : 0;
    return std_;
}

const Json& Json::operator[](const std::string& nyckel) const {
    static const Json tom;
    if (typ_ != Objekt) return tom;
    for (auto& [k, v] : o_) if (k == nyckel) return v;
    return tom;
}

const Json& Json::operator[](size_t i) const {
    static const Json tom;
    return typ_ == Lista && i < l_.size() ? l_[i] : tom;
}

bool Json::har(const std::string& nyckel) const {
    if (typ_ != Objekt) return false;
    for (auto& [k, v] : o_) if (k == nyckel) return true;
    return false;
}

Json& Json::satt(const std::string& nyckel, Json v) {
    typ_ = Objekt;
    for (auto& [k, gammal] : o_) if (k == nyckel) { gammal = std::move(v); return gammal; }
    o_.emplace_back(nyckel, std::move(v));
    return o_.back().second;
}

std::vector<std::string> Json::strangar() const {
    std::vector<std::string> r;
    for (auto& e : l_) r.push_back(e.str());
    return r;
}

}  // namespace matte
