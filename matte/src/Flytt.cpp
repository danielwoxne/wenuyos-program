#include "Flytt.h"

#include "Framsteg.h"
#include "Json.h"
#include "Lara.h"
#include "Logg.h"
#include "PapperLagring.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <stdexcept>
#include <utility>
#include <vector>

namespace matte {

namespace {
const char* FORMAT = "se.wenuyos.lara.elev";

bool lasAllt(const std::string& fil, std::string& ut) {
    gchar* d = nullptr;
    gsize n = 0;
    if (!g_file_get_contents(fil.c_str(), &d, &n, nullptr)) return false;
    ut.assign(d, n);
    g_free(d);
    return true;
}

bool skrivAllt(const std::string& fil, const std::string& data) {
    return g_file_set_contents(fil.c_str(), data.data(), (gssize)data.size(), nullptr);
}

// En teckning får bara heta något.png – inga mappar, inga ".."
bool felmed(std::string* fel, const std::string& text) {
    if (fel) *fel = text;
    return false;
}
// En separat mapp för räknepapper, endast UUID.json.
bool sakertPappernamn(const std::string& f) {
    return f.size() == 41 && f.substr(36) == ".json" && giltigtPapperId(f.substr(0, 36));
}

bool sakertNamn(const std::string& f) {
    return !f.empty() && f.size() < 200 && f.find('/') == std::string::npos && f.find('\\') == std::string::npos &&
           f.find("..") == std::string::npos && f[0] != '.' && f.size() > 4 && f.compare(f.size() - 4, 4, ".png") == 0;
}
}  // namespace

bool exporteraElev(const std::string& namn, const std::string& fil, std::string* fel) {
    std::string elevtext;
    if (!lasAllt(datamapp() + "/" + namn + ".json", elevtext)) { if (fel) *fel = "eleven finns inte"; return false; }
    Json ut = Json::objekt();
    ut.satt("format", FORMAT);
    ut.satt("version", 1);
    ut.satt("program", appId());
    ut.satt("namn", namn);
    try { ut.satt("elev", Json::las(elevtext)); } catch (const std::exception& e) { if (fel) *fel = e.what(); return false; }
    std::string logg;
    if (lasAllt(loggfil(namn), logg)) ut.satt("logg", logg);
    Json rit = Json::objekt();
    if (GDir* d = g_dir_open(ritmapp(namn).c_str(), 0, nullptr)) {
        while (const char* f = g_dir_read_name(d)) {
            std::string data;
            if (!sakertNamn(f) || !lasAllt(ritmapp(namn) + "/" + f, data)) continue;
            gchar* b64 = g_base64_encode((const guchar*)data.data(), data.size());
            rit.satt(f, std::string(b64));
            g_free(b64);
        }
        g_dir_close(d);
    }
    ut.satt("ritningar", rit);
    // Samtliga papper i samma portabla elevfil. Ingen gammal elevexport bryts:
    // det här fältet är valfritt och importerade 1.2-filer saknar det.
    Json papper = Json::objekt();
    size_t antal = 0, totalt = 0;
    if (GDir* d = g_dir_open(papperMapp(namn).c_str(), 0, nullptr)) {
        while (const char* fn = g_dir_read_name(d)) {
            const std::string filnamn = fn;
            if (filnamn.size() != 41 || filnamn.substr(36) != ".json") continue;
            const std::string id = filnamn.substr(0, 36);
            if (!giltigtPapperId(id)) continue;
            PapperData data;
            std::string orsak, innehall;
            if (!papperLadda(namn, id, data, &orsak) ||
                !papperTillJson(data, innehall, &orsak)) {
                g_dir_close(d);
                return felmed(fel, "kunde inte exportera räknepapper: " + orsak);
            }
            totalt += innehall.size();
            if (++antal > 500 || totalt > 90U * 1024U * 1024U) {
                g_dir_close(d);
                return felmed(fel, "för många räknepapper för en enda elevfil");
            }
            papper.satt(filnamn, std::move(innehall));
        }
        g_dir_close(d);
    }
    ut.satt("raknepapper", papper);
    if (!skrivAllt(fil, ut.skriv())) { if (fel) *fel = "kunde inte skriva " + fil; return false; }
    return true;
}

std::string importeraElev(const std::string& fil, bool ersatt, std::string* fel, bool* fanns, std::string* visningsnamn) {
    if (fanns) *fanns = false;
    GStatBuf st;
    if (g_stat(fil.c_str(), &st) != 0) { if (fel) *fel = "filen finns inte"; return ""; }
    if (st.st_size > 200 * 1024 * 1024) { if (fel) *fel = "filen är för stor"; return ""; }
    std::string text;
    Json in;
    try {
        if (!lasAllt(fil, text)) throw std::runtime_error("kunde inte läsa filen");
        in = Json::las(text);
    } catch (const std::exception& e) { if (fel) *fel = e.what(); return ""; }
    if (in["format"].str() != FORMAT) { if (fel) *fel = "det är ingen elevfil från Matte"; return ""; }
    if (in["program"].str() != appId()) { if (fel) *fel = "filen hör till ett annat program (" + in["program"].str() + ")"; return ""; }
    std::string namn = filnamn(in["namn"].str());   // aldrig ett namn med / eller .. i
    if (namn.empty() || !in["elev"].arObjekt()) { if (fel) *fel = "filen saknar elevens framsteg"; return ""; }
    if (visningsnamn) *visningsnamn = in["elev"]["visningsnamn"].str(namn);
    std::string elevfil = datamapp() + "/" + namn + ".json";
    if (g_file_test(elevfil.c_str(), G_FILE_TEST_EXISTS) && !ersatt) { if (fanns) *fanns = true; return namn; }
    // Validera hela det nya filformatet innan en enda elevprofil skrivs.
    // Inget godtyckligt filnamn eller för stora papper får nå disk.
    std::vector<std::pair<std::string, PapperData>> papper;
    size_t rakneByte = 0;
    if (in["raknepapper"].arObjekt()) {
        for (const auto& [filnamn, data] : in["raknepapper"].falt()) {
            if (!sakertPappernamn(filnamn) || !data.arText())
                { felmed(fel, "ogiltigt räknepappersnamn eller innehåll"); return ""; }
            PapperData item;
            std::string orsak, text = data.str();
            rakneByte += text.size();
            if (papper.size() >= 500 || rakneByte > 90U * 1024U * 1024U ||
                !papperFranJson(text, item, &orsak) ||
                item.id + ".json" != filnamn)
                { felmed(fel, "osäkert räknepapper: " + orsak); return ""; }
            papper.emplace_back(filnamn, std::move(item));
        }
    }
    g_mkdir_with_parents(datamapp().c_str(), 0700);
    Json elev = in["elev"];
    elev.satt("namn", namn);
    if (!skrivAllt(elevfil, elev.skriv(true))) { if (fel) *fel = "kunde inte spara eleven"; return ""; }
    if (in["logg"].arText()) {
        g_mkdir_with_parents((datamapp() + "/logg").c_str(), 0700);
        skrivAllt(loggfil(namn), in["logg"].str());
    }
    if (in["ritningar"].arObjekt()) {
        g_mkdir_with_parents(ritmapp(namn).c_str(), 0700);
        for (auto& [f, b64] : in["ritningar"].falt()) {
            if (!sakertNamn(f)) continue;
            gsize n = 0;
            guchar* data = g_base64_decode(b64.str().c_str(), &n);
            if (n > 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')   // bara riktiga PNG
                skrivAllt(ritmapp(namn) + "/" + f, std::string((const char*)data, n));
            g_free(data);
        }
    }
    for (const auto& [filnamn, p] : papper) {
        std::string orsak;
        if (!papperSpara(namn, p, &orsak)) {
            if (fel) *fel = "kunde inte skriva räknepapper: " + orsak;
            return "";
        }
    }
    return namn;
}

}  // namespace matte
