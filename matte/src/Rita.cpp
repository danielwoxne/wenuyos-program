#include "Rita.h"

#include "Algebra.h"
#include "Data.h"
#include "Tal.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace matte {

namespace tema {
const char* TEXT = "#1f2328";
const char* TEXT_SVAG = "#5c6670";
const char* LINJE = "#3a3f44";
const char* VIT = "#ffffff";
const char* ACCENT = "#e8821c";
const char* ACCENT_SVAG = "#fdeedc";
const char* ROD = "#d1403a";
const char* GRON = "#3f9a4a";

std::string farg(const std::string& namn) {
    static const std::map<std::string, std::string> F = {
        {"gul", "#f3ea8f"}, {"bla", "#b9c6ea"}, {"rosa", "#f0b7cf"}, {"gron", "#b8d4a4"}, {"ljusgul", "#efe9a9"},
        {"himmel", "#78b2dd"}, {"sand", "#f2d08c"}, {"aprikos", "#f2c6a6"}, {"mint", "#b7d9bd"}, {"beige", "#e6cfa8"},
        {"lila", "#6f5aa8"}, {"orange", "#f1a63c"}, {"magenta", "#e8609a"}, {"skog", "#4f9a55"}, {"citron", "#f0e24a"},
        {"rod", "#d9534f"}, {"tegel", "#b85c4a"}, {"stal", "#8c9ac2"}};
    if (!namn.empty() && namn[0] == '#') return namn;
    auto f = F.find(namn);
    return f == F.end() ? F.at("gul") : f->second;
}

void satt(cairo_t* cr, const std::string& hex, double alfa) {
    unsigned v = 0;
    if (hex.size() >= 7) v = std::stoul(hex.substr(1, 6), nullptr, 16);
    cairo_set_source_rgba(cr, ((v >> 16) & 255) / 255.0, ((v >> 8) & 255) / 255.0, (v & 255) / 255.0, alfa);
}
}  // namespace tema

// ---- små hjälpare --------------------------------------------------------------------------------

namespace {

double num(const Json& j, double std_) {
    if (j.arTal()) return j.tal();
    if (j.arText()) {
        try { return tolka(j.str()).tal(); } catch (...) { return std_; }
    }
    return std_;
}
int heltal(const Json& j, int std_) { return (int)std::lround(num(j, std_)); }
std::string str(const Json& j, const std::string& std_) { return j.null() ? std_ : j.str(); }
bool sant(const Json& j, bool std_) { return j.typ() == Json::Bool ? j.sant() : std_; }

void rekt(cairo_t* cr, double x, double y, double b, double h, const std::string& fyll, const std::string& kant = "", double tjock = 1.5) {
    cairo_rectangle(cr, x, y, b, h);
    if (!fyll.empty()) { tema::satt(cr, fyll); cairo_fill_preserve(cr); }
    if (!kant.empty()) { tema::satt(cr, kant); cairo_set_line_width(cr, tjock); cairo_stroke(cr); }
    else cairo_new_path(cr);
}
void linje(cairo_t* cr, double x1, double y1, double x2, double y2, const std::string& farg = tema::LINJE, double tjock = 1.5) {
    tema::satt(cr, farg);
    cairo_set_line_width(cr, tjock);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, x1, y1); cairo_line_to(cr, x2, y2); cairo_stroke(cr);
}
void pil(cairo_t* cr, double x1, double y1, double x2, double y2, const std::string& farg = tema::ROD) {
    linje(cr, x1, y1, x2, y2, farg, 2.5);
    double v = std::atan2(y2 - y1, x2 - x1);
    for (double d : {0.5, -0.5}) linje(cr, x2, y2, x2 - 9 * std::cos(v + d), y2 - 9 * std::sin(v + d), farg, 2.5);
}
void bricka(cairo_t* cr, double cx, double cy, double r, const std::string& farg) {
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    tema::satt(cr, farg); cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.2); cairo_set_line_width(cr, 2); cairo_stroke(cr);
}

}  // namespace

std::string talText(double v) {
    char b[32];
    if (std::fabs(v - std::round(v)) < 1e-9) snprintf(b, sizeof b, "%lld", (long long)std::llround(v));
    else snprintf(b, sizeof b, "%g", v);
    std::string s = b;
    for (auto& c : s) if (c == '.') c = ',';
    if (!s.empty() && s[0] == '-') s = "−" + s.substr(1);
    return s;
}

void text(cairo_t* cr, double x, double y, const std::string& s, double storlek, bool fet, const char* farg, char ankare, bool lutande) {
    PangoLayout* l = pango_cairo_create_layout(cr);
    PangoFontDescription* fd = pango_font_description_from_string("Inter, Cantarell, DejaVu Sans, sans");
    pango_font_description_set_absolute_size(fd, storlek * PANGO_SCALE);
    pango_font_description_set_weight(fd, fet ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(l, fd);
    pango_layout_set_text(l, s.c_str(), -1);
    int b = 0, h = 0;
    pango_layout_get_pixel_size(l, &b, &h);
    int baslinje = pango_layout_get_baseline(l) / PANGO_SCALE;
    cairo_save(cr);
    tema::satt(cr, farg ? farg : tema::TEXT);
    cairo_translate(cr, x, y);
    if (lutande) cairo_rotate(cr, -M_PI / 2);
    double dx = ankare == 'm' ? -b / 2.0 : ankare == 'e' ? -b : 0;
    cairo_move_to(cr, dx, -baslinje);   // y är baslinjen, som i SVG
    pango_cairo_show_layout(cr, l);
    cairo_restore(cr);
    pango_font_description_free(fd);
    g_object_unref(l);
}

// Bilder: PNG (lerikonerna) direkt, JPEG (målningarna) via gdk-pixbuf – skalade en gång och sparade.
namespace {
cairo_surface_t* yta(const std::string& sokvag, int b, int h) {
    static std::map<std::string, cairo_surface_t*> cache;
    std::string nyckel = sokvag + "@" + std::to_string(b) + "x" + std::to_string(h);
    if (auto f = cache.find(nyckel); f != cache.end()) return f->second;
    GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(sokvag.c_str(), std::max(1, b), std::max(1, h), FALSE, nullptr);
    cairo_surface_t* s = nullptr;
    if (pb) {
        int w = gdk_pixbuf_get_width(pb), ht = gdk_pixbuf_get_height(pb), n = gdk_pixbuf_get_n_channels(pb);
        int rs = gdk_pixbuf_get_rowstride(pb);
        const guchar* px = gdk_pixbuf_read_pixels(pb);
        s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, ht);
        unsigned char* ut = cairo_image_surface_get_data(s);
        int us = cairo_image_surface_get_stride(s);
        cairo_surface_flush(s);
        for (int y = 0; y < ht; ++y)
            for (int x = 0; x < w; ++x) {
                const guchar* p = px + y * rs + x * n;
                unsigned a = n == 4 ? p[3] : 255;
                uint32_t* d = (uint32_t*)(ut + y * us) + x;
                *d = (a << 24) | ((p[0] * a / 255) << 16) | ((p[1] * a / 255) << 8) | (p[2] * a / 255);
            }
        cairo_surface_mark_dirty(s);
        g_object_unref(pb);
    }
    if (cache.size() > 300) {   // 2 GB-datorerna: håll cachen liten
        for (auto& [k, v] : cache) if (v) cairo_surface_destroy(v);
        cache.clear();
    }
    return cache[nyckel] = s;
}
}  // namespace

