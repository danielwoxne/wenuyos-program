// Arbetsblad och regelbok som PDF. Sidorna byggs av block (en uppgift, en rad i kortet …) som
// hålls ihop på samma sida när de får plats. Figurerna ritas med samma kod som på skärmen (Rita.cpp),
// målningarna bäddas in som JPEG i full upplösning.
#include "Utskrift.h"

#include "Bedom.h"
#include "Rita.h"
#include "Tal.h"

#include <cairo-pdf.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <random>
#include <set>

namespace matte {

namespace {

// ---- språk: korten är svenska, rubrikerna följer datorns språk ------------------------------------
bool svenska() {
    const char* const* l = g_get_language_names();
    return l && l[0] && g_str_has_prefix(l[0], "sv");
}
const char* T(const char* en, const char* sv) { return svenska() ? sv : en; }

// ---- A4 i punkter ---------------------------------------------------------------------------------
const double SB = 595.28, SH = 841.89, M = 48, BR = SB - 2 * M, BOTTEN = SH - M - 14;
const char* FONT = "Inter, Cantarell, DejaVu Sans, sans";
double LATTLAST_SKALA = 1.0;   // 1.25 i lättläst

struct Stil {
    double storlek = 11;
    bool fet = false, kursiv = false;
    const char* farg = nullptr;
    bool mitten = false;
};

// Lättläst: större text, luft mellan bokstäverna och raderna
PangoLayout* layout(cairo_t* cr, const Stil& s) {
    PangoLayout* l = pango_cairo_create_layout(cr);
    PangoFontDescription* fd = pango_font_description_from_string(FONT);
    pango_font_description_set_absolute_size(fd, s.storlek * LATTLAST_SKALA * PANGO_SCALE);
    pango_font_description_set_weight(fd, s.fet ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL);
    if (s.kursiv && LATTLAST_SKALA == 1.0) pango_font_description_set_style(fd, PANGO_STYLE_ITALIC);   // kursiv är svårläst
    pango_layout_set_font_description(l, fd);
    pango_font_description_free(fd);
    if (LATTLAST_SKALA > 1.0) {
        PangoAttrList* a = pango_attr_list_new();
        pango_attr_list_insert(a, pango_attr_letter_spacing_new((int)(s.storlek * 0.06 * PANGO_SCALE)));
        pango_layout_set_attributes(l, a);
        pango_attr_list_unref(a);
        pango_layout_set_line_spacing(l, 1.45f);
    }
    return l;
}

// Text med radbrytning; returnerar höjden. rita = false mäter bara.
double stycke(cairo_t* cr, double x, double y, double w, const std::string& t, const Stil& s, bool rita = true) {
    if (t.empty()) return 0;
    PangoLayout* l = layout(cr, s);
    pango_layout_set_width(l, (int)(w * PANGO_SCALE));
    pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
    if (s.mitten) pango_layout_set_alignment(l, PANGO_ALIGN_CENTER);
    pango_layout_set_text(l, t.c_str(), -1);
    int pb = 0, ph = 0;
    pango_layout_get_pixel_size(l, &pb, &ph);
    if (rita) {
        cairo_save(cr);
        tema::satt(cr, s.farg ? s.farg : tema::TEXT);
        cairo_move_to(cr, x, y);
        pango_cairo_show_layout(cr, l);
        cairo_restore(cr);
    }
    g_object_unref(l);
    return ph;
}

double textbredd(cairo_t* cr, const std::string& t, const Stil& s) {
    PangoLayout* l = layout(cr, s);
    pango_layout_set_text(l, t.c_str(), -1);
    int pb = 0, ph = 0;
    pango_layout_get_pixel_size(l, &pb, &ph);
    g_object_unref(l);
    return pb;
}

void rektangel(cairo_t* cr, double x, double y, double b, double h, const char* fyll, const char* kant, double tjock = 0.8) {
    cairo_rectangle(cr, x, y, b, h);
    if (fyll) { tema::satt(cr, fyll); cairo_fill_preserve(cr); }
    if (kant) { tema::satt(cr, kant); cairo_set_line_width(cr, tjock); cairo_stroke(cr); }
    cairo_new_path(cr);
}

// En del av sidan: höjd + hur den ritas (x, y = övre vänstra hörnet)
struct Del {
    double h = 0;
    std::function<void(cairo_t*, double, double)> rita;
};

// Flödet: delar läggs uppifrån och ned, ny sida när det tar slut. Ett block hålls ihop om det får plats.
class Flode {
public:
    cairo_t* cr;
    double y = M;
    int sida = 1;
    std::string fot;
    std::function<void()> nySidaHook;

    explicit Flode(cairo_t* c) : cr(c) {}

