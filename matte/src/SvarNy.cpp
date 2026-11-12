// De nya svarsytorna (etapp 2): kryssa i rutor, dra linjer mellan par, måla ytor med färgpennor
// och en fri rityta. Rättningen finns i Bedom.cpp; här är bara det eleven ser och gör.
#include "SvarNy.h"

#include "Tal.h"

#include <Wenuy.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>

namespace matte {

namespace {

using wenuy::tr;
const double PAPPER = 10;

std::string accent() {
    GdkRGBA c;
    wenuy_theme_get_accent(wenuy_theme_get_default(), &c);
    char b[16];
    snprintf(b, sizeof b, "#%02x%02x%02x", (int)(c.red * 255), (int)(c.green * 255), (int)(c.blue * 255));
    return b;
}

void rundad(cairo_t* cr, double x, double y, double b, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + b - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + b - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
}

void papper(cairo_t* cr, double b, double h) {
    rundad(cr, 0, 0, b, h, 8);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.12);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
}

GtkWidget* etikett(const char* text, const char* klass = "svag") {
    GtkWidget* l = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_wrap(GTK_LABEL(l), TRUE);
    if (klass) gtk_widget_add_css_class(l, klass);
    return l;
}

void vidKlick(GtkWidget* b, std::function<void()> f) {
    auto* fp = new std::function<void()>(std::move(f));
    g_signal_connect_data(b, "clicked", CB(+[](GtkButton*, gpointer p) { (*static_cast<std::function<void()>*>(p))(); }), fp,
                          +[](gpointer p, GClosure*) { delete static_cast<std::function<void()>*>(p); }, (GConnectFlags)0);
}

GtkWidget* knapp(const char* text, bool huvud, std::function<void()> f, const char* ikon = nullptr) {
    GtkWidget* b = gtk_button_new();
    if (ikon) {
        GtkWidget* c = adw_button_content_new();
        adw_button_content_set_icon_name(ADW_BUTTON_CONTENT(c), ikon);
        adw_button_content_set_label(ADW_BUTTON_CONTENT(c), text);
        gtk_button_set_child(GTK_BUTTON(b), c);
    } else {
        gtk_button_set_label(GTK_BUTTON(b), text);
    }
    if (huvud) gtk_widget_add_css_class(b, "suggested-action");
    gtk_widget_set_halign(b, GTK_ALIGN_START);
    vidKlick(b, std::move(f));
    return b;
}

GtkWidget* rad(int mellan = 8) { return gtk_box_new(GTK_ORIENTATION_HORIZONTAL, mellan); }

bool arBild(const std::string& s) {
    return s.rfind("form:", 0) == 0 || s.rfind("ikon:", 0) == 0 || s.rfind("prickar:", 0) == 0;
}

// En sak (text, form, ikon, prickar) som egen liten bild – i kryssrutorna
struct SakRita { const Data* d; std::string s; };

GtkWidget* sakbild(const Data& d, const std::string& s, int storlek) {
    GtkWidget* w = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w), storlek);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w), storlek);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w), +[](GtkDrawingArea*, cairo_t* cr, int b, int h, gpointer p) {
        auto* s = static_cast<SakRita*>(p);
        papper(cr, b, h);
        sak(cr, *s->d, 4, 4, b - 8, h - 8, s->s);
    }, new SakRita{&d, s}, [](gpointer p) { delete static_cast<SakRita*>(p); });
    return w;
}

// Färgpennan som en rund klick (barnen ser färgen, inte ordet)
struct Klick { std::string farg; bool sudd; };

GtkWidget* pennknapp(const std::string& farg, bool sudd, GtkToggleButton* grupp) {
    GtkWidget* b = gtk_toggle_button_new();
    gtk_widget_add_css_class(b, "penna");
    gtk_widget_set_tooltip_text(b, sudd ? tr("Eraser", "Sudd") : farg.c_str());
    GtkWidget* w = gtk_drawing_area_new();
    int sz = (int)(26 * std::max(1.0, FIGURSKALA * 0.85));
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w), sz);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w), sz);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w), +[](GtkDrawingArea*, cairo_t* cr, int b, int h, gpointer p) {
        auto* k = static_cast<Klick*>(p);
        double r = std::min(b, h) / 2.0 - 2;
        if (k->sudd) {   // ett suddgummi: rosa kloss på sned
            cairo_save(cr);
            cairo_translate(cr, b / 2.0, h / 2.0);
            cairo_rotate(cr, -M_PI / 5);
            rundad(cr, -r, -r * 0.5, 2 * r, r, 3);
            tema::satt(cr, "#f4a6c0"); cairo_fill_preserve(cr);
            tema::satt(cr, tema::LINJE); cairo_set_line_width(cr, 1.5); cairo_stroke(cr);
            cairo_restore(cr);
            return;
        }
        cairo_arc(cr, b / 2.0, h / 2.0, r, 0, 2 * M_PI);
        tema::satt(cr, pennfarg(k->farg)); cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.35); cairo_set_line_width(cr, 1.5); cairo_stroke(cr);
    }, new Klick{farg, sudd}, [](gpointer p) { delete static_cast<Klick*>(p); });
    gtk_button_set_child(GTK_BUTTON(b), w);
    if (grupp) gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(b), grupp);
    return b;
}