bool UTSKRIFT = false;
void bildstorlek(const std::string& sokvag, int& b, int& h);

// Utskrift (PDF): målningen i full upplösning och JPEG-filen som den är – skarp på papper och liten fil
static cairo_surface_t* utskriftsyta(const std::string& sokvag) {
    static std::map<std::string, cairo_surface_t*> cache;
    if (auto f = cache.find(sokvag); f != cache.end()) return f->second;
    int b = 0, h = 0;
    bildstorlek(sokvag, b, h);
    bool jpeg = sokvag.size() > 4 && (sokvag.compare(sokvag.size() - 4, 4, ".jpg") == 0 || sokvag.compare(sokvag.size() - 5, 5, ".jpeg") == 0);
    if (!jpeg) { b = std::min(b, 512); h = std::min(h, 512); }   // lerikonerna räcker i 512 px
    cairo_surface_t* s = yta(sokvag, b, h);
    if (s && jpeg) {
        gchar* data = nullptr; gsize n = 0;
        if (g_file_get_contents(sokvag.c_str(), &data, &n, nullptr))
            cairo_surface_set_mime_data(s, CAIRO_MIME_TYPE_JPEG, (unsigned char*)data, n, g_free, data);
    }
    if (s) cairo_surface_reference(s);   // yta() kan tömma sin cache – den här lever så länge programmet
    if (cache.size() > 64) { for (auto& [k, v] : cache) if (v) cairo_surface_destroy(v); cache.clear(); }
    return cache[sokvag] = s;
}

void bild(cairo_t* cr, double x, double y, double b, double h, const std::string& sokvag) {
    double skala = 1.0;
    // skala efter skärmen (HiDPI) så att målningarna blir skarpa
    double sx = 1, sy = 1;
    cairo_user_to_device_distance(cr, &sx, &sy);
    skala = std::max(1.0, std::fabs(sx));
    cairo_surface_t* s = UTSKRIFT ? utskriftsyta(sokvag) : yta(sokvag, (int)std::lround(b * skala), (int)std::lround(h * skala));
    if (!s) { rekt(cr, x, y, b, h, "#eeeeee", tema::LINJE, 1); return; }
    cairo_save(cr);
    cairo_rectangle(cr, x, y, b, h);
    cairo_clip(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, b / cairo_image_surface_get_width(s), h / cairo_image_surface_get_height(s));
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
}

void bildstorlek(const std::string& sokvag, int& b, int& h) {
    b = 16; h = 9;
    gdk_pixbuf_get_file_info(sokvag.c_str(), &b, &h);
}

// ---- figurerna ----------------------------------------------------------------------------------