    void sidfot() {
        if (fot.empty()) return;
        Stil s{8, false, false, tema::TEXT_SVAG};
        stycke(cr, M, SH - M + 10, BR, fot, s);
        std::string nr = std::string(T("page ", "sida ")) + std::to_string(sida);
        double b = textbredd(cr, nr, s);
        stycke(cr, SB - M - b, SH - M + 10, b + 2, nr, s);
    }
    void nySida() {
        sidfot();
        cairo_show_page(cr);
        ++sida;
        y = M;
        if (nySidaHook) nySidaHook();
    }
    double kvar() const { return BOTTEN - y; }
    bool overst() const { return y <= M + 0.5; }
    void block(const std::vector<Del>& delar, double luftEfter = 0) {
        double sum = 0;
        for (auto& d : delar) sum += d.h;
        if (sum > kvar() && !overst() && sum <= BOTTEN - M) nySida();
        for (auto& d : delar) {
            if (d.h > kvar() && !overst()) nySida();
            if (d.rita) d.rita(cr, M, y);
            y += d.h;
        }
        y += luftEfter;
    }
    void avsluta() { sidfot(); }
};

// ---- byggstenar ------------------------------------------------------------------------------------

Del textdel(cairo_t* cr, const std::string& t, Stil s, double indrag = 0, double luft = 2) {
    double w = BR - indrag;
    double h = stycke(cr, 0, 0, w, t, s, false);
    return {h + luft, [t, s, indrag, w](cairo_t* cr, double x, double y) { stycke(cr, x + indrag, y, w, t, s); }};
}

Del luft(double h) { return {h, nullptr}; }

// En figur, skalad så att den får plats (skärmens pixlar → punkter: 0,75)
Del figurdel(const Data& d, const Json& besk, double indrag = 0, double skala = 0.75, double maxH = 330) {
    Figur f;
    try { f = figur(d, besk); } catch (const std::exception& e) { g_warning("Matte: %s", e.what()); return luft(0); }
    if (f.bredd <= 0 || f.hojd <= 0) return luft(0);
    double s = std::min({skala, (BR - indrag) / f.bredd, maxH / f.hojd});
    return {f.hojd * s + 8, [f, s, indrag](cairo_t* cr, double x, double y) {
        cairo_save(cr);
        cairo_translate(cr, x + indrag, y + 3);
        cairo_scale(cr, s, s);
        f.rita(cr, 0, 0);
        cairo_restore(cr);
    }};
}

// En ruta med färgad kant till vänster ("Gäller", "Slutar gälla", stegen)
Del rutdel(cairo_t* cr, const std::string& t, const char* bakgrund, const char* kant, double indrag = 0, bool streckad = false,
           Stil s = Stil{10.5}) {
    double w = BR - indrag - 20;
    double h = stycke(cr, 0, 0, w, t, s, false) + 12;
    return {h + 6, [=](cairo_t* cr, double x, double y) {
        double bx = x + indrag;
        if (streckad) {
            double streck[] = {3, 2};
            cairo_set_dash(cr, streck, 2, 0);
            rektangel(cr, bx, y, BR - indrag, h, bakgrund, kant, 0.8);
            cairo_set_dash(cr, nullptr, 0, 0);
        } else {
            rektangel(cr, bx, y, BR - indrag, h, bakgrund, nullptr);
            rektangel(cr, bx, y, 3, h, kant, nullptr);
        }
        stycke(cr, bx + 12, y + 6, w, t, s);
    }};
}

Del svarsrad(const std::string& etikett, double indrag) {
    return {26, [etikett, indrag](cairo_t* cr, double x, double y) {
        Stil s{10, false, false, tema::TEXT_SVAG};
        double b = etikett.empty() ? 0 : textbredd(cr, etikett, s) + 6;
        stycke(cr, x + indrag, y + 8, b + 2, etikett, s);
        tema::satt(cr, "#9aa3ad"); cairo_set_line_width(cr, 0.7);
        cairo_move_to(cr, x + indrag + b, y + 21); cairo_line_to(cr, x + indrag + b + 220, y + 21); cairo_stroke(cr);
    }};
}

void kryssruta(cairo_t* cr, double x, double y, double s) {
    rektangel(cr, x, y, s, s, "#ffffff", tema::LINJE, 0.9);
}

bool arBildsak(const std::string& s) {
    return s.rfind("form:", 0) == 0 || s.rfind("ikon:", 0) == 0 || s.rfind("prickar:", 0) == 0;
}

// Alternativ på en rad (radbryts): ☐ text   ☐ text …   – eller bilder i rutor
std::vector<Del> alternativdelar(cairo_t* cr, const Data& d, const std::vector<std::string>& alt, double indrag) {
    std::vector<Del> ut;
    bool bilder = std::any_of(alt.begin(), alt.end(), arBildsak);
    if (bilder) {
        double cell = 56, gap = 26;
        int perRad = std::max(1, (int)((BR - indrag) / (cell + gap)));
        for (size_t i = 0; i < alt.size(); i += perRad) {
            std::vector<std::string> rad(alt.begin() + i, alt.begin() + std::min(alt.size(), i + perRad));
            const Data* dp = &d;
            ut.push_back({cell + 8, [rad, dp, indrag, cell, gap](cairo_t* cr, double x, double y) {
                for (size_t j = 0; j < rad.size(); ++j) {
                    double cx = x + indrag + j * (cell + gap);
                    kryssruta(cr, cx, y + cell / 2 - 5, 10);
                    rektangel(cr, cx + 14, y, cell, cell, "#ffffff", "#c8ced6", 0.6);
                    sak(cr, *dp, cx + 17, y + 3, cell - 6, cell - 6, rad[j]);
                }
            }});
        }
        return ut;
    }
    Stil s{11};
    std::vector<std::vector<std::string>> rader(1);
    double xx = 0;
    for (auto& a : alt) {
        double b = 16 + textbredd(cr, a, s) + 20;
        if (xx + b > BR - indrag && !rader.back().empty()) { rader.emplace_back(); xx = 0; }
        rader.back().push_back(a);
        xx += b;
    }
    for (auto& rad : rader)
        ut.push_back({20, [rad, indrag, s](cairo_t* cr, double x, double y) {
            double xx = x + indrag;
            for (auto& a : rad) {
                kryssruta(cr, xx, y + 4, 10);
                double b = textbredd(cr, a, s);
                stycke(cr, xx + 16, y + 1, b + 4, a, s);
                xx += 16 + b + 20;
            }
        }});
    return ut;
}

// Para på papper: två spalter med prickar att dra streck mellan
Del paradel(cairo_t* cr, const Data& d, const std::vector<std::string>& v, const std::vector<std::string>& h, double indrag) {
    Stil s{11, true};
    bool bilder = false;
    double langst = 30;
    for (auto* l : {&v, &h})
        for (auto& x : *l) { if (arBildsak(x)) bilder = true; else langst = std::max(langst, textbredd(cr, x, s)); }
    double bw = std::clamp(langst + 20, bilder ? 52.0 : 60.0, 210.0), bh = bilder ? 50 : 24, gap = 7, mitt = 110;
    size_t n = std::max(v.size(), h.size());
    const Data* dp = &d;
    return {n * (bh + gap) + 6, [=](cairo_t* cr, double x, double y) {
        for (int sida = 0; sida < 2; ++sida) {
            const auto& l = sida ? h : v;
            double x0 = x + indrag + (sida ? bw + mitt : 0);
            for (size_t i = 0; i < l.size(); ++i) {
                double yy = y + i * (bh + gap);
                rektangel(cr, x0, yy, bw, bh, "#ffffff", "#9aa3ad", 0.8);
                if (arBildsak(l[i])) sak(cr, *dp, x0 + 4, yy + 3, bw - 8, bh - 6, l[i]);
                else { Stil c = s; c.mitten = true; stycke(cr, x0 + 4, yy + (bh - 14) / 2, bw - 8, l[i], c); }
                cairo_new_path(cr);
                cairo_arc(cr, sida ? x0 - 6 : x0 + bw + 6, yy + bh / 2, 2.6, 0, 2 * M_PI);
                tema::satt(cr, tema::LINJE); cairo_fill(cr);
            }
        }
    }};
}

// Färgpennorna som en liten förklaring under en målauppgift
Del penndel(cairo_t* cr, const std::vector<std::string>& pennor, double indrag) {
    return {18, [pennor, indrag](cairo_t* cr, double x, double y) {
        Stil s{9.5, false, false, tema::TEXT_SVAG};
        double xx = x + indrag;
        stycke(cr, xx, y + 2, 80, T("Pens:", "Pennor:"), s);
        xx += textbredd(cr, T("Pens:", "Pennor:"), s) + 8;
        for (auto& p : pennor) {
            cairo_new_path(cr);
            cairo_arc(cr, xx + 5, y + 8, 5, 0, 2 * M_PI);
            tema::satt(cr, pennfarg(p)); cairo_fill_preserve(cr);
            tema::satt(cr, tema::LINJE); cairo_set_line_width(cr, 0.5); cairo_stroke(cr);
            stycke(cr, xx + 13, y + 2, 60, p, s);
            xx += 13 + textbredd(cr, p, s) + 12;
        }
    }};
}

// Ritrutan: en tom ruta, eller figuren (rutpapper, koordinatsystem) med plats runt
Del ritdel(const Data& d, const Json& besk, double indrag) {
    Figur f;
    bool harFig = false;
    if (besk.arObjekt()) try { f = figur(d, besk); harFig = true; } catch (...) {}
    double s = harFig ? std::min(0.75, (BR - indrag - 12) / f.bredd) : 1;
    double h = harFig ? std::max(170.0, f.hojd * s + 12) : 190;
    return {h + 8, [=](cairo_t* cr, double x, double y) {
        rektangel(cr, x + indrag, y, BR - indrag, h, "#ffffff", "#9aa3ad", 0.8);
        if (harFig) {
            cairo_save(cr);
            cairo_translate(cr, x + indrag + 6, y + 6);
            cairo_scale(cr, s, s);
            f.rita(cr, 0, 0);
            cairo_restore(cr);
        }
    }};
}

// ---- arbetsbladets uppgifter -----------------------------------------------------------------------

const char* pappersinstruktion(const std::string& typ) {
    if (typ == "valj") return T("Circle the answer in the picture.", "Ringa in svaret i bilden.");
    if (typ == "markera") return T("Circle everything that fits.", "Ringa in allt som passar.");
    if (typ == "klick") return T("Circle and count.", "Ringa in och räkna.");
    if (typ == "farga") return T("Colour in.", "Färglägg.");
    if (typ == "dra") return T("Draw the counters in the frame.", "Rita brickorna i ramen.");
    if (typ == "punkt") return T("Mark the answer.", "Pricka in svaret.");
    if (typ == "linje") return T("Draw the line.", "Rita linjen.");
    if (typ == "skiss") return T("Draw first if it helps.", "Rita gärna först.");
    if (typ == "val") return T("Tick the right answer.", "Kryssa i rätt svar.");
    if (typ == "kryssa") return T("Tick every one that fits – there can be more than one.", "Kryssa i alla som stämmer – det kan vara flera.");
    if (typ == "para") return T("Draw a line between the things that belong together.", "Dra streck mellan det som hör ihop.");
    if (typ == "mala") return T("Colour with the pens below.", "Färglägg med pennorna nedanför.");
    if (typ == "rita") return T("Draw in the box.", "Rita i rutan.");
    return nullptr;
}

std::string facitVisning(const Data& d, const Mall& m, const Variant& v) {
    const auto& ft = v.facitText;
    if (m.typ == "rita") return T("Not marked – look at it together.", "Rättas inte – titta på den tillsammans.");
    if (ft.empty()) return "–";
    if (m.typ == "markera") {
        std::vector<std::string> delar;
        std::string del;
        for (char c : ft[0] + ",") { if (c == ',') { if (!del.empty()) delar.push_back(trim(del)); del.clear(); } else del += c; }
        std::set<std::string> namn(delar.begin(), delar.end());
        std::string r = std::to_string(delar.size()) + " st: ";
        bool forsta = true;
        for (auto& n : namn) { r += (forsta ? "" : ", ") + n; forsta = false; }
        return r;
    }
    if (m.typ == "valj") { std::string s = ft[0]; if (s.rfind("siffra-", 0) == 0) s = s.substr(7); return s; }
    if (m.typ == "punkt" && ft.size() == 2) return "(" + ft[0] + ", " + ft[1] + ")";
    if (m.typ == "linje" && ft.size() == 2) return "k = " + ft[0] + ", m = " + ft[1];
    if (m.typ == "mala" && v.figur["typ"].str() == "malrutor" && !v.facit.empty() && v.facit[0].rfind("m:", 0) == 0) {
        // per färg: "blå: 16, 18, 20 · röd: 17, 19" i stället för en lång lista
        std::vector<std::string> obj = v.figur["objekt"].strangar();
        std::vector<std::pair<std::string, std::string>> per;
        for (size_t i = 0; i < v.facitSvar.size(); ++i) {
            if (v.facitSvar[i].empty()) continue;
            std::string namn = i < obj.size() && !obj[i].empty() ? obj[i] : std::to_string(i + 1);
            for (const char* p : {"form:", "ikon:"}) if (namn.rfind(p, 0) == 0) namn = namn.substr(strlen(p));
            auto it = std::find_if(per.begin(), per.end(), [&](auto& x) { return x.first == v.facitSvar[i]; });
            if (it == per.end()) per.push_back({v.facitSvar[i], namn});
            else it->second += ", " + namn;
        }
        std::string r;
        for (auto& [f, lista] : per) r += (r.empty() ? "" : "  ·  ") + f + ": " + lista;
        if (!r.empty()) return r;
    }
    if (m.typ == "kryssa" || m.typ == "para") {   // former och prickar i ord: "form:triangel" → "triangel"
        std::string r = ft[0];
        for (const char* p : {"form:", "ikon:"}) for (size_t i; (i = r.find(p)) != std::string::npos;) r.erase(i, strlen(p));
        for (size_t i; (i = r.find("prickar:")) != std::string::npos;) r.replace(i, 8, "");
        return r;
    }
    std::string r;
    for (size_t i = 0; i < ft.size(); ++i) r += (i ? ", " : "") + (i < v.falt.size() && ft.size() > 1 ? v.falt[i] + " = " : "") + ft[i];
    return r;
}

struct Val { const Kort* k; const Mall* m; const Variant* v; };

// Lätt → svårt: mallarna sorteras efter nivå och plockas jämnt utspritt, varianten slumpas – allt ur bladnumret
std::vector<Val> valjUppgifter(const Data& d, const std::vector<const Kort*>& kort, int blad, int antal) {
    std::vector<Val> ut;
    for (const Kort* k : kort) {
        const auto& ms = d.mallar(k->id);
        std::vector<int> idx;
        for (size_t i = 0; i < ms.size(); ++i) if (!ms[i].varianter.empty()) idx.push_back((int)i);
        if (idx.empty()) continue;
        std::seed_seq fro{(unsigned)blad, (unsigned)std::hash<std::string>{}(k->id)};
        std::mt19937 rnd(fro);
        std::shuffle(idx.begin(), idx.end(), rnd);
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return ms[a].niva < ms[b].niva; });
        int n = (int)idx.size();
        for (int i = 0; i < antal; ++i) {
            int val;
            if (n >= antal) {
                int lo = i * n / antal, hi = std::max(lo + 1, (i + 1) * n / antal);
                val = idx[lo + (int)(rnd() % (unsigned)(hi - lo))];
            } else {
                val = idx[i % n];
            }
            const Mall& m = ms[val];
            ut.push_back({k, &m, &m.varianter[rnd() % m.varianter.size()]});
        }
    }
    return ut;
}