void vidVal(GtkWidget* b, std::function<void()> f) {
    auto* fp = new std::function<void()>(std::move(f));
    g_signal_connect_data(b, "toggled", CB(+[](GtkToggleButton* t, gpointer p) {
        if (gtk_toggle_button_get_active(t)) (*static_cast<std::function<void()>*>(p))();
    }), fp, +[](gpointer p, GClosure*) { delete static_cast<std::function<void()>*>(p); }, (GConnectFlags)0);
}

double skalaFor(double bredd, const std::string& typ) {
    double s = FIGURSKALA;
    if (typ == "scen") s = std::min(s, 1.25);
    if ((bredd + 2 * PAPPER) * s > 900) s = 900 / (bredd + 2 * PAPPER);
    return s;
}

}  // namespace

// ---- kryssa i rutor -----------------------------------------------------------------------------

Svarsyta kryssaYta(const Data& d, const Variant& v, std::function<void(std::vector<std::string>)> vidSvar) {
    Svarsyta s;
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    s.widget = box;
    GtkWidget* wrap = adw_wrap_box_new();
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(wrap), 10);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(wrap), 10);
    auto rutor = std::make_shared<std::vector<GtkWidget*>>();
    int storlek = (int)(76 * FIGURSKALA);
    for (auto& alt : v.alternativ) {
        GtkWidget* cb = gtk_check_button_new();
        gtk_widget_add_css_class(cb, "kryssa");
        if (arBild(alt)) {
            gtk_check_button_set_child(GTK_CHECK_BUTTON(cb), sakbild(d, alt, storlek));
            gtk_widget_set_tooltip_text(cb, alt.substr(alt.find(':') + 1).c_str());
        } else {
            GtkWidget* l = gtk_label_new(alt.c_str());
            gtk_widget_add_css_class(l, "kryssa-text");
            gtk_check_button_set_child(GTK_CHECK_BUTTON(cb), l);
        }
        rutor->push_back(cb);
        adw_wrap_box_append(ADW_WRAP_BOX(wrap), cb);
    }
    gtk_box_append(GTK_BOX(box), wrap);
    gtk_box_append(GTK_BOX(box), etikett(tr("Tick every one that fits – there can be more than one.",
                                             "Kryssa i alla som stämmer – det kan vara flera.")));
    auto alt = std::make_shared<std::vector<std::string>>(v.alternativ);
    auto valda = [rutor, alt]() {
        std::vector<std::string> r;
        for (size_t i = 0; i < rutor->size(); ++i)
            if (gtk_check_button_get_active(GTK_CHECK_BUTTON((*rutor)[i]))) r.push_back((*alt)[i]);
        return r;
    };
    gtk_box_append(GTK_BOX(box), knapp(tr("Check", "Kolla"), true, [vidSvar, valda]() { vidSvar(valda()); }));
    s.svar = valda;
    s.visa = [rutor](const Bedomning& b, const std::vector<std::string>&) {
        for (size_t i = 0; i < rutor->size(); ++i) {
            GtkWidget* r = (*rutor)[i];
            gtk_widget_remove_css_class(r, "fel"); gtk_widget_remove_css_class(r, "ratt");
            if (std::find(b.felFalt.begin(), b.felFalt.end(), (int)i) != b.felFalt.end()) gtk_widget_add_css_class(r, "fel");
            else if (b.ratt && gtk_check_button_get_active(GTK_CHECK_BUTTON(r))) gtk_widget_add_css_class(r, "ratt");
        }
    };
    auto facit = std::make_shared<std::vector<std::string>>(v.facitSvar);
    s.visaFacit = [rutor, alt, facit]() {   // efter andra felet: ringa in de rätta
        std::set<std::string> ratt;
        for (auto& f : *facit) ratt.insert(textnorm(f));
        for (size_t i = 0; i < rutor->size(); ++i)
            if (ratt.count(textnorm((*alt)[i]))) gtk_widget_add_css_class((*rutor)[i], "facit");
    };
    s.fyll = [rutor, alt](const std::vector<std::string>& sv) {
        for (size_t i = 0; i < rutor->size(); ++i)
            gtk_check_button_set_active(GTK_CHECK_BUTTON((*rutor)[i]), std::find(sv.begin(), sv.end(), (*alt)[i]) != sv.end());
    };
    if (!rutor->empty()) s.fokus = rutor->front();
    return s;
}