namespace {

using Bygg = std::function<Figur(const Data&, const Json&)>;

Figur staende(int delar, int fyllda, const std::string& farg, double bredd, double hojd) {
    delar = std::max(1, delar); fyllda = std::clamp(fyllda, 0, delar);
    std::string f = tema::farg(farg);
    return {bredd, hojd, [=](cairo_t* cr, double x, double y) {
        double db = bredd / delar;
        for (int i = 0; i < delar; ++i) rekt(cr, x + i * db, y, db, hojd, i < fyllda ? f : tema::VIT, tema::LINJE);
    }};
}

const char* POSITIONER[] = {"tusental", "hundratal", "tiotal", "ental", "tiondelar", "hundradelar", "tusendelar"};

std::vector<std::string> utf8Tecken(const std::string& s) {
    std::vector<std::string> ut;
    for (const char* p = s.c_str(); *p; p = g_utf8_next_char(p)) ut.emplace_back(p, g_utf8_next_char(p) - p);
    return ut;
}

const std::map<std::string, Bygg>& byggare() {
    static const std::map<std::string, Bygg> B = {
        {"stapel", [](const Data&, const Json& j) {
            int delar = std::max(1, heltal(j["delar"], 1)), fyllda = std::clamp(heltal(j["fyllda"], 1), 0, delar);
            double bredd = num(j["bredd"], 420), hojd = num(j["hojd"], 40);
            std::string f = tema::farg(str(j["farg"], "gul"));
            return Figur{bredd, hojd, [=](cairo_t* cr, double x, double y) {
                double db = bredd / delar;
                for (int i = 0; i < delar; ++i) rekt(cr, x + i * db, y, db, hojd, i < fyllda ? f : tema::VIT);
                for (int i = 0; i < delar; ++i) rekt(cr, x + i * db, y, db, hojd, "", tema::LINJE);
            }};
        }},
        {"staende", [](const Data&, const Json& j) {
            return staende(heltal(j["delar"], 1), heltal(j["fyllda"], 1), str(j["farg"], "rod"), num(j["bredd"], 110), num(j["hojd"], 64));
        }},
        {"tiostaplar", [](const Data&, const Json& j) {
            return staende(10, heltal(j["fyllda"], 1), str(j["farg"], "orange"), num(j["bredd"], 110), num(j["hojd"], 64));
        }},
        {"hundraruta", [](const Data&, const Json& j) {
            int fyllda = std::clamp(heltal(j["fyllda"], 1), 0, 100);
            double ruta = num(j["ruta"], 22);
            std::string f = tema::farg(str(j["farg"], "lila"));
            return Figur{ruta * 10, ruta * 10, [=](cairo_t* cr, double x, double y) {
                for (int i = 0; i < 100; ++i) rekt(cr, x + (i % 10) * ruta, y + (i / 10) * ruta, ruta, ruta, i < fyllda ? f : tema::VIT, tema::LINJE, 1.2);
            }};
        }},
        {"positionstabell", [](const Data&, const Json& j) {
            std::string tal = str(j["tal"], "0");
            for (auto& c : tal) if (c == '.') c = ',';
            int skift = heltal(j["skift"], 0);
            double ruta = num(j["ruta"], 52);
            auto k = tal.find(',');
            std::string hel = k == std::string::npos ? tal : tal.substr(0, k), dec = k == std::string::npos ? "" : tal.substr(k + 1);
            std::vector<std::string> siffror = utf8Tecken(hel + dec);
            int pos0 = 4 - (int)hel.size();
            double topp = 96, mellan = 34;
            int rader = skift ? 2 : 1;
            double hojd = topp + rader * ruta + (skift ? mellan : 0) + 8;
            return Figur{ruta * 7, hojd, [=](cairo_t* cr, double x, double y) {
                auto satt = [&](double yy, int start, size_t kommaEfter, const std::vector<std::string>& sf) {
                    for (size_t i = 0; i < sf.size(); ++i) {
                        int kol = start + (int)i;
                        if (kol < 0 || kol >= 7) continue;
                        text(cr, x + kol * ruta + ruta / 2, yy + ruta * 0.72, sf[i], 30, true, nullptr, 'm');
                        if (i + 1 == kommaEfter && i + 1 < sf.size()) text(cr, x + (kol + 1) * ruta - 3, yy + ruta * 0.72, ",", 30, true, nullptr, 'm');
                    }
                };
                for (int c = 0; c < 7; ++c) text(cr, x + c * ruta + ruta / 2 + 5, y + topp - 8, POSITIONER[c], 12, false, nullptr, 's', true);
                double y1 = y + topp, y2 = y1 + ruta + mellan;
                for (double yy : skift ? std::vector<double>{y1, y2} : std::vector<double>{y1}) {
                    for (int c = 0; c < 7; ++c) rekt(cr, x + c * ruta, yy, ruta, ruta, tema::VIT, tema::LINJE);
                    linje(cr, x + 4 * ruta, yy, x + 4 * ruta, yy + ruta, tema::LINJE, 3);
                }
                satt(y1, pos0, hel.size(), siffror);
                if (skift) {
                    int start2 = pos0 - skift;
                    auto sf2 = siffror;
                    for (int n = 0; n < std::max(0, skift - (int)dec.size()); ++n) sf2.push_back("0");
                    satt(y2, start2, hel.size() + skift, sf2);
                    for (size_t i = 0; i < siffror.size(); ++i) {
                        int k1 = pos0 + (int)i, k2 = start2 + (int)i;
                        if (k1 >= 0 && k1 < 7 && k2 >= 0 && k2 < 7) pil(cr, x + k1 * ruta + ruta / 2, y1 + ruta + 4, x + k2 * ruta + ruta / 2, y2 - 4);
                    }
                }
            }};
        }},
        {"tallinje", [](const Data&, const Json& j) {
            Rutnat r(j);
            int etikett = std::max(1, heltal(j["etikett"], 1));
            std::vector<double> markera;
            for (auto& m : j["markera"].element()) markera.push_back(num(m, 0));
            int n = std::max(1, (int)std::lround((r.till - r.fran) / r.steg));
            double bredd = r.bredd;
            return Figur{bredd, 50, [=](cairo_t* cr, double x, double y) {
                double y0 = y + 24, x0 = x + 18, b = bredd - 36;
                linje(cr, x0, y0, x0 + b, y0, tema::LINJE, 2);
                for (int i = 0; i <= n; ++i) {
                    double xx = x0 + i * b / n;
                    bool stor = i % etikett == 0;
                    linje(cr, xx, y0 - (stor ? 7 : 4), xx, y0 + (stor ? 7 : 4), tema::LINJE, stor ? 2 : 1.2);
                    if (stor) text(cr, xx, y0 + 22, talText(r.fran + i * r.steg), 12, false, nullptr, 'm');
                }
                for (double v : markera) {
                    double xx = x0 + (v - r.fran) / (r.till - r.fran) * b;
                    rekt(cr, xx - 5, y0 - 5, 10, 10, tema::ROD);
                }
            }};
        }},
        {"brak", [](const Data&, const Json& j) {
            std::string t = str(j["taljare"], "1"), n = str(j["namnare"], "2");
            double s = num(j["storlek"], 26);
            double b = std::max(g_utf8_strlen(t.c_str(), -1), g_utf8_strlen(n.c_str(), -1)) * s * 0.62 + 10, h = s * 2.4;
            return Figur{std::floor(b), std::floor(h), [=](cairo_t* cr, double x, double y) {
                text(cr, x + b / 2, y + s * 0.95, t, s, false, nullptr, 'm');
                linje(cr, x + 2, y + s * 1.15, x + b - 2, y + s * 1.15, tema::TEXT, 2);
                text(cr, x + b / 2, y + s * 2.15, n, s, false, nullptr, 'm');
            }};
        }},
        {"rektangel", [](const Data&, const Json& j) {
            int b = heltal(j["bredd"], 5), h = heltal(j["hojd"], 3);
            double ruta = num(j["ruta"], 30);
            bool rutnat = sant(j["rutnat"], true), matt = sant(j["matt"], true);
            std::string f = tema::farg(str(j["farg"], "gron"));
            double marg = matt ? 46 : 2;
            return Figur{marg + b * ruta + 6, h * ruta + (matt ? 28 : 10), [=](cairo_t* cr, double x, double y) {
                rekt(cr, x + marg, y + 4, b * ruta, h * ruta, f);
                if (rutnat) {
                    for (int i = 0; i <= b; ++i) linje(cr, x + marg + i * ruta, y + 4, x + marg + i * ruta, y + 4 + h * ruta, tema::LINJE, 1);
                    for (int k = 0; k <= h; ++k) linje(cr, x + marg, y + 4 + k * ruta, x + marg + b * ruta, y + 4 + k * ruta, tema::LINJE, 1);
                }
                rekt(cr, x + marg, y + 4, b * ruta, h * ruta, "", tema::LINJE, 2);
                if (matt) {
                    text(cr, x + marg + b * ruta / 2, y + 4 + h * ruta + 18, std::to_string(b) + " cm", 13, false, nullptr, 'm');
                    text(cr, x + marg - 8, y + 4 + h * ruta / 2 + 5, std::to_string(h) + " cm", 13, false, nullptr, 'e');
                }
            }};
        }},
        {"triangel", [](const Data&, const Json& j) {
            int b = heltal(j["bas"], 6), h = heltal(j["hojd"], 4);
            double ruta = num(j["ruta"], 28), marg = 4, topp = b * ruta * 0.35;
            std::string f = tema::farg(str(j["farg"], "rosa"));
            return Figur{b * ruta + marg * 2 + 80, h * ruta + 30, [=](cairo_t* cr, double x, double y) {
                double x0 = x + marg, y0 = y + 6 + h * ruta;
                cairo_move_to(cr, x0, y0); cairo_line_to(cr, x0 + b * ruta, y0); cairo_line_to(cr, x0 + topp, y0 - h * ruta);
                cairo_close_path(cr);
                tema::satt(cr, f); cairo_fill_preserve(cr);
                tema::satt(cr, tema::LINJE); cairo_set_line_width(cr, 2); cairo_stroke(cr);
                for (double yy = 0; yy < h * ruta; yy += 8) linje(cr, x0 + topp, y0 - yy, x0 + topp, y0 - std::min(yy + 4, h * ruta), tema::ROD, 1.5);
                text(cr, x0 + b * ruta / 2, y0 + 18, "bas " + std::to_string(b) + " cm", 13, false, nullptr, 'm');
                text(cr, x0 + topp + 6, y0 - h * ruta / 2, "höjd " + std::to_string(h) + " cm", 13, false, tema::ROD);
            }};
        }},
        {"stapeldiagram", [](const Data&, const Json& j) {
            std::vector<double> v;
            for (auto& e : j["varden"].element()) v.push_back(num(e, 0));
            if (v.empty()) v = {3, 5, 2};
            std::vector<std::string> namn = j["namn"].strangar();
            while (namn.size() < v.size()) namn.push_back(std::to_string(namn.size() + 1));
            std::string f = tema::farg(str(j["farg"], "himmel")), rubrik = str(j["rubrik"], "");
            double bredd = num(j["bredd"], 360), hojd = num(j["hojd"], 180);
            double maxv = *std::max_element(v.begin(), v.end());
            int steg = maxv <= 10 ? 1 : maxv <= 20 ? 2 : maxv <= 50 ? 5 : 10;
            int topp = (int)std::ceil(maxv / steg) * steg;
            if (topp == 0) topp = steg;
            double vm = 36, nm = 30, htopp = rubrik.empty() ? 6 : 22;
            return Figur{bredd + 4, hojd, [=](cairo_t* cr, double x, double y) {
                double y0 = y + hojd - nm, yt = y + htopp;
                linje(cr, x + vm, yt, x + vm, y0, tema::LINJE, 1.5);
                linje(cr, x + vm, y0, x + bredd, y0, tema::LINJE, 1.5);
                for (int t = 0; t <= topp; t += steg) {
                    double yy = y0 - (y0 - yt) * t / topp;
                    linje(cr, x + vm - 4, yy, x + bredd, yy, t ? "#d8d8d8" : tema::LINJE, 1);
                    text(cr, x + vm - 8, yy + 4, std::to_string(t), 11, false, nullptr, 'e');
                }
                double bb = (bredd - vm) / v.size();
                for (size_t i = 0; i < v.size(); ++i) {
                    double hh = (y0 - yt) * v[i] / topp;
                    rekt(cr, x + vm + i * bb + bb * 0.2, y0 - hh, bb * 0.6, hh, f, tema::LINJE);
                    text(cr, x + vm + i * bb + bb / 2, y0 + 16, namn[i], 12, false, nullptr, 'm');
                }
                if (!rubrik.empty()) text(cr, x + vm + (bredd - vm) / 2, y + 14, rubrik, 13, true, nullptr, 'm');
            }};
        }},
        {"koordinatsystem", [](const Data&, const Json& j) {
            Rutnat r(j);
            int kol = r.xmax - r.xmin, rad = r.ymax - r.ymin;
            std::vector<std::pair<double, double>> punkter;
            for (auto& p : j["punkter"].element()) punkter.push_back({num(p[0], 0), num(p[1], 0)});
            std::vector<std::pair<double, double>> linjer;
            if (j["linje"].arLista()) linjer.push_back({num(j["linje"][0], 1), num(j["linje"][1], 0)});
            for (auto& l : j["linjer"].element()) linjer.push_back({num(l[0], 1), num(l[1], 0)});
            std::vector<Polynom> funk;
            for (auto& f : j["funktioner"].element()) try { funk.push_back(tolkaUttryck(f.str())); } catch (...) {}
            return Figur{(double)r.marg + kol * r.ruta + 16, (double)rad * r.ruta + 30, [=](cairo_t* cr, double x, double y) {
                auto px = [&](double vx) { return r.pixel(vx, 0).first; };
                auto py = [&](double vy) { return r.pixel(0, vy).second; };
                for (int i = 0; i <= kol; ++i) linje(cr, x + px(r.xmin + i), y + py(r.ymax), x + px(r.xmin + i), y + py(r.ymin), "#d8d8d8", 1);
                for (int k = 0; k <= rad; ++k) linje(cr, x + px(r.xmin), y + py(r.ymin + k), x + px(r.xmax), y + py(r.ymin + k), "#d8d8d8", 1);
                double ay = (r.ymin <= 0 && 0 <= r.ymax) ? py(0) : py(r.ymin);
                double ax = (r.xmin <= 0 && 0 <= r.xmax) ? px(0) : px(r.xmin);
                linje(cr, x + px(r.xmin), y + ay, x + px(r.xmax), y + ay, tema::LINJE, 2);
                linje(cr, x + ax, y + py(r.ymax), x + ax, y + py(r.ymin), tema::LINJE, 2);
                for (int i = 0; i <= kol; ++i) if (r.xmin + i) text(cr, x + px(r.xmin + i), y + ay + 16, talText(r.xmin + i), 11, false, nullptr, 'm');
                for (int k = 0; k <= rad; ++k) if (r.ymin + k) text(cr, x + ax - 6, y + py(r.ymin + k) + 4, talText(r.ymin + k), 11, false, nullptr, 'e');
                text(cr, x + px(r.xmax) - 2, y + ay - 6, "x", 12, false, nullptr, 'e');
                text(cr, x + ax + 8, y + py(r.ymax) + 10, "y", 12);
                const char* farger[] = {tema::ACCENT, "#78b2dd", "#4f9a55"};
                cairo_save(cr);
                cairo_rectangle(cr, x + px(r.xmin), y + py(r.ymax), (r.xmax - r.xmin) * r.ruta, (r.ymax - r.ymin) * r.ruta);
                cairo_clip(cr);
                for (size_t n = 0; n < funk.size(); ++n) {
                    int steg = 60 * kol;
                    tema::satt(cr, farger[n % 3]); cairo_set_line_width(cr, 2.5);
                    bool ny = true;
                    for (int i = 0; i <= steg; ++i) {
                        double vx = r.xmin + (double)kol * i / steg, vy = varde(funk[n], {{"x", vx}});
                        if (vy < r.ymin - 2 || vy > r.ymax + 2) { ny = true; continue; }
                        if (ny) cairo_move_to(cr, x + px(vx), y + py(vy)); else cairo_line_to(cr, x + px(vx), y + py(vy));
                        ny = false;
                    }
                    cairo_stroke(cr);
                }
                for (size_t n = 0; n < linjer.size(); ++n) {
                    auto [k, m] = linjer[n];
                    linje(cr, x + px(r.xmin), y + py(k * r.xmin + m), x + px(r.xmax), y + py(k * r.xmax + m), farger[n % 3], 2.5);
                }
                cairo_restore(cr);
                for (auto [vx, vy] : punkter) rekt(cr, x + px(vx) - 5, y + py(vy) - 5, 10, 10, tema::ROD);
            }};
        }},
        {"tiobas", [](const Data&, const Json& j) {
            int t = heltal(j["tiotal"], 2), e = heltal(j["ental"], 3);
            double ruta = num(j["ruta"], 14);
            std::string f = tema::farg(str(j["farg"], "orange"));
            double bredd = t * (ruta + 8) + (e ? e * (ruta + 4) : 0) + 8;
            return Figur{std::max(bredd, 5 * (ruta + 4) + t * (ruta + 8) + 14), 10 * ruta + 10, [=](cairo_t* cr, double x, double y) {
                double xx = x + 4;
                for (int s = 0; s < t; ++s) {
                    for (int k = 0; k < 10; ++k) rekt(cr, xx, y + 4 + k * ruta, ruta, ruta, f, tema::LINJE, 1);
                    xx += ruta + 8;
                }
                xx += 6;
                for (int i = 0; i < e; ++i) rekt(cr, xx + (i % 5) * (ruta + 4), y + 4 + 9 * ruta - (i / 5) * (ruta + 4), ruta, ruta, f, tema::LINJE, 1);
            }};
        }},
        {"prickar", [](const Data&, const Json& j) {
            int n = heltal(j["antal"], 7), pr = std::max(1, heltal(j["per_rad"], 5));
            double d = num(j["d"], 26);
            std::string f = tema::farg(str(j["farg"], "himmel"));
            int rader = std::max(1, (n + pr - 1) / pr);
            return Figur{pr * (d + 6) + 8, rader * (d + 6) + 8, [=](cairo_t* cr, double x, double y) {
                for (int i = 0; i < n; ++i) rekt(cr, x + 4 + (i % pr) * (d + 6), y + 4 + (i / pr) * (d + 6), d, d, f, tema::LINJE, 1);
            }};
        }},
        {"talkamrater", [](const Data&, const Json& j) {
            int n = heltal(j["summa"], 10), a = heltal(j["del1"], 4);
            double d = num(j["d"], 26);
            bool visa = sant(j["visa"], true);
            std::string f1 = tema::farg(str(j["farg1"], "gul")), f2 = tema::farg(str(j["farg2"], "himmel"));
            return Figur{n * (d + 4) + (visa ? 130 : 8), d + 10, [=](cairo_t* cr, double x, double y) {
                for (int i = 0; i < n; ++i) rekt(cr, x + 4 + i * (d + 4), y + 4, d, d, i < a ? f1 : f2, tema::LINJE, 1);
                if (visa) text(cr, x + 4 + n * (d + 4) + 6, y + 4 + d * 0.75, std::to_string(a) + " + " + std::to_string(n - a) + " = " + std::to_string(n), 16);
            }};
        }},
        {"klocka", [](const Data&, const Json& j) {
            int h = ((heltal(j["timme"], 3) % 12) + 12) % 12, m = ((heltal(j["minut"], 0) % 60) + 60) % 60;
            double r = num(j["r"], 56);
            return Figur{2 * r + 8, 2 * r + 8, [=](cairo_t* cr, double x, double y) {
                double cx = x + r + 4, cy = y + r + 4;
                cairo_arc(cr, cx, cy, r, 0, 2 * M_PI); tema::satt(cr, tema::VIT); cairo_fill(cr);
                for (int i = 0; i < 60; ++i) {
                    double v = (i * 6 - 90) * M_PI / 180, L = i % 5 == 0 ? 8 : 4;
                    linje(cr, cx + (r - L) * std::cos(v), cy + (r - L) * std::sin(v), cx + r * std::cos(v), cy + r * std::sin(v), tema::LINJE, i % 5 == 0 ? 2 : 1);
                }
                for (int i = 1; i <= 12; ++i) {
                    double v = (i * 30 - 90) * M_PI / 180;
                    text(cr, cx + (r - 20) * std::cos(v), cy + (r - 20) * std::sin(v) + 5, std::to_string(i), 13, false, nullptr, 'm');
                }
                double vh = ((h + m / 60.0) * 30 - 90) * M_PI / 180, vm = (m * 6 - 90) * M_PI / 180;
                linje(cr, cx, cy, cx + r * 0.5 * std::cos(vh), cy + r * 0.5 * std::sin(vh), tema::TEXT, 4);
                linje(cr, cx, cy, cx + r * 0.78 * std::cos(vm), cy + r * 0.78 * std::sin(vm), tema::ROD, 2.5);
            }};
        }},
        {"tabell", [](const Data&, const Json& j) {
            std::vector<std::string> rub = j["rubriker"].strangar();
            std::vector<std::vector<std::string>> alla;
            if (!rub.empty()) alla.push_back(rub);
            for (auto& r : j["rader"].element()) alla.push_back(r.strangar());
            size_t kol = rub.empty() ? (alla.empty() ? 1 : alla[0].size()) : rub.size();
            double kb = num(j["kolbredd"], 90), rh = num(j["radhojd"], 28);
            bool harRub = !rub.empty();
            return Figur{kol * kb + 2, alla.size() * rh + 2, [=](cairo_t* cr, double x, double y) {
                for (size_t i = 0; i < alla.size(); ++i)
                    for (size_t k = 0; k < alla[i].size(); ++k) {
                        bool rubrik = harRub && i == 0;
                        rekt(cr, x + k * kb, y + i * rh, kb, rh, rubrik ? tema::ACCENT_SVAG : tema::VIT, tema::LINJE, 1);
                        text(cr, x + k * kb + kb / 2, y + i * rh + rh * 0.68, alla[i][k], 13, rubrik, nullptr, 'm');
                    }
            }};
        }},
        {"ikoner", [](const Data& d, const Json& j) {
            int n = heltal(j["antal"], 5), pr = std::max(1, heltal(j["per_rad"], 5));
            double sz = num(j["d"], 48);
            std::string p = d.ikon(str(j["namn"], "apple"));
            int rader = std::max(1, (n + pr - 1) / pr);
            return Figur{std::min(n, pr) * (sz + 6) + 8, rader * (sz + 6) + 8, [=](cairo_t* cr, double x, double y) {
                for (int i = 0; i < n; ++i) bild(cr, x + 4 + (i % pr) * (sz + 6), y + 4 + (i / pr) * (sz + 6), sz, sz, p);
            }};
        }},
        {"scen", [](const Data& d, const Json& j) {
            std::string p = d.bild(str(j["bild"], ""));
            double bredd = num(j["bredd"], 560);
            int bb = 16, hh = 9;
            bildstorlek(p, bb, hh);
            double hojd = std::floor(bredd * hh / bb);
            return Figur{bredd, hojd, [=](cairo_t* cr, double x, double y) { bild(cr, x, y, bredd, hojd, p); }};
        }},
        {"ikonval", [](const Data& d, const Json& j) {
            std::vector<std::string> val = j["ikoner"].strangar(), fore = j["fore"].strangar();
            int fa = heltal(j["fore_antal"], 0);
            std::string fn = str(j["fore_namn"], "");
            for (int i = 0; i < fa; ++i) fore.push_back(fn);
            bool fragetecken = fa == 0;
            double sz = num(j["d"], 76), fd = num(j["fore_d"], 56);
            double topp = fore.empty() ? 0 : fd + 26;
            double bredd = std::max(val.size() * (sz + 16), fore.size() * (fd + 8) + (fragetecken ? 60 : 0)) + 8;
            std::vector<std::string> pv, pf;
            for (auto& n : val) pv.push_back(d.ikon(n));
            for (auto& n : fore) pf.push_back(d.ikon(n));
            std::string fr = d.ikon("fragetecken");
            return Figur{bredd, topp + sz + 20, [=](cairo_t* cr, double x, double y) {
                for (size_t i = 0; i < pf.size(); ++i) bild(cr, x + 4 + i * (fd + 8), y + 4, fd, fd, pf[i]);
                if (!pf.empty() && fragetecken) {
                    bild(cr, x + 4 + pf.size() * (fd + 8), y + 4, fd, fd, fr);
                    linje(cr, x, y + fd + 14, x + bredd, y + fd + 14, "#d8d8d8", 1);
                }
                for (size_t i = 0; i < pv.size(); ++i) {
                    if (sz > 2) rekt(cr, x + 4 + i * (sz + 16), y + topp + 4, sz + 8, sz + 8, tema::VIT, "#d8d8d8", 1);
                    bild(cr, x + 8 + i * (sz + 16), y + topp + 8, sz, sz, pv[i]);
                }
            }};
        }},
        {"grupper", [](const Data& d, const Json& j) {
            std::vector<int> antal;
            for (auto& a : j["antal"].element()) antal.push_back(heltal(a, 0));
            if (antal.empty()) antal = {3, 5};
            double sz = num(j["d"], 40);
            int pr = std::max(1, heltal(j["per_rad"], 3));
            std::string p = d.ikon(str(j["namn"], "apple"));
            double gb = pr * (sz + 4) + 16;
            int rader = 1;
            for (int a : antal) rader = std::max(rader, (a + pr - 1) / pr);
            double gh = rader * (sz + 4) + 16;
            return Figur{antal.size() * (gb + 20) + 8, gh + 8, [=](cairo_t* cr, double x, double y) {
                for (size_t g = 0; g < antal.size(); ++g) {
                    double gx = x + 4 + g * (gb + 20);
                    rekt(cr, gx, y + 4, gb, gh, tema::VIT, "#d8d8d8", 1);
                    for (int i = 0; i < antal[g]; ++i) bild(cr, gx + 8 + (i % pr) * (sz + 4), y + 12 + (i / pr) * (sz + 4), sz, sz, p);
                }
            }};
        }},
        {"tioram", [](const Data&, const Json& j) {
            int n1 = heltal(j["fyllda"], 0), n2 = heltal(j["extra"], 0);
            double ruta = num(j["ruta"], 52);
            std::string f1 = tema::farg(str(j["farg"], "#2f5fb3")), f2 = tema::farg(str(j["farg2"], "#f2c230"));
            return Figur{5 * ruta + 12, 2 * ruta + 12, [=](cairo_t* cr, double x, double y) {
                rekt(cr, x + 2, y + 2, 5 * ruta + 8, 2 * ruta + 8, "#e9c99a", "#9c7444", 2);
                for (int i = 0; i < 10; ++i) {
                    double cx = x + 6 + (i % 5) * ruta, cy = y + 6 + (i / 5) * ruta;
                    rekt(cr, cx, cy, ruta, ruta, "#f6e3c4", "#9c7444", 1.5);
                    if (i < n1 + n2) bricka(cr, cx + ruta / 2, cy + ruta / 2, ruta * 0.38, i < n1 ? f1 : f2);
                }
            }};
        }},
        {"malrutor", [](const Data& d, const Json& j) {
            // Rutor att måla eller kryssa: tal, ord, former ("form:triangel"), ikoner ("ikon:apple")
            std::vector<std::string> obj = j["objekt"].arText() ? std::vector<std::string>{} : j["objekt"].strangar();
            if (j["objekt"].arText()) { std::string s = j["objekt"].str(), del; std::stringstream in(s); while (std::getline(in, del, '|')) obj.push_back(trim(del)); }
            std::vector<std::string> malade = j["malade"].strangar();
            int k = std::max(1, heltal(j["kolumner"], 6));
            double ruta = num(j["ruta"], 64);
            int rader = std::max(1, (int)((obj.size() + k - 1) / k));
            const Data* dp = &d;
            return Figur{std::min((int)obj.size(), k) * ruta + 8, rader * ruta + 8, [=](cairo_t* cr, double x, double y) {
                for (size_t i = 0; i < obj.size(); ++i) {
                    double cx = x + 4 + (i % k) * ruta, cy = y + 4 + (i / k) * ruta;
                    std::string f = i < malade.size() ? malade[i] : "";
                    bool form = obj[i].rfind("form:", 0) == 0;
                    rekt(cr, cx, cy, ruta - 6, ruta - 6, !f.empty() && !form ? pennfarg(f) : tema::VIT, "#c8ced6", 1.2);
                    sak(cr, *dp, cx, cy, ruta - 6, ruta - 6, obj[i], form ? f : "");
                }
            }};
        }},
        {"rutpapper", [](const Data&, const Json& j) {
            int b = heltal(j["bredd"], 16), h = heltal(j["hojd"], 10);
            double r = num(j["ruta"], 28);
            return Figur{b * r + 1, h * r + 1, [=](cairo_t* cr, double x, double y) {
                for (int i = 0; i <= b; ++i) linje(cr, x + i * r + 0.5, y, x + i * r + 0.5, y + h * r, "#d6dde8", 1);
                for (int i = 0; i <= h; ++i) linje(cr, x, y + i * r + 0.5, x + b * r, y + i * r + 0.5, "#d6dde8", 1);
            }};
        }},
    };
    return B;
}

}  // namespace