// Skärmens ord blir pappersord: "Klicka på cirkeln" → "Ringa in cirkeln", "Dra 4 brickor till" → "Rita 4 brickor i"
std::string pappersfraga(const std::string& fraga, const std::string& typ, bool& andrad) {
    std::string f = fraga;
    andrad = false;
    auto byt = [&](const std::string& fran, const std::string& till) {
        for (size_t i; (i = f.find(fran)) != std::string::npos;) { f.replace(i, fran.size(), till); andrad = true; }
    };
    if (typ == "valj" || typ == "markera" || typ == "klick") { byt("Klicka på ", "Ringa in "); byt("klicka på ", "ringa in "); }
    if (typ == "dra") { byt("Dra ", "Rita "); byt(" till tioramen", " i tioramen"); }
    return f;
}

std::vector<Del> uppgiftsdelar(cairo_t* cr, const Data& d, int nr, const Mall& m, const Variant& v) {
    const double IN = 26;
    std::vector<Del> delar;
    Stil fraga{11.5};
    bool andrad = false;
    std::string fr = pappersfraga(v.fraga, m.typ, andrad), nrs = std::to_string(nr) + ".";
    double h = stycke(cr, 0, 0, BR - IN, fr, fraga, false);
    delar.push_back({h + 4, [fr, nrs, fraga, IN](cairo_t* cr, double x, double y) {
        stycke(cr, x, y, IN - 2, nrs, Stil{11.5, true});
        stycke(cr, x + IN, y, BR - IN, fr, fraga);
    }});
    if (!v.steg.empty()) {
        std::string s;
        for (auto& x : v.steg) s += (s.empty() ? "" : "\n") + x;
        delar.push_back(rutdel(cr, s, "#fafbfc", "#9aa3ad", IN, true, Stil{11.5}));
    }
    if (const char* ins = andrad ? nullptr : pappersinstruktion(m.typ)) delar.push_back(textdel(cr, ins, Stil{9.5, false, true, tema::TEXT_SVAG}, IN, 4));

    if (m.typ == "rita") {
        delar.push_back(ritdel(d, v.figur, IN));
    } else if (m.typ == "para") {
        delar.push_back(paradel(cr, d, v.falt, v.alternativ, IN));
    } else {
        if (v.figur.arObjekt()) {
            Json f = v.figur;
            if (f["typ"].str() == "malrutor") f.satt("malade", Json::lista());
            delar.push_back(figurdel(d, f, IN, 0.75, 300));
        }
        if (m.typ == "mala") {
            std::vector<std::string> pennor = v.figur["farger"].strangar();
            if (!pennor.empty()) delar.push_back(penndel(cr, pennor, IN));
        }
        if (m.typ == "val" || m.typ == "kryssa")
            for (auto& x : alternativdelar(cr, d, v.alternativ, IN)) delar.push_back(x);
    }
    // plats för svaret
    static const std::set<std::string> I_FIGUREN = {"valj", "markera", "farga", "dra", "val", "kryssa", "para", "mala", "rita"};
    if ((m.typ == "tabell" || m.typ == "punkt" || m.typ == "linje") && !v.falt.empty() && v.facit.size() > 1) {
        for (auto& f : v.falt) delar.push_back(svarsrad(f.find('=') != std::string::npos || f.find(':') != std::string::npos ? f : f + " =", IN));
    } else if (m.typ == "klick") {
        delar.push_back(svarsrad(T("How many?", "Hur många?"), IN));
    } else if ((m.typ == "punkt" || m.typ == "linje") && v.figur.arObjekt()) {
        // svaret ritas i figuren
    } else if (!I_FIGUREN.count(m.typ)) {
        delar.push_back(svarsrad(T("Answer:", "Svar:"), IN));
    }
    return delar;
}