// ---- dra linjer mellan par -----------------------------------------------------------------------

namespace {

struct Para {
    const Data* d = nullptr;
    std::vector<std::string> v, h, facit;      // vänster, höger (blandad), rätt höger för varje vänster
    std::vector<int> koppling;                 // vänster i → höger j, -1 = ingen linje
    std::set<int> fel;
    bool ratt = false, visaFacit = false;
    double bw = 140, bh = 52, gap = 12, mitt = 130, skala = 1;
    int fran = -1; bool franHoger = false, drar = false;
    int vantar = -1; bool vantarHoger = false;  // klickad men inte kopplad än (klicka-klicka går också)
    double sx = 0, sy = 0, px = 0, py = 0;

    double bredd() const { return 2 * bw + mitt; }
    double hojd() const { return std::max(v.size(), h.size()) * (bh + gap) - gap; }
    double y(int i) const { return i * (bh + gap); }
    // (sida, index): sida 0 = vänster, 1 = höger
    std::pair<int, int> traff(double x, double yy) const {
        for (int s = 0; s < 2; ++s) {
            double x0 = s ? bw + mitt : 0;
            const auto& l = s ? h : v;
            for (int i = 0; i < (int)l.size(); ++i)
                if (x >= x0 - 8 && x <= x0 + bw + 8 && yy >= y(i) && yy <= y(i) + bh) return {s, i};
        }
        return {-1, -1};
    }
    void koppla(int i, int j) {
        for (auto& k : koppling) if (k == j) k = -1;
        koppling[i] = j;
        fel.clear(); ratt = false;
    }
};

void ritaPara(GtkDrawingArea*, cairo_t* cr, int, int, gpointer p) {
    Para* s = static_cast<Para*>(p);
    cairo_scale(cr, s->skala, s->skala);
    papper(cr, s->bredd() + 2 * PAPPER, s->hojd() + 2 * PAPPER);
    cairo_translate(cr, PAPPER, PAPPER);
    std::string ac = accent();
    auto punktV = [&](int i) { return std::make_pair(s->bw, s->y(i) + s->bh / 2); };
    auto punktH = [&](int j) { return std::make_pair(s->bw + s->mitt, s->y(j) + s->bh / 2); };
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    if (s->visaFacit) {   // de rätta linjerna streckade under elevens
        double streck[] = {7, 6};
        for (size_t i = 0; i < s->v.size(); ++i)
            for (size_t j = 0; j < s->h.size(); ++j)
                if (textnorm(s->h[j]) == textnorm(s->facit[i])) {
                    auto [x1, y1] = punktV((int)i);
                    auto [x2, y2] = punktH((int)j);
                    cairo_set_dash(cr, streck, 2, 0);
                    tema::satt(cr, tema::GRON); cairo_set_line_width(cr, 3);
                    cairo_move_to(cr, x1, y1); cairo_line_to(cr, x2, y2); cairo_stroke(cr);
                    cairo_set_dash(cr, nullptr, 0, 0);
                    break;
                }
    }
    for (size_t i = 0; i < s->v.size(); ++i) {
        if (s->koppling[i] < 0) continue;
        auto [x1, y1] = punktV((int)i);
        auto [x2, y2] = punktH(s->koppling[i]);
        tema::satt(cr, s->ratt ? tema::GRON : s->fel.count((int)i) ? tema::ROD : ac);
        cairo_set_line_width(cr, 3.5);
        cairo_move_to(cr, x1, y1); cairo_line_to(cr, x2, y2); cairo_stroke(cr);
    }
    if (s->drar && s->fran >= 0) {
        auto [x1, y1] = s->franHoger ? punktH(s->fran) : punktV(s->fran);
        tema::satt(cr, ac, 0.8); cairo_set_line_width(cr, 3.5);
        cairo_move_to(cr, x1, y1); cairo_line_to(cr, s->px, s->py); cairo_stroke(cr);
    }
    for (int sida = 0; sida < 2; ++sida) {
        const auto& l = sida ? s->h : s->v;
        double x0 = sida ? s->bw + s->mitt : 0;
        for (int i = 0; i < (int)l.size(); ++i) {
            bool vald = s->vantar == i && s->vantarHoger == (bool)sida;
            rundad(cr, x0, s->y(i), s->bw, s->bh, 8);
            cairo_set_source_rgb(cr, 1, 1, 1); cairo_fill_preserve(cr);
            if (vald) { tema::satt(cr, ac); cairo_set_line_width(cr, 3.5); }
            else { tema::satt(cr, "#b9c1cb"); cairo_set_line_width(cr, 1.5); }
            cairo_stroke(cr);
            sak(cr, *s->d, x0 + 6, s->y(i) + 4, s->bw - 12, s->bh - 8, l[i]);
            double px = sida ? x0 : x0 + s->bw;   // prickarna man drar ifrån
            bool kopplad = sida ? std::count(s->koppling.begin(), s->koppling.end(), i) > 0 : s->koppling[i] >= 0;
            cairo_new_path(cr);   // texten lämnar en aktuell punkt – annars blir det ett streck till pricken
            cairo_arc(cr, px, s->y(i) + s->bh / 2, 7, 0, 2 * M_PI);
            tema::satt(cr, kopplad || vald ? ac : "#ffffff"); cairo_fill_preserve(cr);
            tema::satt(cr, kopplad || vald ? ac : "#8a939d"); cairo_set_line_width(cr, 2); cairo_stroke(cr);
        }
    }
}

}  // namespace