std::string pennfarg(const std::string& namn) {
    static const std::map<std::string, std::string> P = {
        {"röd", "#e5484d"}, {"blå", "#3e7be0"}, {"gul", "#f5c518"}, {"grön", "#3fae57"}, {"orange", "#f28c28"},
        {"lila", "#8e5bd0"}, {"svart", "#2b2f33"}, {"brun", "#8a5a3b"}, {"rosa", "#f08cb8"}};
    auto f = P.find(namn);
    return f != P.end() ? f->second : tema::farg(namn);
}

// En form som väg (centrerad i rutan, ~80 %) – samma namn som lerikonerna och korten använder
static bool formvag(cairo_t* cr, const std::string& namn, double x, double y, double b, double h) {
    double s = std::min(b, h) * 0.8, cx = x + b / 2, cy = y + h / 2, r = s / 2;
    auto hörn = [&](int n, double rad, double vrid) {
        for (int i = 0; i < n; ++i) {
            double v = vrid + i * 2 * M_PI / n;
            i ? cairo_line_to(cr, cx + rad * std::cos(v), cy + rad * std::sin(v)) : cairo_move_to(cr, cx + rad * std::cos(v), cy + rad * std::sin(v));
        }
        cairo_close_path(cr);
    };
    if (namn == "cirkel") cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    else if (namn == "kvadrat") cairo_rectangle(cr, cx - r * 0.85, cy - r * 0.85, r * 1.7, r * 1.7);
    else if (namn == "rektangel") cairo_rectangle(cr, cx - r, cy - r * 0.55, 2 * r, r * 1.1);
    else if (namn == "triangel") {
        cairo_move_to(cr, cx, cy - r); cairo_line_to(cr, cx + r, cy + r * 0.8); cairo_line_to(cr, cx - r, cy + r * 0.8); cairo_close_path(cr);
    } else if (namn == "romb") {
        cairo_move_to(cr, cx, cy - r); cairo_line_to(cr, cx + r * 0.7, cy); cairo_line_to(cr, cx, cy + r); cairo_line_to(cr, cx - r * 0.7, cy); cairo_close_path(cr);
    } else if (namn == "femhorning") hörn(5, r, -M_PI / 2);
    else if (namn == "sexhorning") hörn(6, r, 0);
    else if (namn == "oval") { cairo_save(cr); cairo_translate(cr, cx, cy); cairo_scale(cr, r, r * 0.62); cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI); cairo_restore(cr); }
    else if (namn == "halvcirkel") { cairo_arc(cr, cx, cy + r * 0.4, r, M_PI, 2 * M_PI); cairo_close_path(cr); }
    else if (namn == "stjarna") {
        for (int i = 0; i < 10; ++i) {
            double v = -M_PI / 2 + i * M_PI / 5, rr = i % 2 ? r * 0.45 : r;
            i ? cairo_line_to(cr, cx + rr * std::cos(v), cy + rr * std::sin(v)) : cairo_move_to(cr, cx + rr * std::cos(v), cy + rr * std::sin(v));
        }
        cairo_close_path(cr);
    } else return false;
    return true;
}

