#include "PapperLagring.h"

#include "Framsteg.h"
#include "Json.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>

namespace matte {
namespace {

constexpr const char* FORMAT = "se.wenuyos.matte.raknepapper";
bool felmed(std::string* fel, const std::string& text) {
    if (fel) *fel = text;
    return false;
}
bool rutkoordinat(int n) { return n >= 0 && n <= 300; }
bool tryggText(const std::string& s, size_t maxByte) {
    return s.size() <= maxByte && g_utf8_validate(s.c_str(), (gssize)s.size(), nullptr);
}
bool kortId(const std::string& s) {
    if (s.empty() || s.size() > 120 || s.find("..") != std::string::npos) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return g_ascii_isalnum(c) || c == '-' || c == '_';
    });
}
bool xyz(double n) { return std::isfinite(n) && n >= -4 && n <= 300; }

} // namespace

bool giltigtPapperId(const std::string& id) {
    if (id.size() != 36) return false;
    for (size_t i = 0; i < id.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (id[i] != '-') return false;
        } else if (!(id[i] >= '0' && id[i] <= '9') && !(id[i] >= 'a' && id[i] <= 'f')) return false;
    }
    return true;
}

std::string nyttPapperId() {
    gchar* id = g_uuid_string_random();
    std::string ut = id ? id : "";
    g_free(id);
    return ut;
}

std::string papperMapp(const std::string& elev) {
    return datamapp() + "/raknepapper/" + filnamn(elev);
}

std::string papperFil(const std::string& elev, const std::string& id) {
    return giltigtPapperId(id) ? papperMapp(elev) + "/" + id + ".json" : "";
}

bool papperKontroll(const PapperData& p, std::string* fel) {
    if (!giltigtPapperId(p.id) || !kortId(p.kort) || p.mallIndex < 0 ||
        p.mallIndex > 10000 || p.variantIndex < 0 || p.variantIndex > 10000 ||
        !tryggText(p.fraga, 8192)) return felmed(fel, "ogiltig identitet eller fråga");
    if (!rutkoordinat(p.rad) || !rutkoordinat(p.kol) || !rutkoordinat(p.start) ||
        p.lage < 0 || p.lage > 2) return felmed(fel, "ogiltig markör");
    if (p.c.size() > 2500 || p.streck.size() > 600 || p.drag.size() > PAPPER_MAX_STRECK)
        return felmed(fel, "för mycket innehåll på räknepappret");
    for (const auto& [rc, c] : p.c) {
        if (!rutkoordinat(rc.first) || !rutkoordinat(rc.second) ||
            !tryggText(c.t, 16) || !tryggText(c.liten, 16))
            return felmed(fel, "ogiltig ruttext");
    }
    for (const auto& s : p.streck) {
        if (!rutkoordinat(s.rad) || !rutkoordinat(s.fran) ||
            !rutkoordinat(s.till) || s.till < s.fran)
            return felmed(fel, "ogiltigt summastreck");
    }
    size_t points = 0;
    for (const auto& stroke : p.drag) {
        if (stroke.empty() || stroke.size() > 10000 || points + stroke.size() > PAPPER_MAX_PUNKTER)
            return felmed(fel, "för många pennpunkter");
        points += stroke.size();
        for (const auto& v : stroke)
            if (!xyz(v.first) || !xyz(v.second) || !std::isfinite(v.tryck) ||
                v.tryck < 0 || v.tryck > 1)
                return felmed(fel, "ogiltig pennpunkt");
    }
    return true;
}

bool papperTillJson(const PapperData& p, std::string& ut, std::string* fel) {
    if (!papperKontroll(p, fel)) return false;
    Json root = Json::objekt();
    root.satt("format", FORMAT); root.satt("version", 1);
    root.satt("id", p.id); root.satt("kort", p.kort);
    root.satt("fraga", p.fraga); root.satt("mall", p.mallIndex); root.satt("variant", p.variantIndex);
    root.satt("rad", p.rad); root.satt("kol", p.kol); root.satt("start", p.start);
    root.satt("lage", p.lage); root.satt("liten", p.liten); root.satt("vanster", p.vanster);
    Json cells = Json::lista();
    for (const auto& [rc, c] : p.c) {
        Json v = Json::objekt(); v.satt("rad", rc.first); v.satt("kol", rc.second);
        v.satt("text", c.t); v.satt("liten", c.liten); cells.lagg(v);
    }
    root.satt("rutor", cells);
    Json lines = Json::lista();
    for (const auto& l : p.streck) {
        Json v = Json::objekt();
        v.satt("rad", l.rad); v.satt("fran", l.fran); v.satt("till", l.till); lines.lagg(v);
    }
    root.satt("summastreck", lines);
    Json strokes = Json::lista();
    for (const auto& stroke : p.drag) {
        Json points = Json::lista();
        for (const auto& pt : stroke) {
            Json point = Json::lista();
            point.lagg(pt.first); point.lagg(pt.second);
            point.lagg(pt.tryck); point.lagg(pt.tryckFinns);
            points.lagg(point);
        }
        strokes.lagg(points);
    }
    root.satt("pennstreck", strokes);
    ut = root.skriv();
    if (ut.size() > PAPPER_MAX_BYTE) {
        ut.clear();
        return felmed(fel, "räknepappret överstiger 2 MiB");
    }
    return true;
}