cairo_surface_t* pdfyta(const std::string& fil, const std::string& titel) {
    cairo_surface_t* y = cairo_pdf_surface_create(fil.c_str(), SB, SH);
    if (cairo_surface_status(y) != CAIRO_STATUS_SUCCESS) return y;
    cairo_pdf_surface_set_metadata(y, CAIRO_PDF_METADATA_TITLE, titel.c_str());
    cairo_pdf_surface_set_metadata(y, CAIRO_PDF_METADATA_CREATOR, "Matte (WenuyOS)");
    return y;
}

std::string datumIdag() {
    GDateTime* nu = g_date_time_new_now_local();
    gchar* s = g_date_time_format(nu, "%Y-%m-%d");
    std::string r = s;
    g_free(s); g_date_time_unref(nu);
    return r;
}

}  // namespace

// ---- arbetsblad -------------------------------------------------------------------------------------

int arbetsbladPdf(const Data& d, const std::vector<const Kort*>& kort, const std::string& fil, int blad, int antal, std::string* fel,
                  bool lattlast) {
    if (kort.empty()) { if (fel) *fel = "inga kort"; return 0; }
    struct Aterstall { ~Aterstall() { LATTLAST_SKALA = 1.0; } } aterstall;
    LATTLAST_SKALA = lattlast ? 1.25 : 1.0;
    if (blad <= 0) blad = 1000 + g_random_int_range(0, 9000);
    auto uppg = valjUppgifter(d, kort, blad, antal);
    if (uppg.empty()) { if (fel) *fel = "korten har inga uppgifter"; return 0; }
    std::string titel = kort.size() == 1 ? kort[0]->titel : T("Worksheet", "Arbetsblad");
    cairo_surface_t* yta = pdfyta(fil, std::string(T("Worksheet ", "Arbetsblad ")) + std::to_string(blad) + " – " + titel);
    if (cairo_surface_status(yta) != CAIRO_STATUS_SUCCESS) {
        if (fel) *fel = cairo_status_to_string(cairo_surface_status(yta));
        cairo_surface_destroy(yta);
        return 0;
    }
    UTSKRIFT = true;
    cairo_t* cr = cairo_create(yta);
    Flode fl(cr);
    std::string kortnamn;
    for (size_t i = 0; i < kort.size(); ++i) kortnamn += (i ? ", " : "") + kort[i]->titel;
    fl.fot = std::string("Matte · ") + T("Sheet ", "Blad ") + std::to_string(blad) + " · " + kortnamn;

    // huvudet: titel, namn och datum att fylla i
    fl.block({{34, [titel](cairo_t* cr, double x, double y) {
        stycke(cr, x, y, BR * 0.55, titel, Stil{17, true});
        Stil s{10, false, false, tema::TEXT_SVAG};
        std::string n = std::string(T("Name", "Namn")) + " ____________________   " + T("Date", "Datum") + " ________";
        double b = textbredd(cr, n, s);
        stycke(cr, x + BR - b, y + 6, b + 2, n, s);
    }}, {10, [](cairo_t* cr, double x, double y) {
        tema::satt(cr, tema::LINJE); cairo_set_line_width(cr, 0.8);
        cairo_move_to(cr, x, y + 2); cairo_line_to(cr, x + BR, y + 2); cairo_stroke(cr);
    }}, textdel(cr, std::string(T("Sheet ", "Blad ")) + std::to_string(blad) + " · " + kortnamn + " · " +
                    T("Same number = same tasks.", "Samma nummer ger samma uppgifter."),
                Stil{9, false, true, tema::TEXT_SVAG}, 0, 10)});

    for (size_t i = 0; i < uppg.size(); ++i) {
        std::vector<Del> delar = uppgiftsdelar(cr, d, (int)i + 1, *uppg[i].m, *uppg[i].v);
        if (kort.size() > 1 && (i == 0 || uppg[i].k != uppg[i - 1].k))   // flera kort: en rubrik för varje
            delar.insert(delar.begin(), textdel(cr, uppg[i].k->titel, Stil{12.5, true, false, tema::ACCENT}, 0, 6));
        fl.block(delar, 14);
    }

    // facit på en egen sida – kan vikas bort eller rivas av
    fl.nySida();
    fl.block({textdel(cr, std::string(T("Answers – sheet ", "Facit – blad ")) + std::to_string(blad), Stil{15, true}, 0, 4),
              textdel(cr, kortnamn, Stil{9.5, false, true, tema::TEXT_SVAG}, 0, 10)});
    for (size_t i = 0; i < uppg.size(); ++i) {
        const Variant& v = *uppg[i].v;
        std::vector<Del> delar;
        std::string nrs = std::to_string(i + 1) + ".", svar = facitVisning(d, *uppg[i].m, v);
        if (uppg[i].m->typ == "farga" && !v.facitText.empty()) svar = v.facitText[0] + T(" parts", " delar");
        Stil fs{11, true};
        double h = stycke(cr, 0, 0, BR - 26, svar, fs, false);
        delar.push_back({h + 2, [nrs, svar, fs](cairo_t* cr, double x, double y) {
            stycke(cr, x, y, 24, nrs, Stil{11});
            stycke(cr, x + 26, y, BR - 26, svar, fs);
        }});
        if (!v.losning.empty()) {
            std::string l;
            for (auto& x : v.losning) l += (l.empty() ? "" : "  ·  ") + x;
            delar.push_back(textdel(cr, l, Stil{9, false, false, tema::TEXT_SVAG}, 26, 0));
        }
        fl.block(delar, 8);
    }
    fl.avsluta();
    cairo_destroy(cr);
    cairo_surface_finish(yta);
    bool ok = cairo_surface_status(yta) == CAIRO_STATUS_SUCCESS;
    if (!ok && fel) *fel = cairo_status_to_string(cairo_surface_status(yta));
    cairo_surface_destroy(yta);
    UTSKRIFT = false;
    return ok ? blad : 0;
}