Svarsyta paraYta(const Data& d, const Variant& v, std::function<void(std::vector<std::string>)> vidSvar) {
    Svarsyta s;
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    s.widget = box;
    auto* p = new Para;
    p->d = &d; p->v = v.falt; p->h = v.alternativ; p->facit = v.facitSvar;
    p->koppling.assign(p->v.size(), -1);
    bool bilder = false;
    size_t langst = 1;
    for (auto* l : {&p->v, &p->h})
        for (auto& x : *l) { bilder |= arBild(x); if (!arBild(x)) langst = std::max(langst, (size_t)g_utf8_strlen(x.c_str(), -1)); }
    p->bh = bilder ? 76 : 52;
    p->bw = std::clamp(28.0 + langst * 11.5, bilder ? 84.0 : 96.0, 300.0);
    p->skala = skalaFor(p->bredd(), "");
    GtkWidget* w = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w), (int)std::ceil((p->bredd() + 2 * PAPPER) * p->skala));
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w), (int)std::ceil((p->hojd() + 2 * PAPPER) * p->skala));
    gtk_widget_set_halign(w, GTK_ALIGN_START);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w), ritaPara, p, [](gpointer x) { delete static_cast<Para*>(x); });
    gtk_widget_set_cursor_from_name(w, "pointer");
    GtkGesture* g = gtk_gesture_drag_new();
    g_signal_connect(g, "drag-begin", CB(+[](GtkGestureDrag*, double x, double y, gpointer d) {
        Para* s = static_cast<Para*>(d);
        x = x / s->skala - PAPPER; y = y / s->skala - PAPPER;
        auto [sida, i] = s->traff(x, y);
        s->fran = i; s->franHoger = sida == 1; s->sx = s->px = x; s->sy = s->py = y; s->drar = false;
    }), p);
    g_signal_connect(g, "drag-update", CB(+[](GtkGestureDrag* g, double dx, double dy, gpointer d) {
        Para* s = static_cast<Para*>(d);
        if (s->fran < 0) return;
        s->px = s->sx + dx / s->skala; s->py = s->sy + dy / s->skala;
        if (std::hypot(dx, dy) > 6) s->drar = true;
        gtk_widget_queue_draw(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g)));
    }), p);
    g_signal_connect(g, "drag-end", CB(+[](GtkGestureDrag* g, double dx, double dy, gpointer d) {
        Para* s = static_cast<Para*>(d);
        GtkWidget* w = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g));
        if (s->fran < 0) { s->vantar = -1; gtk_widget_queue_draw(w); return; }
        auto [sida, i] = s->traff(s->sx + dx / s->skala, s->sy + dy / s->skala);
        if (s->drar) {                                   // dragen linje
            if (i >= 0 && (sida == 1) != s->franHoger) s->koppla(s->franHoger ? i : s->fran, s->franHoger ? s->fran : i);
            else if (i < 0 && !s->franHoger) { s->koppling[s->fran] = -1; s->fel.clear(); }   // släppt i tomma luften: bort
            s->vantar = -1;
        } else if (s->vantar >= 0 && s->vantarHoger != s->franHoger) {   // klick, klick
            s->koppla(s->franHoger ? s->vantar : s->fran, s->franHoger ? s->fran : s->vantar);
            s->vantar = -1;
        } else {
            s->vantar = s->vantar == s->fran && s->vantarHoger == s->franHoger ? -1 : s->fran;
            s->vantarHoger = s->franHoger;
        }
        s->fran = -1; s->drar = false;
        gtk_widget_queue_draw(w);
    }), p);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(g));
    gtk_box_append(GTK_BOX(box), w);
    gtk_box_append(GTK_BOX(box), etikett(tr("Draw a line from each thing on the left to its partner on the right. Drag a line out to remove it.",
                                             "Dra en linje från varje sak till vänster till den som hör ihop med den. Dra ut en linje för att ta bort den.")));
    auto svar = [p]() {
        std::vector<std::string> r;
        for (int k : p->koppling) r.push_back(k >= 0 ? p->h[k] : "");
        return r;
    };
    GtkWidget* knappar = rad();
    gtk_box_append(GTK_BOX(knappar), knapp(tr("Check", "Kolla"), true, [vidSvar, svar]() { vidSvar(svar()); }));
    gtk_box_append(GTK_BOX(knappar), knapp(tr("Start over", "Börja om"), false, [p, w]() {
        std::fill(p->koppling.begin(), p->koppling.end(), -1);
        p->fel.clear(); p->ratt = false; p->vantar = -1;
        gtk_widget_queue_draw(w);
    }));
    gtk_box_append(GTK_BOX(box), knappar);
    s.svar = svar;
    s.visa = [p, w](const Bedomning& b, const std::vector<std::string>&) {
        p->fel = std::set<int>(b.felFalt.begin(), b.felFalt.end());
        p->ratt = b.ratt;
        gtk_widget_queue_draw(w);
    };
    s.visaFacit = [p, w]() { p->visaFacit = true; gtk_widget_queue_draw(w); };
    s.fyll = [p, w](const std::vector<std::string>& sv) {
        for (size_t i = 0; i < sv.size() && i < p->koppling.size(); ++i) {
            auto it = std::find(p->h.begin(), p->h.end(), sv[i]);
            p->koppling[i] = it == p->h.end() ? -1 : (int)(it - p->h.begin());
        }
        gtk_widget_queue_draw(w);
    };
    s.fokus = w;
    return s;
}