void sak(cairo_t* cr, const Data& d, double x, double y, double b, double h, const std::string& s, const std::string& fyll) {
    cairo_new_path(cr);
    if (s.rfind("form:", 0) == 0) {
        if (!formvag(cr, s.substr(5), x, y, b, h)) { text(cr, x + b / 2, y + h / 2 + 6, s.substr(5), 16, false, nullptr, 'm'); return; }
        tema::satt(cr, fyll.empty() ? tema::VIT : pennfarg(fyll)); cairo_fill_preserve(cr);
        tema::satt(cr, tema::LINJE); cairo_set_line_width(cr, 2.5); cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND); cairo_stroke(cr);
        return;
    }
    if (!fyll.empty()) rekt(cr, x, y, b, h, pennfarg(fyll));
    if (s.rfind("ikon:", 0) == 0) {
        double sz = std::min(b, h) * 0.8;
        bild(cr, x + (b - sz) / 2, y + (h - sz) / 2, sz, sz, d.ikon(s.substr(5)));
        return;
    }
    if (s.rfind("prickar:", 0) == 0) {    // som på en tärning upp till sex, sedan rader om fem
        int n = std::max(0, atoi(s.c_str() + 8));
        static const double T[7][6][2] = {{}, {{.5, .5}}, {{.25, .25}, {.75, .75}}, {{.25, .25}, {.5, .5}, {.75, .75}},
            {{.25, .25}, {.75, .25}, {.25, .75}, {.75, .75}}, {{.25, .25}, {.75, .25}, {.5, .5}, {.25, .75}, {.75, .75}},
            {{.25, .22}, {.75, .22}, {.25, .5}, {.75, .5}, {.25, .78}, {.75, .78}}};
        double sz = std::min(b, h);
        double ox = x + (b - sz) / 2, oy = y + (h - sz) / 2;
        for (int i = 0; i < n; ++i) {
            double px, py, rr;
            if (n <= 6) { px = ox + T[n][i][0] * sz; py = oy + T[n][i][1] * sz; rr = sz * 0.09; }
            else { int k = std::min(n, 5); px = ox + sz * (0.12 + 0.76 * ((i % 5) + 0.5) / k); py = oy + sz * (0.2 + 0.2 * (i / 5)); rr = sz * 0.06; }
            cairo_new_sub_path(cr);
            cairo_arc(cr, px, py, rr, 0, 2 * M_PI);
        }
        tema::satt(cr, tema::LINJE); cairo_fill(cr);
        return;
    }
    // text: så stor som får plats
    double storlek = std::min(h * 0.42, b / std::max(1.0, g_utf8_strlen(s.c_str(), -1) * 0.62));
    text(cr, x + b / 2, y + h / 2 + storlek * 0.36, s, std::max(10.0, storlek), true, nullptr, 'm');
}

