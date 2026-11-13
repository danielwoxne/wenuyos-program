#include "Logg.h"

#include "Framsteg.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <fstream>
#include <sstream>

namespace matte {

size_t LOGG_MAX = 2 * 1024 * 1024;

std::string loggfil(const std::string& elev) { return datamapp() + "/logg/" + elev + ".jsonl"; }

namespace {
std::string nu(const char* format) {
    GDateTime* t = g_date_time_new_now_local();
    gchar* s = g_date_time_format(t, format);
    std::string r = s;
    g_free(s);
    g_date_time_unref(t);
    return r;
}

// Filen har vuxit: behåll den nyare halvan (från en radbrytning)
void krymp(const std::string& fil) {
    gchar* data = nullptr;
    gsize n = 0;
    if (!g_file_get_contents(fil.c_str(), &data, &n, nullptr)) return;
    std::string s(data, n);
    g_free(data);
    size_t fran = s.find('\n', s.size() / 2);
    if (fran == std::string::npos) return;
    std::string kvar = s.substr(fran + 1);
    g_file_set_contents(fil.c_str(), kvar.data(), (gssize)kvar.size(), nullptr);
}
}  // namespace

void logga(const std::string& elev, const std::string& handelse, Json data) {
    if (elev.empty()) return;
    std::string mapp = datamapp() + "/logg";
    g_mkdir_with_parents(mapp.c_str(), 0700);
    Json rad = Json::objekt();
    rad.satt("t", nu("%Y-%m-%dT%H:%M:%S"));
    rad.satt("h", handelse);
    if (data.arObjekt()) for (auto& [k, v] : data.falt()) rad.satt(k, v);
    std::string fil = loggfil(elev);
    {
        std::ofstream ut(fil, std::ios::app);
        if (!ut) { g_warning("Matte: kunde inte skriva loggen %s", fil.c_str()); return; }
        ut << rad.skriv() << '\n';
    }
    GStatBuf st;
    if (g_stat(fil.c_str(), &st) == 0 && (size_t)st.st_size > LOGG_MAX) krymp(fil);
}

std::vector<Json> lasLogg(const std::string& elev, int dagar) {
    std::vector<Json> ut;
    std::ifstream in(loggfil(elev));
    if (!in) return ut;
    std::string gräns;
    if (dagar > 0) gräns = plusDagar(idag(), -dagar);
    std::string rad;
    while (std::getline(in, rad)) {
        if (rad.empty()) continue;
        try {
            Json j = Json::las(rad);
            if (!gräns.empty() && j["t"].str().substr(0, 10) < gräns) continue;
            ut.push_back(std::move(j));
        } catch (...) {   // en trasig rad (strömavbrott mitt i en skrivning) hoppas över
        }
    }
    return ut;
}

bool rensaLogg(const std::string& elev) {
    std::string f = loggfil(elev);
    return !g_file_test(f.c_str(), G_FILE_TEST_EXISTS) || g_remove(f.c_str()) == 0;
}

}  // namespace matte