// ---- måla ytor --------------------------------------------------------------------------------------

namespace {

struct Mala {
    const Data* d = nullptr;
    Json besk;
    std::string typ, penna;                    // penna "" = suddet
    Figur fig;
    double skala = 1;
    std::vector<Ruta> celler;
    std::vector<std::string> farg;             // en per yta, "" = omålad
    std::set<int> fel;
    bool ratt = false;
    std::string satter;                        // vad det här draget gör: en färg eller "" (sudda)
    int senast = -1;
    double sx = 0, sy = 0;

    int traff(double x, double y) const {
        for (size_t i = 0; i < celler.size(); ++i) {
            const Ruta& r = celler[i];
            if (x >= r.x && x <= r.x + r.b && y >= r.y && y <= r.y + r.h) return (int)i;
        }
        return -1;
    }
};

void ritaMala(GtkDrawingArea*, cairo_t* cr, int, int, gpointer p) {
    Mala* s = static_cast<Mala*>(p);
    cairo_scale(cr, s->skala, s->skala);
    papper(cr, s->fig.bredd + 2 * PAPPER, s->fig.hojd + 2 * PAPPER);
    cairo_translate(cr, PAPPER, PAPPER);
    if (s->typ == "malrutor") {   // figuren målar själv (formen fylls, eller bakgrunden bakom tal och ikoner)
        Json b = s->besk;
        Json m = Json::lista();
        for (auto& f : s->farg) m.lagg(f);
        b.satt("malade", m);
        try { figur(*s->d, b).rita(cr, 0, 0); } catch (...) {}
    } else {
        s->fig.rita(cr, 0, 0);
        bool tackande = s->typ == "stapel" || s->typ == "staende" || s->typ == "tiostaplar" || s->typ == "hundraruta" || s->typ == "prickar";
        for (size_t i = 0; i < s->celler.size(); ++i) {
            if (s->farg[i].empty()) continue;
            const Ruta& r = s->celler[i];
            std::string f = pennfarg(s->farg[i]);
            if (tackande) {
                cairo_rectangle(cr, r.x + 1, r.y + 1, r.b - 2, r.h - 2);
                tema::satt(cr, f); cairo_fill_preserve(cr);
                tema::satt(cr, tema::LINJE); cairo_set_line_width(cr, 1.2); cairo_stroke(cr);
            } else if (s->typ == "scen") {
                cairo_save(cr);
                cairo_translate(cr, r.x + r.b / 2, r.y + r.h / 2);
                cairo_scale(cr, r.b / 2, r.h / 2);
                cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
                cairo_restore(cr);
                tema::satt(cr, f, 0.45); cairo_fill_preserve(cr);
                tema::satt(cr, f); cairo_set_line_width(cr, 3.5); cairo_stroke(cr);
            } else {
                rundad(cr, r.x, r.y, r.b, r.h, 6);
                tema::satt(cr, f, 0.5); cairo_fill_preserve(cr);
                tema::satt(cr, f); cairo_set_line_width(cr, 3); cairo_stroke(cr);
            }
        }
    }
    double streck[] = {6, 4};
    for (int i : s->fel) {
        if (i < 0 || i >= (int)s->celler.size()) continue;
        const Ruta& r = s->celler[i];
        cairo_set_dash(cr, streck, 2, 0);
        cairo_rectangle(cr, r.x - 2, r.y - 2, r.b + 4, r.h + 4);
        tema::satt(cr, tema::ROD); cairo_set_line_width(cr, 3); cairo_stroke(cr);
        cairo_set_dash(cr, nullptr, 0, 0);
    }
}

void malaVid(Mala* s, GtkWidget* w, double x, double y, bool forsta) {
    x = x / s->skala - PAPPER; y = y / s->skala - PAPPER;
    int i = s->traff(x, y);
    if (i < 0 || i == s->senast) return;
    if (forsta) s->satter = (!s->penna.empty() && s->farg[i] == s->penna) ? "" : s->penna;   // samma färg igen = sudda
    s->farg[i] = s->satter;
    s->senast = i;
    s->fel.clear(); s->ratt = false;
    gtk_widget_queue_draw(w);
}

}  // namespace