bool figurFinns(const std::string& typ) { return byggare().count(typ) > 0; }

Figur figur(const Data& d, const Json& j) {
    std::string typ = j["typ"].str();
    auto f = byggare().find(typ);
    if (f == byggare().end()) throw std::runtime_error("Okänd figur: " + typ);
    return f->second(d, j);
}

Json medFyllda(const Json& j, int fyllda, const std::string& nyckel) {
    Json r = j;
    r.satt(nyckel, fyllda);
    return r;
}

std::vector<Ruta> celler(const Data& d, const Json& j) {
    std::string typ = j["typ"].str();
    std::vector<Ruta> ut;
    if (!figurFinns(typ)) return ut;
    Figur f = figur(d, j);
    if (typ == "stapel" || typ == "staende" || typ == "tiostaplar") {
        int delar = std::max(1, heltal(j["delar"], typ == "tiostaplar" ? 10 : 1));
        double db = f.bredd / delar;
        for (int i = 0; i < delar; ++i) ut.push_back({i * db, 0, db, f.hojd});
    } else if (typ == "hundraruta") {
        double r = num(j["ruta"], 22);
        for (int i = 0; i < 100; ++i) ut.push_back({(i % 10) * r, (i / 10) * r, r, r});
    } else if (typ == "prickar" || typ == "ikoner") {
        int n = heltal(j["antal"], typ == "prickar" ? 7 : 5), pr = std::max(1, heltal(j["per_rad"], 5));
        double sz = num(j["d"], typ == "prickar" ? 26 : 48);
        for (int i = 0; i < n; ++i) ut.push_back({4 + (i % pr) * (sz + 6), 4 + (i / pr) * (sz + 6), sz, sz});
    } else if (typ == "scen") {
        for (auto& o : j["omraden"].element())
            ut.push_back({num(o[0], 0) * f.bredd, num(o[1], 0) * f.hojd, num(o[2], 0) * f.bredd, num(o[3], 0) * f.hojd});
    } else if (typ == "ikonval") {
        double sz = num(j["d"], 76), fd = num(j["fore_d"], 56);
        double topp = (j["fore"].storlek() || heltal(j["fore_antal"], 0)) ? fd + 26 : 0;
        for (size_t i = 0; i < j["ikoner"].storlek(); ++i) ut.push_back({4 + i * (sz + 16), topp + 4, sz + 8, sz + 8});
    } else if (typ == "grupper") {
        double sz = num(j["d"], 40);
        int pr = std::max(1, heltal(j["per_rad"], 3));
        double gb = pr * (sz + 4) + 16;
        size_t n = j["antal"].storlek() ? j["antal"].storlek() : 2;
        for (size_t g = 0; g < n; ++g) ut.push_back({4 + g * (gb + 20), 4, gb, f.hojd - 8});
    } else if (typ == "tioram") {
        double r = num(j["ruta"], 52);
        for (int i = 0; i < 10; ++i) ut.push_back({6 + (i % 5) * r, 6 + (i / 5) * r, r, r});
    } else if (typ == "malrutor") {
        std::string o = j["objekt"].arText() ? j["objekt"].str() : "";
        size_t n = j["objekt"].arText() ? 1 + std::count(o.begin(), o.end(), '|') : j["objekt"].storlek();
        int k = std::max(1, heltal(j["kolumner"], 6));
        double r = num(j["ruta"], 64);
        for (size_t i = 0; i < n; ++i) ut.push_back({4 + (i % k) * r, 4 + (i / k) * r, r - 6, r - 6});
    }
    return ut;
}