std::vector<std::string> arbetsbladFragor(const Data& d, const std::vector<const Kort*>& kort, int blad, int antal) {
    std::vector<std::string> ut;
    for (auto& u : valjUppgifter(d, kort, blad, antal)) ut.push_back(u.v->fraga);
    return ut;
}

// ---- regelboken -------------------------------------------------------------------------------------

namespace {

// Ett kort: rubrik, målning, kärnmening, raderna (man säger – bild – man skriver), exemplen, gäller/slutar gälla
void ritaKort(Flode& fl, const Data& d, const Kort& k) {
    cairo_t* cr = fl.cr;
    fl.block({textdel(cr, k.titel, Stil{19, true}, 0, 2),
              textdel(cr, d.omradeNamn(k.omrade), Stil{9.5, false, false, tema::TEXT_SVAG}, 0, 8)});
    if (!k.bild.empty()) {
        std::string p = d.bild(k.bild);
        int ib = 0, ih = 0;
        bildstorlek(p, ib, ih);
        if (ib > 0 && ih > 0) {
            double b = BR, h = BR * ih / ib;
            if (h > 270) { h = 270; b = h * ib / ih; }
            std::vector<Del> bd = {{h + 6, [p, b, h](cairo_t* cr, double x, double y) { bild(cr, x + (BR - b) / 2, y, b, h, p); }}};
            if (!k.bildtext.empty()) bd.push_back(textdel(cr, k.bildtext, Stil{9.5, false, true, tema::TEXT_SVAG}, 0, 4));
            fl.block(bd, 6);
        }
    }
    if (!k.sager.empty()) fl.block({textdel(cr, k.sager, Stil{12.5}, 0, 8)});

    if (!k.rader.empty()) {
        bool treFormer = std::all_of(k.rader.begin(), k.rader.end(), [](const Rad& r) { return r.skriver.size() == 3; });
        bool sagerKolumn = !treFormer && std::any_of(k.rader.begin(), k.rader.end(), [](const Rad& r) { return !r.sager.empty(); });
        double kSager = sagerKolumn ? 150 : 0, kFig = 170, kSkriver = BR - kSager - kFig - 16;
        Stil rub{9.5, true, false, tema::TEXT_SVAG};
        if (treFormer) {
            fl.block({{16, [=](cairo_t* cr, double x, double y) {
                double w = (BR - kFig - 8) / 3;
                const char* r[] = {T("Fraction", "Bråkform"), T("Percent", "Procentform"), T("Decimal", "Decimalform")};
                for (int i = 0; i < 3; ++i) stycke(cr, x + kFig + 8 + i * w, y, w, r[i], rub);
            }}});
        } else if (sagerKolumn) {
            fl.block({{16, [=](cairo_t* cr, double x, double y) {
                stycke(cr, x, y, kSager, T("You say", "Man säger"), rub);
                stycke(cr, x + kSager + kFig + 16, y, kSkriver, T("You write", "Man skriver"), rub);
            }}});
        }
        for (const Rad& r : k.rader) {
            Figur f; bool harFig = false;
            if (r.figur.arObjekt()) try { f = figur(d, r.figur); harFig = f.bredd > 0; } catch (...) {}
            double s = harFig ? std::min({0.7, kFig / f.bredd, 110 / f.hojd}) : 0;
            double fh = harFig ? f.hojd * s : 0;
            Stil ts{11.5}, ws{12.5, true};
            std::string skriver;
            for (auto& x : r.skriver) skriver += (skriver.empty() ? "" : "\n") + x;
            double h = std::max(fh, stycke(cr, 0, 0, std::max(40.0, kSager - 8), r.sager, ts, false));
            if (treFormer) {
                double w = (BR - kFig - 8) / 3;
                for (auto& x : r.skriver) h = std::max(h, stycke(cr, 0, 0, w - 6, x, ws, false));
            } else {
                h = std::max(h, stycke(cr, 0, 0, kSkriver, skriver, ws, false));
            }
            std::vector<Del> rad = {{h + 10, [=](cairo_t* cr, double x, double y) {
                if (sagerKolumn) stycke(cr, x, y + 4, kSager - 8, r.sager, ts);
                if (harFig) {
                    cairo_save(cr);
                    cairo_translate(cr, x + kSager, y + 4);
                    cairo_scale(cr, s, s);
                    f.rita(cr, 0, 0);
                    cairo_restore(cr);
                }
                if (treFormer) {
                    double w = (BR - kFig - 8) / 3;
                    for (size_t i = 0; i < r.skriver.size(); ++i) stycke(cr, x + kFig + 8 + i * w, y + 4, w - 6, r.skriver[i], ws);
                } else {
                    stycke(cr, x + kSager + kFig + 16, y + 4, kSkriver, skriver, ws);
                }
                tema::satt(cr, "#dde2e7"); cairo_set_line_width(cr, 0.6);
                cairo_move_to(cr, x, y + h + 9); cairo_line_to(cr, x + BR, y + h + 9); cairo_stroke(cr);
            }}};
            if (!r.kommentar.empty()) rad.push_back(textdel(cr, r.kommentar, Stil{9.5, false, true, tema::TEXT_SVAG}, 0, 4));
            fl.block(rad);
        }
        fl.y += 6;
    }

    for (const Exempel& e : k.exempel) {
        std::vector<Del> ex;
        if (!e.rubrik.empty()) ex.push_back(textdel(cr, e.rubrik, Stil{12, true}, 0, 3));
        if (!e.text.empty()) ex.push_back(textdel(cr, e.text, Stil{11}, 0, 3));
        if (e.figur.arObjekt()) ex.push_back(figurdel(d, e.figur, 0, 0.7, 200));
        for (size_t i = 0; i < e.steg.size(); ++i) ex.push_back(textdel(cr, std::to_string(i + 1) + ".  " + e.steg[i], Stil{11}, 10, 2));
        fl.block(ex, 10);
    }
    std::vector<Del> slut;
    if (!k.galler.empty()) slut.push_back(rutdel(cr, std::string(T("Applies: ", "Gäller: ")) + k.galler, "#fdeedc", tema::ACCENT));
    if (!k.galler_inte.empty()) slut.push_back(rutdel(cr, std::string(T("Stops applying: ", "Slutar gälla: ")) + k.galler_inte, "#fbe9e8", tema::ROD));
    if (!slut.empty()) fl.block(slut, 4);
    if (!k.kalla.empty()) fl.block({textdel(cr, std::string(T("Based on: ", "Bygger på: ")) + k.kalla, Stil{8.5, false, false, tema::TEXT_SVAG}, 0, 0)});
}

// Hela boken på en yta. sidor (ut) = sidan där varje kort börjar; innehåll (in) = samma från förra varvet.
void ritaBok(cairo_t* cr, const Data& d, const std::vector<const Kort*>& kort, const std::string& titel, const std::string& undertitel,
             const std::vector<int>& innehall, std::vector<int>& sidor, cairo_surface_t* pdf, const std::vector<std::string>* grupper) {
    Flode fl(cr);
    fl.fot = "Matte · " + titel;
    // försättsblad med innehållsförteckning
    fl.y = M + 80;
    fl.block({textdel(cr, titel, Stil{28, true}, 0, 6), textdel(cr, undertitel, Stil{12, false, false, tema::TEXT_SVAG}, 0, 4),
              textdel(cr, std::to_string(kort.size()) + T(" cards · ", " kort · ") + datumIdag(), Stil{10, false, false, tema::TEXT_SVAG}, 0, 30),
              textdel(cr, T("Contents", "Innehåll"), Stil{13, true}, 0, 6)});
    std::string grupp;
    for (size_t i = 0; i < kort.size(); ++i) {
        std::string g = grupper && i < grupper->size() ? (*grupper)[i] : d.omradeNamn(kort[i]->omrade);
        if (g != grupp) {
            grupp = g;
            fl.block({luft(4), textdel(cr, g, Stil{9.5, true, false, tema::TEXT_SVAG}, 0, 2)});
        }
        std::string t = kort[i]->titel, nr = i < innehall.size() ? std::to_string(innehall[i]) : "";
        fl.block({{16, [t, nr](cairo_t* cr, double x, double y) {
            Stil s{11};
            stycke(cr, x + 10, y, BR - 60, t, s);
            double b = textbredd(cr, nr, s);
            stycke(cr, x + BR - b, y, b + 2, nr, s);
            double tb = textbredd(cr, t, s);
            double streck[] = {1, 3};
            cairo_set_dash(cr, streck, 2, 0);
            tema::satt(cr, "#b0b7bf"); cairo_set_line_width(cr, 0.6);
            cairo_move_to(cr, x + 16 + tb, y + 11); cairo_line_to(cr, x + BR - b - 6, y + 11); cairo_stroke(cr);
            cairo_set_dash(cr, nullptr, 0, 0);
        }}});
    }
    sidor.clear();
    for (const Kort* k : kort) {
        fl.nySida();
        sidor.push_back(fl.sida);
        if (pdf) cairo_pdf_surface_add_outline(pdf, CAIRO_PDF_OUTLINE_ROOT, k->titel.c_str(),
                                               ("page=" + std::to_string(fl.sida)).c_str(), (cairo_pdf_outline_flags_t)0);
        ritaKort(fl, d, *k);
    }
    fl.avsluta();
}

cairo_status_t ingenstans(void*, const unsigned char*, unsigned int) { return CAIRO_STATUS_SUCCESS; }

}  // namespace