Svarsyta malaYta(const Data& d, const Variant& v, std::function<void(std::vector<std::string>)> vidSvar) {
    Svarsyta s;
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    s.widget = box;
    auto* m = new Mala;
    m->d = &d; m->besk = v.figur; m->typ = v.figur["typ"].str();
    try { m->fig = figur(d, v.figur); } catch (const std::exception& e) {
        g_warning("Matte: %s", e.what());
        m->fig = Figur{1, 1, [](cairo_t*, double, double) {}};
    }
    m->celler = celler(d, v.figur);
    m->farg.assign(m->celler.size(), "");
    m->skala = skalaFor(m->fig.bredd, m->typ);
    std::vector<std::string> pennor = v.figur["farger"].strangar();
    if (pennor.empty()) for (auto* f : FARGER) pennor.push_back(f);
    m->penna = pennor.front();

    GtkWidget* pennrad = rad(6);
    GtkToggleButton* grupp = nullptr;
    for (auto& f : pennor) {
        GtkWidget* b = pennknapp(f, false, grupp);
        if (!grupp) { grupp = GTK_TOGGLE_BUTTON(b); gtk_toggle_button_set_active(grupp, TRUE); }
        vidVal(b, [m, f]() { m->penna = f; });
        gtk_box_append(GTK_BOX(pennrad), b);
    }
    GtkWidget* sudd = pennknapp("", true, grupp);
    vidVal(sudd, [m]() { m->penna = ""; });
    gtk_box_append(GTK_BOX(pennrad), sudd);
    gtk_box_append(GTK_BOX(box), pennrad);

    GtkWidget* w = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w), (int)std::ceil((m->fig.bredd + 2 * PAPPER) * m->skala));
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w), (int)std::ceil((m->fig.hojd + 2 * PAPPER) * m->skala));
    gtk_widget_set_halign(w, GTK_ALIGN_START);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w), ritaMala, m, [](gpointer x) { delete static_cast<Mala*>(x); });
    gtk_widget_set_cursor_from_name(w, "crosshair");
    GtkGesture* g = gtk_gesture_drag_new();   // klicka på en yta, eller dra över flera
    g_signal_connect(g, "drag-begin", CB(+[](GtkGestureDrag* g, double x, double y, gpointer d) {
        Mala* s = static_cast<Mala*>(d);
        s->sx = x; s->sy = y; s->senast = -1;
        malaVid(s, gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g)), x, y, true);
    }), m);
    g_signal_connect(g, "drag-update", CB(+[](GtkGestureDrag* g, double dx, double dy, gpointer d) {
        Mala* s = static_cast<Mala*>(d);
        bool forsta = s->senast < 0;
        malaVid(s, gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g)), s->sx + dx, s->sy + dy, forsta);
    }), m);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(g));
    gtk_box_append(GTK_BOX(box), w);
    gtk_box_append(GTK_BOX(box), etikett(tr("Pick a pen and click the parts to colour them. Click again with the same colour to undo.",
                                             "Välj en penna och klicka på delarna för att måla dem. Klicka igen med samma färg för att ångra.")));
    auto svar = [m]() { return m->farg; };
    GtkWidget* knappar = rad();
    gtk_box_append(GTK_BOX(knappar), knapp(tr("Check", "Kolla"), true, [vidSvar, svar]() { vidSvar(svar()); }));
    gtk_box_append(GTK_BOX(knappar), knapp(tr("Start over", "Börja om"), false, [m, w]() {
        std::fill(m->farg.begin(), m->farg.end(), std::string());
        m->fel.clear(); m->ratt = false;
        gtk_widget_queue_draw(w);
    }));
    gtk_box_append(GTK_BOX(box), knappar);
    s.svar = svar;
    s.visa = [m, w](const Bedomning& b, const std::vector<std::string>&) {
        m->fel = std::set<int>(b.felFalt.begin(), b.felFalt.end());
        m->ratt = b.ratt;
        gtk_widget_queue_draw(w);
    };
    s.fyll = [m, w](const std::vector<std::string>& sv) {
        for (size_t i = 0; i < m->farg.size(); ++i) m->farg[i] = i < sv.size() ? sv[i] : "";
        gtk_widget_queue_draw(w);
    };
    s.fokus = w;
    return s;
}