bool papperFranJson(const std::string& text, PapperData& ut, std::string* fel) {
    if (text.size() > PAPPER_MAX_BYTE) return felmed(fel, "för stor fil");
    try {
        const Json j = Json::las(text);
        if (!j.arObjekt() || j["format"].str() != FORMAT || j["version"].heltal() != 1 ||
            !j["rutor"].arLista() || !j["summastreck"].arLista() || !j["pennstreck"].arLista())
            return felmed(fel, "okänt eller trasigt räknepappersformat");
        PapperData p;
        p.id = j["id"].str(); p.kort = j["kort"].str(); p.fraga = j["fraga"].str();
        p.mallIndex = j["mall"].heltal(-1); p.variantIndex = j["variant"].heltal(-1);
        p.rad = j["rad"].heltal(1); p.kol = j["kol"].heltal(2);
        p.start = j["start"].heltal(2); p.lage = j["lage"].heltal();
        p.liten = j["liten"].sant(); p.vanster = j["vanster"].sant();
        if (j["rutor"].storlek() > 2500 || j["summastreck"].storlek() > 600 ||
            j["pennstreck"].storlek() > PAPPER_MAX_STRECK)
            return felmed(fel, "för många element");
        for (const auto& c : j["rutor"].element()) {
            if (!c.arObjekt()) return felmed(fel, "ogiltig ruta");
            int row = c["rad"].heltal(-1), col = c["kol"].heltal(-1);
            if (!rutkoordinat(row) || !rutkoordinat(col)) return felmed(fel, "rutan ligger utanför");
            if (!p.c.emplace(std::make_pair(row, col),
                PapperCell{c["text"].str(), c["liten"].str()}).second)
                return felmed(fel, "dubblerad ruta");
        }
        for (const auto& l : j["summastreck"].element()) {
            if (!l.arObjekt()) return felmed(fel, "ogiltigt streck");
            p.streck.push_back({l["rad"].heltal(-1), l["fran"].heltal(-1), l["till"].heltal(-1)});
        }
        size_t total = 0;
        for (const auto& st : j["pennstreck"].element()) {
            if (!st.arLista() || st.storlek() > 10000 ||
                total + st.storlek() > PAPPER_MAX_PUNKTER)
                return felmed(fel, "för många punkter");
            std::vector<PapperPunkt> points;
            points.reserve(st.storlek());
            total += st.storlek();
            for (const auto& pt : st.element()) {
                if (!pt.arLista() || pt.storlek() != 4)
                    return felmed(fel, "ogiltig pennpunkt");
                points.push_back({pt[0].tal(), pt[1].tal(), pt[2].tal(1), pt[3].sant()});
            }
            p.drag.push_back(std::move(points));
        }
        if (!papperKontroll(p, fel)) return false;
        ut = std::move(p);  // gamla data bevaras vid varje läsfel
        return true;
    } catch (const std::exception&) {
        return felmed(fel, "ogiltig JSON");
    }
}

bool papperSpara(const std::string& elev, const PapperData& p, std::string* fel) {
    std::string innehall;
    if (!papperTillJson(p, innehall, fel)) return false;
    const std::string mapp = papperMapp(elev), path = papperFil(elev, p.id);
    if (path.empty()) return felmed(fel, "ogiltigt filnamn");
    if (g_mkdir_with_parents(mapp.c_str(), 0700) != 0)
        return felmed(fel, "kunde inte skapa privat lagringsmapp");
    GError* e = nullptr;
    const bool ok = g_file_set_contents(path.c_str(), innehall.c_str(), (gssize)innehall.size(), &e);
    if (!ok) {
        if (fel) *fel = e ? e->message : "kunde inte skriva filen";
        g_clear_error(&e);
        return false;
    }
    g_chmod(path.c_str(), 0600);
    return true;
}

bool papperLadda(const std::string& elev, const std::string& id, PapperData& ut, std::string* fel) {
    const std::string path = papperFil(elev, id);
    if (path.empty()) return felmed(fel, "osäkert filnamn");
    GStatBuf st;
    if (g_stat(path.c_str(), &st) != 0) return felmed(fel, "räknepappret finns inte");
    if (st.st_size < 0 || (size_t)st.st_size > PAPPER_MAX_BYTE)
        return felmed(fel, "för stor fil");
    gchar* data = nullptr; gsize n = 0;
    GError* e = nullptr;
    if (!g_file_get_contents(path.c_str(), &data, &n, &e)) {
        if (fel) *fel = e ? e->message : "kunde inte läsa filen";
        g_clear_error(&e);
        return false;
    }
    std::string text(data, n);
    g_free(data);
    PapperData candidate;
    if (!papperFranJson(text, candidate, fel)) return false;
    if (candidate.id != id) return felmed(fel, "fel id i sparad fil");
    ut = std::move(candidate);
    return true;
}

} // namespace matte