bool regelbokPdf(const Data& d, const std::vector<const Kort*>& kort, const std::string& titel, const std::string& undertitel,
                 const std::string& fil, std::string* fel, const std::vector<std::string>* grupper) {
    if (kort.empty()) { if (fel) *fel = "inga kort"; return false; }
    UTSKRIFT = true;
    // Varv 1 räknar ut sidnumren till innehållsförteckningen (ritas till ingenstans)
    std::vector<int> sidor, ingen;
    {
        cairo_surface_t* prov = cairo_pdf_surface_create_for_stream(ingenstans, nullptr, SB, SH);
        cairo_t* cr = cairo_create(prov);
        ritaBok(cr, d, kort, titel, undertitel, ingen, sidor, nullptr, grupper);
        cairo_destroy(cr);
        cairo_surface_finish(prov);
        cairo_surface_destroy(prov);
    }
    cairo_surface_t* yta = pdfyta(fil, titel);
    bool ok = cairo_surface_status(yta) == CAIRO_STATUS_SUCCESS;
    if (ok) {
        cairo_t* cr = cairo_create(yta);
        std::vector<int> igen;
        ritaBok(cr, d, kort, titel, undertitel, sidor, igen, yta, grupper);
        cairo_destroy(cr);
        cairo_surface_finish(yta);
        ok = cairo_surface_status(yta) == CAIRO_STATUS_SUCCESS;
    }
    if (!ok && fel) *fel = cairo_status_to_string(cairo_surface_status(yta));
    cairo_surface_destroy(yta);
    UTSKRIFT = false;
    return ok;
}