// ---- fri rityta -------------------------------------------------------------------------------------

namespace {

struct Streck {
    std::string farg;
    double bredd = 4;
    bool sudd = false;
    std::vector<std::pair<double, double>> p;
};

struct Rityta {
    const Data* d = nullptr;
    bool harFig = false;
    Figur fig;
    double b = 640, h = 400;
    std::vector<Streck> streck;
    std::string farg = "svart";
    double bredd = 4;
    bool sudd = false;
    double sx = 0, sy = 0;

    void rita(cairo_t* cr) const {
        papper(cr, b, h);
        if (harFig) { cairo_save(cr); cairo_translate(cr, PAPPER, PAPPER); fig.rita(cr, 0, 0); cairo_restore(cr); }
        cairo_push_group(cr);   // strecken på ett eget lager – suddet tar bort streck, inte bakgrunden
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        for (auto& s : streck) {
            if (s.p.empty()) continue;
            cairo_set_operator(cr, s.sudd ? CAIRO_OPERATOR_CLEAR : CAIRO_OPERATOR_OVER);
            tema::satt(cr, pennfarg(s.farg));
            cairo_set_line_width(cr, s.sudd ? s.bredd * 3 : s.bredd);
            cairo_move_to(cr, s.p[0].first, s.p[0].second);
            if (s.p.size() == 1) cairo_line_to(cr, s.p[0].first + 0.1, s.p[0].second);
            for (size_t i = 1; i < s.p.size(); ++i) cairo_line_to(cr, s.p[i].first, s.p[i].second);
            cairo_stroke(cr);
        }
        cairo_pop_group_to_source(cr);
        cairo_paint(cr);
    }
};

}  // namespace

