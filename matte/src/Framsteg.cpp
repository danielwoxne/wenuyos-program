#include "Framsteg.h"

#include "Json.h"
#include "Lara.h"
#include "Text.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <algorithm>
#include <fstream>

namespace matte {

namespace {
const int INTERVALL[] = {1, 3, 7, 14, 30, 60, 120};
const int ANTAL_INTERVALL = 7;
const int KRAV_RATT_I_RAD = 4;      // så många rätt i rad flyttar Övar → Behärskar
}  // namespace

std::string datamapp() {
    const char* x = g_getenv("XDG_DATA_HOME");
    std::string bas = x && *x ? x : g_get_user_data_dir();
    std::string m = bas + "/" + appId();
    std::string gammal = bas + "/matte";                 // Matte 0.1–0.3.0 sparade här – flytta en gång
    if (appId() == "se.wenuyos.app.matte" && !g_file_test(m.c_str(), G_FILE_TEST_IS_DIR) && g_file_test(gammal.c_str(), G_FILE_TEST_IS_DIR))
        g_rename(gammal.c_str(), m.c_str());
    return m;
}

std::string ritmapp(const std::string& elev) { return datamapp() + "/ritningar/" + elev; }

std::string filnamn(const std::string& namn) {
    std::string r;
    bool streck = false;
    std::string g = gemener(trim(namn));
    for (const char* p = g.c_str(); *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (g_unichar_isalnum(c) || c == '_') {
            if (streck && !r.empty()) r += '-';
            streck = false;
            char buf[8]; int n = g_unichar_to_utf8(c, buf); r.append(buf, n);
        } else streck = true;
    }
    return r.empty() ? "elev" : r;
}

std::vector<std::string> elever() {
    std::vector<std::string> ut;
    GDir* d = g_dir_open(datamapp().c_str(), 0, nullptr);
    if (!d) return ut;
    while (const char* f = g_dir_read_name(d)) {
        std::string s = f;
        if (s.size() > 5 && s.compare(s.size() - 5, 5, ".json") == 0) ut.push_back(s.substr(0, s.size() - 5));
    }
    g_dir_close(d);
    std::sort(ut.begin(), ut.end());
    return ut;
}

std::string senasteElev() {
    gchar* t = nullptr;
    std::string r = "elev";
    if (g_file_get_contents((datamapp() + "/senaste").c_str(), &t, nullptr, nullptr)) {
        std::string s = trim(t);
        if (!s.empty()) r = s;
    }
    g_free(t);
    return r;
}

void sattSenasteElev(const std::string& namn) {
    std::string m = datamapp();
    g_mkdir_with_parents(m.c_str(), 0700);
    g_file_set_contents((m + "/senaste").c_str(), namn.c_str(), -1, nullptr);
}

std::string idag() {
    GDateTime* nu = g_date_time_new_now_local();
    gchar* s = g_date_time_format(nu, "%Y-%m-%d");
    std::string r = s;
    g_free(s); g_date_time_unref(nu);
    return r;
}

std::string plusDagar(const std::string& datum, int dagar) {
    int y = 0, m = 0, d = 0;
    if (sscanf(datum.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return datum;
    GDate* g = g_date_new_dmy((GDateDay)d, (GDateMonth)m, (GDateYear)y);
    if (dagar >= 0) g_date_add_days(g, (guint)dagar); else g_date_subtract_days(g, (guint)-dagar);   // add_days tar bara positiva
    char buf[16];
    snprintf(buf, sizeof buf, "%04d-%02d-%02d", g_date_get_year(g), g_date_get_month(g), g_date_get_day(g));
    g_date_free(g);
    return buf;
}

// ---- läsa och spara ---------------------------------------------------------------------------

Elev Elev::ladda(const std::string& namn) {
    Elev e;
    e.namn = namn;
    Json d;
    try { d = Json::lasFil(datamapp() + "/" + namn + ".json"); } catch (...) {}   // trasig fil → börja om
    if (!d.arObjekt()) return e;
    for (auto& [id, v] : d["kort"].falt()) {
        Kortstatus s;
        s.lage = v["lage"].heltal(); s.rattIRad = v["ratt_i_rad"].heltal(); s.steg = v["steg"].heltal();
        s.nasta = v["nasta"].str(); s.antal = v["antal"].heltal(); s.antalRatt = v["antal_ratt"].heltal();
        s.senast = v["senast"].str(); s.gamlaFeltankar = v["feltankar"].strangar();
        for (auto& h : v["historik"].element()) s.historik.push_back(h.heltal());
        for (auto& [k, n] : v["feltanke_antal"].falt()) s.feltankeAntal[k] = n.heltal();
        for (auto& [k, n] : v["feltanke_totalt"].falt()) s.feltankeTotalt[k] = n.heltal();
        e.kort[id] = s;
    }
    e.regelbok = d["regelbok"].strangar();
    e.placerad = d["placerad"].sant();
    e.leklage = d["lekläge"].sant();
    e.visningsnamn = d["visningsnamn"].str(namn);
    e.lattlast = d["lättläst"].sant();
    e.dagensKlar = d["dagens"]["klar"].str();
    e.dagensSvit = d["dagens"]["svit"].heltal();
    e.sifferknappar = d["sifferknappar"].sant();
    e.zoom = std::clamp(d["zoom"].tal(1.0), 0.8, 1.6);
    for (auto& r : d["ritningar"].element())
        e.ritningar.push_back({r["kort"].str(), r["fil"].str(), r["fraga"].str(), r["tid"].str()});
    return e;
}

bool Elev::spara() const {
    Json k = Json::objekt();
    for (auto& [id, s] : kort) {
        Json v = Json::objekt();
        v.satt("lage", s.lage); v.satt("ratt_i_rad", s.rattIRad); v.satt("steg", s.steg); v.satt("nasta", s.nasta);
        v.satt("antal", s.antal); v.satt("antal_ratt", s.antalRatt); v.satt("senast", s.senast);
        Json f = Json::lista(); for (auto& x : s.gamlaFeltankar) f.lagg(x); v.satt("feltankar", f);
        Json h = Json::lista(); for (int x : s.historik) h.lagg(x); v.satt("historik", h);
        Json a = Json::objekt(); for (auto& [x, n] : s.feltankeAntal) a.satt(x, n); v.satt("feltanke_antal", a);
        Json t = Json::objekt(); for (auto& [x, n] : s.feltankeTotalt) t.satt(x, n); v.satt("feltanke_totalt", t);
        k.satt(id, v);
    }
    Json rb = Json::lista();
    for (auto& r : regelbok) rb.lagg(r);
    Json d = Json::objekt();
    d.satt("namn", namn); d.satt("kort", k); d.satt("regelbok", rb); d.satt("placerad", placerad);
    d.satt("lekläge", leklage); d.satt("lättläst", lattlast);
    { Json dg = Json::objekt(); dg.satt("klar", dagensKlar); dg.satt("svit", dagensSvit); d.satt("dagens", dg); } d.satt("sifferknappar", sifferknappar); if (zoom != 1.0) d.satt("zoom", zoom); d.satt("visningsnamn", visningsnamn.empty() ? namn : visningsnamn);
    Json rl = Json::lista();
    size_t fran = ritningar.size() > 200 ? ritningar.size() - 200 : 0;
    for (size_t i = fran; i < ritningar.size(); ++i) {
        Json r = Json::objekt();
        r.satt("kort", ritningar[i].kort); r.satt("fil", ritningar[i].fil); r.satt("fraga", ritningar[i].fraga); r.satt("tid", ritningar[i].tid);
        rl.lagg(r);
    }
    d.satt("ritningar", rl);
    std::string m = datamapp();
    g_mkdir_with_parents(m.c_str(), 0700);
    // säkert: g_file_set_contents skriver till en temporär fil och byter namn
    return g_file_set_contents((m + "/" + namn + ".json").c_str(), d.skriv(true).c_str(), -1, nullptr);
}

void Elev::settKort(const std::string& id) {
    if (std::find(regelbok.begin(), regelbok.end(), id) == regelbok.end()) regelbok.push_back(id);
    Kortstatus& s = status(id);
    if (s.lage == LAR) s.lage = OVAR;
}

void Elev::svarade(const std::string& id, bool ratt, const std::string& feltankeId, int mallIndex, const std::string& dag) {
    std::string d = dag.empty() ? idag() : dag;
    Kortstatus& s = status(id);
    s.antal++; s.senast = d;
    s.historik.push_back(ratt ? 1 : 0);
    if (s.historik.size() > 10) s.historik.erase(s.historik.begin());
    if (!feltankeId.empty() && !ratt) { s.feltankeAntal[feltankeId]++; s.feltankeTotalt[feltankeId]++; }
    if (ratt && mallIndex >= 0) {   // ett rätt svar på samma mall räknar ned dess feltankar
        std::string pre = std::to_string(mallIndex) + "|";
        for (auto it = s.feltankeAntal.begin(); it != s.feltankeAntal.end();) {
            if (it->first.rfind(pre, 0) == 0 && --it->second <= 0) it = s.feltankeAntal.erase(it);
            else ++it;
        }
    }
    if (ratt) {
        s.antalRatt++; s.rattIRad++;
        if (s.lage == BEHARSKAR) s.steg = std::min(s.steg + 1, ANTAL_INTERVALL - 1);
        else if (s.rattIRad >= KRAV_RATT_I_RAD) { s.lage = BEHARSKAR; s.steg = 0; }
        s.nasta = s.lage == BEHARSKAR ? plusDagar(d, INTERVALL[s.steg]) : "";
    } else {
        s.rattIRad = 0;
        if (s.lage == BEHARSKAR) { s.steg = std::max(s.steg - 2, 0); s.nasta = plusDagar(d, INTERVALL[s.steg]); }
    }
}

std::vector<std::string> Elev::attRepetera(const std::string& dag) const {
    std::string d = dag.empty() ? idag() : dag;
    std::vector<std::string> ut;
    for (auto& [id, s] : kort) if (s.lage == BEHARSKAR && !s.nasta.empty() && s.nasta <= d) ut.push_back(id);
    return ut;
}

int Elev::stjarnor(const std::string& id) const {
    auto f = kort.find(id);
    if (f == kort.end()) return 0;
    const Kortstatus& s = f->second;
    if (s.lage == BEHARSKAR) return s.steg >= 3 ? 3 : 2;
    return s.antalRatt > 0 ? 1 : 0;
}

int Elev::allaStjarnor() const {
    int n = 0;
    for (auto& [id, s] : kort) n += stjarnor(id);
    return n;
}

// Tre kort eleven redan mött – helst sådana som inte övats på länge. Samma tre hela dagen.
std::vector<std::string> Elev::dagensKort(const std::string& dag) const {
    std::string d = dag.empty() ? idag() : dag;
    std::vector<std::pair<std::string, std::string>> motta;   // senast, id
    for (auto& [id, s] : kort) if (s.antal > 0 || s.lage == BEHARSKAR) motta.push_back({s.senast, id});
    if (motta.size() < 2) return {};
    std::sort(motta.begin(), motta.end());                    // äldst först
    size_t pool = std::min<size_t>(motta.size(), 8);
    std::seed_seq fro(d.begin(), d.end());
    std::mt19937 rnd(fro);
    std::vector<std::string> ut;
    std::vector<size_t> idx(pool);
    for (size_t i = 0; i < pool; ++i) idx[i] = i;
    std::shuffle(idx.begin(), idx.end(), rnd);
    for (size_t i = 0; i < idx.size() && ut.size() < 3; ++i) ut.push_back(motta[idx[i]].second);
    return ut;
}

void Elev::dagensGjorda(const std::string& dag) {
    std::string d = dag.empty() ? idag() : dag;
    if (dagensKlar == d) return;
    dagensSvit = dagensKlar == plusDagar(d, -1) ? dagensSvit + 1 : 1;
    dagensKlar = d;
}

std::map<int, int> Elev::sammanfattning() const {
    std::map<int, int> n{{LAR, 0}, {OVAR, 0}, {BEHARSKAR, 0}};
    for (auto& [id, s] : kort) n[s.lage]++;
    return n;
}

// ---- anpassning --------------------------------------------------------------------------------

int nivaFor(const Kortstatus& s) { return s.rattIRad < 2 ? 1 : s.rattIRad < 4 ? 2 : 3; }

namespace {
const char* INTERAKTIVA[] = {"klick", "farga", "valj", "markera", "dra", "val", "punkt", "kryssa", "para", "mala", "rita"};
bool interaktiv(const std::string& typ) {
    for (auto* t : INTERAKTIVA) if (typ == t) return true;
    return false;
}
}  // namespace

Uppgift valjUppgift(const Data& d, const Kort& k, const Kortstatus& s, bool baraInteraktiva, std::mt19937& rnd, bool* tranar) {
    const auto& mallar = d.mallar(k.id);
    Uppgift u;
    u.kort = &k;
    if (tranar) *tranar = false;
    if (mallar.empty()) return u;
    std::vector<int> tillatna;
    for (size_t i = 0; i < mallar.size(); ++i)
        if (!mallar[i].varianter.empty() && (!baraInteraktiva || interaktiv(mallar[i].typ))) tillatna.push_back((int)i);
    if (tillatna.empty())
        for (size_t i = 0; i < mallar.size(); ++i) if (!mallar[i].varianter.empty()) tillatna.push_back((int)i);
    if (tillatna.empty()) return u;
    int val = -1;
    // 1. Träna feltanken som kommit minst två gånger nyligen (60 % av gångerna)
    std::string bastId; int bast = 0;
    for (auto& [id, n] : s.feltankeAntal) if (n > bast) { bast = n; bastId = id; }
    if (bast >= 2) {
        int idx = atoi(bastId.substr(0, bastId.find('|')).c_str());
        if (std::find(tillatna.begin(), tillatna.end(), idx) != tillatna.end() &&
            std::uniform_real_distribution<double>(0, 1)(rnd) < 0.6) {
            val = idx;
            if (tranar) *tranar = true;
        }
    }
    // 2. Annars nivå efter rätt i rad
    if (val < 0) {
        int niva = nivaFor(s);
        std::vector<int> pa;
        // En rita-uppgift rättas inte – den får komma ibland, inte varannan gång
        bool rita = std::uniform_real_distribution<double>(0, 1)(rnd) < 0.3;
        for (int i : tillatna) if (mallar[i].niva == niva && (rita || mallar[i].typ != "rita")) pa.push_back(i);
        if (pa.empty()) for (int i : tillatna) if (rita || mallar[i].typ != "rita") pa.push_back(i);
        if (pa.empty()) pa = tillatna;
        val = pa[std::uniform_int_distribution<size_t>(0, pa.size() - 1)(rnd)];
    }
    u.mall = &mallar[val];
    u.mallIndex = val;
    u.v = &u.mall->varianter[std::uniform_int_distribution<size_t>(0, u.mall->varianter.size() - 1)(rnd)];
    return u;
}

const Kort* gaTillbaka(const Data& d, const Kort& k, Elev& e) {
    auto& h = e.status(k.id).historik;
    if (h.size() < 6) return nullptr;
    int fel = 0;
    for (size_t i = h.size() - 6; i < h.size(); ++i) fel += h[i] == 0;
    if (fel < 4) return nullptr;
    for (auto& f : k.forkunskap) if (d.kort(f) && e.lage(f) != BEHARSKAR) return d.kort(f);
    for (auto& f : k.forkunskap) if (d.kort(f)) return d.kort(f);
    return nullptr;
}

// ---- placering ---------------------------------------------------------------------------------

std::vector<Placering> placeraAlla(const Data& d) {
    std::map<std::string, std::vector<std::string>> ordning;
    for (auto& [st, namn] : d.stadier())
        for (auto& m : d.laroplan()) {
            if (m.stadium != st) continue;
            auto& v = ordning[m.omrade];
            for (auto& id : m.kort) if (d.kort(id) && std::find(v.begin(), v.end(), id) == v.end()) v.push_back(id);
        }
    std::vector<Placering> ut;
    for (auto& o : d.omradeOrdning()) {
        auto f = ordning.find(o);
        if (f == ordning.end() || f->second.empty()) continue;
        Placering p;
        p.omrade = o; p.kortIds = f->second; p.hi = (int)p.kortIds.size() - 1;
        ut.push_back(p);
    }
    return ut;
}

Uppgift Placering::nasta(const Data& d, std::mt19937& rnd) {
    aktuell = (lo + hi) / 2;
    const Kort* k = d.kort(kortIds[aktuell]);
    Uppgift u;
    u.kort = k;
    const auto& mallar = d.mallar(k->id);
    std::vector<int> val;
    auto duger = [&](size_t i) { return !mallar[i].varianter.empty() && mallar[i].typ != "rita"; };   // placeringen behöver ett facit
    for (size_t i = 0; i < mallar.size(); ++i) if (mallar[i].niva == 2 && duger(i)) val.push_back((int)i);
    if (val.empty()) for (size_t i = 0; i < mallar.size(); ++i) if (duger(i)) val.push_back((int)i);
    if (val.empty()) return u;
    u.mallIndex = val[std::uniform_int_distribution<size_t>(0, val.size() - 1)(rnd)];
    u.mall = &mallar[u.mallIndex];
    u.v = &u.mall->varianter[std::uniform_int_distribution<size_t>(0, u.mall->varianter.size() - 1)(rnd)];
    return u;
}

void Placering::svara(bool ratt) {
    ++fragor;
    if (ratt) lo = aktuell + 1; else hi = aktuell - 1;
}

void Placering::tillampa(Elev& e) const {
    std::string d = idag();
    for (int i = 0; i < lo && i < (int)kortIds.size(); ++i) {
        Kortstatus& s = e.status(kortIds[i]);
        if (s.lage != BEHARSKAR) { s.lage = BEHARSKAR; s.steg = 1; s.rattIRad = 0; s.nasta = plusDagar(d, INTERVALL[1]); }
        if (std::find(e.regelbok.begin(), e.regelbok.end(), kortIds[i]) == e.regelbok.end()) e.regelbok.push_back(kortIds[i]);
    }
}

}  // namespace matte