std::vector<const Kort*> kortILaroplansordning(const Data& d, std::vector<std::string>* stadium) {
    std::vector<const Kort*> ut;
    std::set<std::string> sett;
    for (auto& [st, namn] : d.stadier())
        for (auto& m : d.laroplan()) {
            if (m.stadium != st) continue;
            for (auto& id : m.kort)
                if (const Kort* k = d.kort(id); k && sett.insert(id).second) {
                    ut.push_back(k);
                    if (stadium) stadium->push_back(namn);
                }
        }
    for (auto& k : d.kort())   // kort utanför läroplanen sist
        if (sett.insert(k.id).second) { ut.push_back(&k); if (stadium) stadium->push_back(T("Other", "Övrigt")); }
    return ut;
}

// ---- var filerna hamnar -----------------------------------------------------------------------------

std::string utskriftsmapp() {
    const char* dok = g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
    std::string bas = dok && *dok ? dok : std::string(g_get_home_dir()) + "/" + T("Documents", "Dokument");
    std::string m = bas + "/Matte";
    g_mkdir_with_parents(m.c_str(), 0700);
    return m;
}

std::string sakertFilnamn(const std::string& namn) {
    std::string r;
    for (char c : namn) r += (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') ? '-' : c;
    while (!r.empty() && (r[0] == '.' || r[0] == ' ')) r.erase(0, 1);
    return r.empty() ? "Matte" : r;
}

}  // namespace matte