Svarsyta ritYta(const Data& d, const Variant& v, bool kompakt, std::function<void(std::vector<std::string>)> vidSvar) {
    Svarsyta s;
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    s.widget = box;
    auto* r = new Rityta;
    r->d = &d;
    double minB = kompakt ? 460 : 640, minH = kompakt ? 240 : 400;
    if (v.figur.arObjekt()) {
        try { r->fig = figur(d, v.figur); r->harFig = true; } catch (const std::exception& e) { g_warning("Matte: %s", e.what()); }
    }
    r->b = std::max(minB, r->harFig ? r->fig.bredd + 2 * PAPPER : 0);
    r->h = std::max(minH, r->harFig ? r->fig.hojd + 2 * PAPPER : 0);
    r->b = std::min(r->b, 900.0);

    GtkWidget* w = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w), (int)r->b);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w), (int)r->h);
    gtk_widget_set_halign(w, GTK_ALIGN_START);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w), +[](GtkDrawingArea*, cairo_t* cr, int, int, gpointer p) {
        static_cast<Rityta*>(p)->rita(cr);
    }, r, [](gpointer x) { delete static_cast<Rityta*>(x); });
    gtk_widget_set_cursor_from_name(w, "crosshair");
    GtkGesture* g = gtk_gesture_drag_new();
    g_signal_connect(g, "drag-begin", CB(+[](GtkGestureDrag* g, double x, double y, gpointer p) {
        Rityta* r = static_cast<Rityta*>(p);
        r->sx = x; r->sy = y;
        r->streck.push_back({r->farg, r->bredd, r->sudd, {{x, y}}});
        gtk_widget_queue_draw(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g)));
    }), r);
    g_signal_connect(g, "drag-update", CB(+[](GtkGestureDrag* g, double dx, double dy, gpointer p) {
        Rityta* r = static_cast<Rityta*>(p);
        if (r->streck.empty()) return;
        auto& pt = r->streck.back().p;
        std::pair<double, double> ny{r->sx + dx, r->sy + dy};
        if (!pt.empty() && std::hypot(ny.first - pt.back().first, ny.second - pt.back().second) < 1.5) return;   // inte varje pixel
        pt.push_back(ny);
        gtk_widget_queue_draw(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g)));
    }), r);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(g));

    // Verktygen: pennor, sudd, tjocklek, ångra
    GtkWidget* verktyg = adw_wrap_box_new();   // radbryts i ett smalt fönster
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(verktyg), 6);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(verktyg), 6);
    auto lagg = [verktyg](GtkWidget* w) { adw_wrap_box_append(ADW_WRAP_BOX(verktyg), w); };
    GtkToggleButton* grupp = nullptr;
    for (const char* f : {"svart", "röd", "blå", "grön", "gul", "orange", "lila", "brun"}) {
        GtkWidget* b = pennknapp(f, false, grupp);
        if (!grupp) { grupp = GTK_TOGGLE_BUTTON(b); gtk_toggle_button_set_active(grupp, TRUE); }
        std::string fs = f;
        vidVal(b, [r, fs]() { r->farg = fs; r->sudd = false; });
        lagg(b);
    }
    GtkWidget* sudd = pennknapp("", true, grupp);
    vidVal(sudd, [r]() { r->sudd = true; });
    lagg(sudd);
    lagg(gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    GtkToggleButton* tgrupp = nullptr;
    struct T { const char* en; const char* sv; double b; };
    for (T t : {T{"Thin", "Tunn", 2}, T{"Medium", "Mellan", 4.5}, T{"Thick", "Tjock", 10}}) {
        GtkWidget* b = gtk_toggle_button_new_with_label(tr(t.en, t.sv));
        if (tgrupp) gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(b), tgrupp);
        else tgrupp = GTK_TOGGLE_BUTTON(b);
        if (t.b == 4.5) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b), TRUE);
        double bredd = t.b;
        vidVal(b, [r, bredd]() { r->bredd = bredd; });
        lagg(b);
    }
    lagg(gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    GtkWidget* angra = knapp(tr("Undo", "Ångra"), false, [r, w]() { if (!r->streck.empty()) { r->streck.pop_back(); gtk_widget_queue_draw(w); } });
    lagg(angra);
    lagg(knapp(tr("Clear all", "Sudda allt"), false, [r, w]() { r->streck.clear(); gtk_widget_queue_draw(w); }));
    GtkWidget* rull = verktyg;

    if (kompakt) {
        gtk_box_append(GTK_BOX(box), etikett(tr("Feel free to draw here – it helps to see the problem. It is not graded.",
                                                 "Rita gärna här – det hjälper att se problemet. Det rättas inte.")));
        gtk_box_append(GTK_BOX(box), rull);
        gtk_box_append(GTK_BOX(box), w);
    } else {
        gtk_box_append(GTK_BOX(box), rull);
        gtk_box_append(GTK_BOX(box), w);
        gtk_box_append(GTK_BOX(box), etikett(tr("Your drawing is not graded. It is saved, and your teacher can see it.",
                                                 "Teckningen rättas inte. Den sparas, och din lärare kan titta på den.")));
        gtk_box_append(GTK_BOX(box), knapp(tr("Done", "Klar"), true, [vidSvar]() { vidSvar({}); }));
    }
    s.svar = []() { return std::vector<std::string>{}; };
    s.fyll = [r, w](const std::vector<std::string>&) {   // testkroken: en ritad cirkel och ett streck
        Streck c{"blå", 4.5, false, {}};
        for (int i = 0; i <= 40; ++i) c.p.push_back({200 + 70 * std::cos(i * M_PI / 20), 160 + 70 * std::sin(i * M_PI / 20)});
        r->streck.push_back(c);
        r->streck.push_back({"röd", 10, false, {{80, 300}, {400, 280}, {560, 320}}});
        gtk_widget_queue_draw(w);
    };
    s.tom = [r]() {
        for (auto& st : r->streck) if (!st.sudd) return false;
        return true;
    };
    s.spara = [r](const std::string& fil) {
        cairo_surface_t* y = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)r->b, (int)r->h);
        cairo_t* cr = cairo_create(y);
        r->rita(cr);
        cairo_destroy(cr);
        bool ok = cairo_surface_write_to_png(y, fil.c_str()) == CAIRO_STATUS_SUCCESS;
        cairo_surface_destroy(y);
        return ok;
    };
    s.fokus = w;
    return s;
}

}  // namespace matte
