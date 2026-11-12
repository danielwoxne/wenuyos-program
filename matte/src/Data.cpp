#include "Data.h"

#include "Lara.h"

#include <glib.h>

#include <unistd.h>

namespace matte {

std::string Data::hittaMapp() {
    if (const char* m = g_getenv(datamiljo().c_str())) return m;     // utveckling och tester (MATTE_DATA)
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n > 0) {
        buf[n] = 0;
        gchar* bin = g_path_get_dirname(buf);
        gchar* rot = g_path_get_dirname(bin);
        std::string m = std::string(rot) + "/share/" + appId();
        g_free(bin); g_free(rot);
        if (g_file_test((m + "/kort.json").c_str(), G_FILE_TEST_EXISTS)) return m;
    }
    return "/usr/share/" + appId();
}

bool Data::las(const std::string& mapp, std::string* fel) {
    mapp_ = mapp;
    Json allt;
    try {
        allt = Json::lasFil(mapp + "/kort.json");
    } catch (const std::exception& e) {
        if (fel) *fel = e.what();
        return false;
    }
    if (!allt.arObjekt()) { if (fel) *fel = "kort.json saknas i " + mapp; return false; }
    for (auto& [id, namn] : allt["omraden"].falt()) { omraden_[id] = namn.str(); omradeOrdning_.push_back(id); }
    for (auto& s : allt["stadier"].element()) stadier_.push_back({s[0].str(), s[1].str()});
    for (auto& m : allt["laroplan"].element())
        mal_.push_back({m["id"].str(), m["stadium"].str(), m["omrade"].str(), m["text"].str(), m["kort"].strangar()});
    for (auto& k : allt["kort"].element()) {
        Kort c;
        c.id = k["id"].str(); c.titel = k["titel"].str(); c.omrade = k["omrade"].str(); c.ikon = k["ikon"].str();
        c.bild = k["bild"].str(); c.bildtext = k["bildtext"].str(); c.sager = k["sager"].str();
        c.galler = k["galler"].str(); c.galler_inte = k["galler_inte"].str(); c.kalla = k["kalla"].str();
        c.forkunskap = k["forkunskap"].strangar(); c.ateruppta = k["ateruppta"].strangar();
        for (auto& r : k["rader"].element()) c.rader.push_back({r["sager"].str(), r["kommentar"].str(), r["figur"], r["skriver"].strangar()});
        for (auto& e : k["exempel"].element()) c.exempel.push_back({e["rubrik"].str(), e["text"].str(), e["figur"], e["steg"].strangar()});
        for (auto& [fid, text] : k["feltankar"].falt()) c.feltankar[fid] = text.str();
        index_[c.id] = kort_.size();
        kort_.push_back(std::move(c));
    }
    try {   // ordlistan är valfri – programmet fungerar utan den
        Json ol = Json::lasFil(mapp + "/ordlista.json");
        for (auto& o : ol.element()) {
            Ord x{o["ord"].str(), o["forklaring"].str(), o["exempel"].str(), o["former"].strangar(), o["se"].strangar(), o["figur"]};
            for (auto& f : x.former) former_[f] = (int)ord_.size();
            ord_.push_back(std::move(x));
        }
    } catch (const std::exception& e) {
        g_warning("Matte: ordlistan gick inte att läsa: %s", e.what());
    }
    return !kort_.empty();
}

std::string Data::ikon(const std::string& namn) const {
    std::string p = mapp_ + "/ikoner/" + namn + ".png";
    return g_file_test(p.c_str(), G_FILE_TEST_EXISTS) ? p : mapp_ + "/ikoner/fragetecken.png";
}

int Data::ordFor(const std::string& form) const {
    auto f = former_.find(form);
    return f == former_.end() ? -1 : f->second;
}

int Data::ordMedNamn(const std::string& ord) const {
    for (size_t i = 0; i < ord_.size(); ++i) if (ord_[i].ord == ord) return (int)i;
    return -1;
}

const Kort* Data::kort(const std::string& id) const {
    auto f = index_.find(id);
    return f == index_.end() ? nullptr : &kort_[f->second];
}

std::string Data::omradeNamn(const std::string& id) const {
    auto f = omraden_.find(id);
    return f == omraden_.end() ? id : f->second;
}

const std::vector<Mall>& Data::mallar(const std::string& kortId) const {
    auto f = mallar_.find(kortId);
    if (f != mallar_.end()) return f->second;
    std::vector<Mall> ut;
    Json j;
    try { j = Json::lasFil(mapp_ + "/uppgifter/" + kortId + ".json"); } catch (...) {}
    for (auto& m : j.element()) {
        Mall mall;
        mall.niva = m["niva"].heltal(2); mall.typ = m["typ"].str("text"); mall.form = m["form"].str("auto");
        mall.tolerans = m["tol"].tal(); mall.oordnad = m["oordnad"].sant();
        for (auto& v : m["varianter"].element()) {
            Variant x;
            x.fraga = v["f"].str(); x.ledtrad = v["h"].str();
            x.steg = v["s"].strangar(); x.losning = v["l"].strangar(); x.alternativ = v["alt"].strangar();
            x.falt = v["falt"].strangar(); x.facit = v["facit"].strangar(); x.facitText = v["ft"].strangar();
            x.facitSvar = v["fs"].strangar();
            x.figur = v["fig"];
            for (auto& f : v["fel"].element()) x.fel.push_back({f["k"].str(), f["v"].str(), f["x"].str(), f["id"].str(), f["s"].str()});
            mall.varianter.push_back(std::move(x));
        }
        ut.push_back(std::move(mall));
    }
    return mallar_[kortId] = std::move(ut);
}

}  // namespace matte