std::vector<std::string> cellnamn(const Data& d, const Json& j) {
    size_t n = celler(d, j).size();
    std::string typ = j["typ"].str();
    std::vector<std::string> namn;
    if (typ == "grupper") namn = {"första", "andra", "tredje", "fjärde"};
    else {
        namn = j["namn"].strangar();
        if (namn.empty() && typ == "ikonval") namn = j["ikoner"].strangar();
    }
    std::vector<std::string> ut;
    for (size_t i = 0; i < n; ++i) ut.push_back(i < namn.size() ? namn[i] : std::to_string(i + 1));
    return ut;
}

Rutnat::Rutnat(const Json& j) {
    typ = j["typ"].str();
    if (typ == "tallinje") {
        fran = num(j["fran"], 0); till = num(j["till"], 10); steg = num(j["steg"], 1); bredd = num(j["bredd"], 420);
        if (steg <= 0) steg = 1;
        if (till <= fran) till = fran + 1;
    } else if (typ == "koordinatsystem") {
        xmin = heltal(j["xmin"], 0); xmax = heltal(j["xmax"], 6); ymin = heltal(j["ymin"], 0); ymax = heltal(j["ymax"], 6);
        ruta = heltal(j["ruta"], 32);
    }
}

std::pair<double, double> Rutnat::snapp(double x, double y) const {
    if (typ == "tallinje") {
        double b = bredd - 36;
        int n = (int)std::lround((till - fran) / steg);
        int i = std::clamp((int)std::lround((x - 18) / b * n), 0, n);
        return {fran + i * steg, 0};
    }
    int vx = (int)std::lround((x - marg) / ruta) + xmin, vy = ymax - (int)std::lround((y - 6) / ruta);
    return {std::clamp(vx, xmin, xmax), std::clamp(vy, ymin, ymax)};
}

std::pair<double, double> Rutnat::pixel(double vx, double vy) const {
    if (typ == "tallinje") return {18 + (vx - fran) / (till - fran) * (bredd - 36), 24};
    return {marg + (vx - xmin) * ruta, 6 + (ymax - vy) * ruta};
}

}  // namespace matte
