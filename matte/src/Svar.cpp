#include "Svar.h"

#include "SvarNy.h"

#include "Tal.h"

#include <Wenuy.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>

// En figur som krymper till kolumnens bredd (behåller proportionerna) men aldrig blir större än sin
// naturliga storlek. Klicken räknas om med den skala figuren faktiskt ritas i.
G_DECLARE_FINAL_TYPE(MatteFigur, matte_figur, MATTE, FIGUR, GtkDrawingArea)
struct _MatteFigur { GtkDrawingArea parent; double b, h; };
G_DEFINE_FINAL_TYPE(MatteFigur, matte_figur, GTK_TYPE_DRAWING_AREA)
static void matte_figur_measure(GtkWidget* w, GtkOrientation o, int for_size, int* min, int* nat, int* minb, int* natb) {
    MatteFigur* f = MATTE_FIGUR(w);
    if (o == GTK_ORIENTATION_HORIZONTAL) {
        *nat = (int)std::ceil(f->b);
        *min = std::min(*nat, 140);
    } else {
        double s = for_size > 0 && for_size < f->b ? for_size / f->b : 1.0;
        *min = *nat = (int)std::ceil(f->h * s);
    }
    *minb = *natb = -1;
}
static GtkSizeRequestMode matte_figur_mode(GtkWidget*) { return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH; }
static void matte_figur_class_init(MatteFigurClass* k) {
    GTK_WIDGET_CLASS(k)->measure = matte_figur_measure;
    GTK_WIDGET_CLASS(k)->get_request_mode = matte_figur_mode;
}
static void matte_figur_init(MatteFigur* f) { f->b = f->h = 1; }

namespace matte {

double FIGURSKALA = 1.0;
static const double PAPPER = 10;      // marginal runt figuren på det vita pappret

namespace {

std::string accent() {
    GdkRGBA c;
    wenuy_theme_get_accent(wenuy_theme_get_default(), &c);
    char b[16];
    snprintf(b, sizeof b, "#%02x%02x%02x", (int)(c.red * 255), (int)(c.green * 255), (int)(c.blue * 255));
    return b;
}

void papper(cairo_t* cr, double b, double h) {
    double r = 8;
    cairo_new_sub_path(cr);
    cairo_arc(cr, b - r, r, r, -M_PI / 2, 0);
    cairo_arc(cr, b - r, h - r, r, 0, M_PI / 2);
    cairo_arc(cr, r, h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, r, r, r, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.12);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
}

struct Vy {
    const Data* d = nullptr;
    Json besk;
    std::string typ, lage;
    Figur fig;
    double skala = 1;
    double eff = 1;                                   // skalan figuren ritas i just nu (krympt till kolumnen)
    std::vector<Ruta> celler;
    std::vector<std::string> namn;
    std::unique_ptr<Rutnat> rutnat;
    std::set<int> markerade;
    std::vector<std::pair<double, double>> punkter;
    std::function<void()> vidKlick;
};

Vy* vyAv(GtkWidget* w) { return static_cast<Vy*>(g_object_get_data(G_OBJECT(w), "matte-vy")); }

void ritaVy(GtkDrawingArea*, cairo_t* cr, int bredd, int, gpointer data) {
    Vy* v = static_cast<Vy*>(data);
    double hel = (v->fig.bredd + 2 * PAPPER) * v->skala;
    v->eff = hel > bredd && bredd > 0 ? v->skala * bredd / hel : v->skala;
    cairo_scale(cr, v->eff, v->eff);
    papper(cr, v->fig.bredd + 2 * PAPPER, v->fig.hojd + 2 * PAPPER);
    cairo_translate(cr, PAPPER, PAPPER);
    v->fig.rita(cr, 0, 0);
    std::string ac = accent();
    for (int i : v->markerade) {
        if (i < 0 || i >= (int)v->celler.size()) continue;
        Ruta r = v->celler[i];
        if (v->typ == "scen") {   // en ring runt det man klickat på i målningen
            cairo_save(cr);
            cairo_translate(cr, r.x + r.b / 2, r.y + r.h / 2);
            cairo_scale(cr, r.b / 2, r.h / 2);
            cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
            cairo_restore(cr);
            tema::satt(cr, ac); cairo_set_line_width(cr, 4); cairo_stroke(cr);
            continue;
        }
        if (v->typ == "ikonval" || v->typ == "grupper") {
            cairo_rectangle(cr, r.x, r.y, r.b, r.h);
            tema::satt(cr, ac); cairo_set_line_width(cr, 4); cairo_stroke(cr);
            continue;
        }
        bool fargas = v->typ == "stapel" || v->typ == "staende" || v->typ == "tiostaplar" || v->typ == "hundraruta";
        cairo_rectangle(cr, r.x + 1.5, r.y + 1.5, r.b - 3, r.h - 3);
        if (fargas) { tema::satt(cr, tema::farg(v->besk["farg"].str("gul"))); cairo_fill_preserve(cr); }
        tema::satt(cr, tema::ACCENT); cairo_set_line_width(cr, 3); cairo_stroke(cr);
    }
    if (v->rutnat) {
        std::vector<std::pair<double, double>> pix;
        for (auto& p : v->punkter) pix.push_back(v->rutnat->pixel(p.first, p.second));
        if (v->lage == "linje" && pix.size() == 2) {
            auto [x1, y1] = pix[0];
            auto [x2, y2] = pix[1];
            tema::satt(cr, ac); cairo_set_line_width(cr, 2.5);
            const Rutnat& r = *v->rutnat;
            if (std::fabs(x1 - x2) > 0.5) {
                double k = (y2 - y1) / (x2 - x1), xa = r.marg, xb = r.marg + (r.xmax - r.xmin) * r.ruta;
                cairo_move_to(cr, xa, y1 + k * (xa - x1)); cairo_line_to(cr, xb, y1 + k * (xb - x1));
            } else {
                cairo_move_to(cr, x1, 6); cairo_line_to(cr, x1, 6 + (r.ymax - r.ymin) * r.ruta);
            }
            cairo_stroke(cr);
        }
        for (auto [x, y] : pix) {
            cairo_rectangle(cr, x - 6, y - 6, 12, 12);
            tema::satt(cr, ac); cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 1, 1, 1); cairo_set_line_width(cr, 1.5); cairo_stroke(cr);
        }
    }
}

void klickVy(GtkGestureClick* g, int, double x, double y, gpointer data) {
    Vy* v = static_cast<Vy*>(data);
    x = x / v->eff - PAPPER; y = y / v->eff - PAPPER;
    GtkWidget* w = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g));
    if (v->rutnat) {
        auto p = v->rutnat->snapp(x, y);
        if (v->lage == "punkt") v->punkter = {p};
        else {   // linje: två punkter, tredje klicket börjar om
            if (v->punkter.size() >= 2) v->punkter.clear();
            v->punkter.push_back(p);
            if (v->punkter.size() == 2 && v->punkter[0] == v->punkter[1]) v->punkter = {p};
        }
        gtk_widget_queue_draw(w);
        if (v->vidKlick) v->vidKlick();
        return;
    }
    for (size_t i = 0; i < v->celler.size(); ++i) {
        const Ruta& r = v->celler[i];
        if (x >= r.x && x <= r.x + r.b && y >= r.y && y <= r.y + r.h) {
            if (v->lage == "valj") v->markerade = {(int)i};            // bara en sak åt gången
            else if (v->markerade.count((int)i)) v->markerade.erase((int)i);
            else v->markerade.insert((int)i);
            gtk_widget_queue_draw(w);
            if (v->vidKlick) v->vidKlick();
            return;
        }
    }
}

}  // namespace

GtkWidget* figurvy(const Data& d, const Json& besk, const std::string& lage, double skala, std::function<void()> vidKlick) {
    auto* v = new Vy;
    v->d = &d; v->besk = besk; v->typ = besk["typ"].str(); v->lage = lage; v->vidKlick = std::move(vidKlick);
    GtkWidget* w = GTK_WIDGET(g_object_new(matte_figur_get_type(), nullptr));
    try {
        v->fig = figur(d, besk);
    } catch (const std::exception& e) {
        g_warning("Matte: %s", e.what());
        v->fig = Figur{1, 1, [](cairo_t*, double, double) {}};
    }
    v->skala = skala > 0 ? skala : FIGURSKALA;
    if (v->typ == "scen") v->skala = std::min(v->skala, 1.25);          // målningarna är redan stora
    double maxb = 1500;                                                    // krymper ändå till kolumnen
    if ((v->fig.bredd + 2 * PAPPER) * v->skala > maxb) v->skala = maxb / (v->fig.bredd + 2 * PAPPER);
    if (lage == "klick" || lage == "valj" || lage == "markera") v->celler = celler(d, besk);
    if (lage == "valj" || lage == "markera") v->namn = cellnamn(d, besk);
    if (lage == "punkt" || lage == "linje") v->rutnat = std::make_unique<Rutnat>(besk);
    v->eff = v->skala;
    MATTE_FIGUR(w)->b = (v->fig.bredd + 2 * PAPPER) * v->skala;
    MATTE_FIGUR(w)->h = (v->fig.hojd + 2 * PAPPER) * v->skala;
    gtk_widget_set_halign(w, GTK_ALIGN_START);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w), ritaVy, v, nullptr);
    g_object_set_data_full(G_OBJECT(w), "matte-vy", v, [](gpointer p) { delete static_cast<Vy*>(p); });
    if (!v->celler.empty() || v->rutnat) {
        GtkGesture* g = gtk_gesture_click_new();
        g_signal_connect(g, "pressed", G_CALLBACK(klickVy), v);
        gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(g));
        gtk_widget_set_cursor_from_name(w, "pointer");
    }
    return w;
}

std::vector<std::string> figurvySvar(GtkWidget* w) {
    Vy* v = vyAv(w);
    if (!v) return {};
    if (v->lage == "klick") return {std::to_string(v->markerade.size())};
    if (v->lage == "valj") return v->markerade.empty() ? std::vector<std::string>{} : std::vector<std::string>{v->namn[*v->markerade.begin()]};
    if (v->lage == "markera") {
        if (v->markerade.empty()) return {};
        std::string s;
        for (int i : v->markerade) s += (s.empty() ? "" : ",") + v->namn[i];
        return {s};
    }
    if (v->lage == "punkt") {
        if (v->punkter.empty()) return {};
        auto [x, y] = v->punkter[0];
        if (v->rutnat->typ == "tallinje") return {Brak::ungefar(x).text()};
        return {std::to_string((int)x), std::to_string((int)y)};
    }
    if (v->lage == "linje") {
        if (v->punkter.size() < 2) return {};
        auto [x1, y1] = v->punkter[0];
        auto [x2, y2] = v->punkter[1];
        if (x1 == x2) return {"lodrät", "lodrät"};
        Brak k((int64_t)(y2 - y1), (int64_t)(x2 - x1));
        Brak m = Brak((int64_t)y1) - k * Brak((int64_t)x1);
        return {k.text(), m.text()};
    }
    return {};
}

void figurvyRensa(GtkWidget* w) {
    if (Vy* v = vyAv(w)) { v->markerade.clear(); v->punkter.clear(); gtk_widget_queue_draw(w); }
}

// ---- dra brickor till tioramen ---------------------------------------------------------------------

namespace {

struct Drag {
    const Data* d;
    Json besk;
    int fyllda = 0, lagda = 0, hog = 10;
    double ruta = 52, skala = 1, startX = 0, startY = 0;
    bool drar = false, franRam = false;
    double dx = 0, dy = 0;
    double bredd() const { return 5 * ruta + 12; }
    double hogY() const { return 2 * ruta + 12 + 30; }
    bool iRam(double x, double y) const { return x >= 0 && x <= bredd() && y >= 0 && y <= 2 * ruta + 12; }
};

}  // namespace

static GtkWidget* dragyta(const Data& d, const Json& besk, std::function<std::string()>& svar) {
    auto* s = new Drag;
    s->d = &d; s->besk = besk;
    s->fyllda = besk["fyllda"].heltal(); s->ruta = besk["ruta"].tal(52); s->skala = FIGURSKALA;
    s->hog = 10 - s->fyllda;
    GtkWidget* w = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w), (int)((s->bredd() + 2 * PAPPER) * s->skala));
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w), (int)((s->hogY() + 2 * s->ruta + 2 * PAPPER) * s->skala));
    gtk_widget_set_halign(w, GTK_ALIGN_START);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w), +[](GtkDrawingArea*, cairo_t* cr, int, int, gpointer p) {
        Drag* s = static_cast<Drag*>(p);
        cairo_scale(cr, s->skala, s->skala);
        papper(cr, s->bredd() + 2 * PAPPER, s->hogY() + 2 * s->ruta + 2 * PAPPER);
        cairo_translate(cr, PAPPER, PAPPER);
        figur(*s->d, medFyllda(medFyllda(s->besk, s->fyllda), s->lagda, "extra")).rita(cr, 0, 0);
        double r = s->ruta * 0.38;
        std::string f2 = tema::farg(s->besk["farg2"].str("#f2c230"));
        auto bricka = [&](double cx, double cy) {
            cairo_arc(cr, cx, cy, r, 0, 2 * M_PI); tema::satt(cr, f2); cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 0, 0, 0, 0.2); cairo_set_line_width(cr, 2); cairo_stroke(cr);
        };
        for (int i = 0; i < s->hog - s->lagda - (s->drar && !s->franRam ? 1 : 0); ++i)
            // högen ligger som en egen tiorad (två rader om fem) – inga brickor överlappar, lätt att räkna
            bricka(6 + s->ruta / 2 + (i % 5) * s->ruta, s->hogY() + s->ruta / 2 + (i / 5) * s->ruta);
        if (s->drar) bricka(s->startX + s->dx, s->startY + s->dy);
    }, s, nullptr);
    g_object_set_data_full(G_OBJECT(w), "matte-drag", s, [](gpointer p) { delete static_cast<Drag*>(p); });
    GtkGesture* g = gtk_gesture_drag_new();
    g_signal_connect(g, "drag-begin", CB(+[](GtkGestureDrag* g, double x, double y, gpointer p) {
        Drag* s = static_cast<Drag*>(p);
        x = x / s->skala - PAPPER; y = y / s->skala - PAPPER;
        s->startX = x; s->startY = y; s->dx = s->dy = 0;
        if (y >= s->hogY() - 10 && s->hog - s->lagda > 0) { s->drar = true; s->franRam = false; }
        else if (s->iRam(x, y) && s->lagda > 0) { s->drar = true; s->franRam = true; s->lagda--; }
        gtk_widget_queue_draw(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g)));
    }), s);
    g_signal_connect(g, "drag-update", CB(+[](GtkGestureDrag* g, double dx, double dy, gpointer p) {
        Drag* s = static_cast<Drag*>(p);
        if (!s->drar) return;
        s->dx = dx / s->skala; s->dy = dy / s->skala;
        gtk_widget_queue_draw(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g)));
    }), s);
    g_signal_connect(g, "drag-end", CB(+[](GtkGestureDrag* g, double dx, double dy, gpointer p) {
        Drag* s = static_cast<Drag*>(p);
        if (!s->drar) return;
        double x = s->startX + dx / s->skala, y = s->startY + dy / s->skala;
        bool knack = std::fabs(dx) < 6 && std::fabs(dy) < 6;
        if (s->franRam) { if (s->iRam(x, y) && !knack) s->lagda++; }                     // släpptes i ramen igen – ångra
        else if ((s->iRam(x, y) || knack) && s->fyllda + s->lagda < 10) s->lagda++;     // dragen dit, eller knack på högen
        s->drar = false;
        gtk_widget_queue_draw(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g)));
    }), s);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(g));
    svar = [s]() { return std::to_string(s->lagda); };
    return w;
}

// ---- svarsytan för en uppgift --------------------------------------------------------------------------

bool figurVisasSeparat(const std::string& typ) {
    static const std::set<std::string> I_FIGUREN = {"klick", "farga", "punkt", "linje", "valj", "markera", "dra", "mala", "rita", "skiss"};
    return !I_FIGUREN.count(typ);
}

namespace {

GtkWidget* etikett(const char* text, const char* klass = "svag") {
    GtkWidget* l = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_wrap(GTK_LABEL(l), TRUE);
    if (klass) gtk_widget_add_css_class(l, klass);
    return l;
}

GtkWidget* knapp(const char* text, bool huvud, std::function<void()> f) {
    GtkWidget* b = gtk_button_new_with_label(text);
    if (huvud) gtk_widget_add_css_class(b, "suggested-action");
    gtk_widget_set_halign(b, GTK_ALIGN_START);
    auto* fp = new std::function<void()>(std::move(f));
    g_signal_connect_data(b, "clicked", CB(+[](GtkButton*, gpointer p) { (*static_cast<std::function<void()>*>(p))(); }), fp,
                          +[](gpointer p, GClosure*) { delete static_cast<std::function<void()>*>(p); }, (GConnectFlags)0);
    return b;
}

void vidEnter(GtkWidget* entry, std::function<void()> f) {
    auto* fp = new std::function<void()>(std::move(f));
    g_signal_connect_data(entry, "activate", CB(+[](GtkEntry*, gpointer p) { (*static_cast<std::function<void()>*>(p))(); }), fp,
                          +[](gpointer p, GClosure*) { delete static_cast<std::function<void()>*>(p); }, (GConnectFlags)0);
}

}  // namespace

Svarsyta svarsyta(const Data& d, const Mall& m, const Variant& v, std::function<void(std::vector<std::string>)> vidSvar) {
    using wenuy::tr;
    Svarsyta s;
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    s.widget = box;
    auto falt = std::make_shared<std::vector<GtkWidget*>>();

    if (m.typ == "kryssa") return kryssaYta(d, v, vidSvar);
    if (m.typ == "para") return paraYta(d, v, vidSvar);
    if (m.typ == "mala" && v.figur.arObjekt()) return malaYta(d, v, vidSvar);
    if (m.typ == "rita") return ritYta(d, v, false, vidSvar);

    if (m.typ == "val") {
        GtkWidget* rad = adw_wrap_box_new();
        adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(rad), 8);
        adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(rad), 8);
        auto knappar = std::make_shared<std::vector<GtkWidget*>>();
        for (auto& alt : v.alternativ) {
            GtkWidget* b = knapp(alt.c_str(), false, [vidSvar, alt]() { vidSvar({alt}); });
            gtk_widget_add_css_class(b, "val");
            knappar->push_back(b);
            adw_wrap_box_append(ADW_WRAP_BOX(rad), b);
        }
        gtk_box_append(GTK_BOX(box), rad);
        s.svar = []() { return std::vector<std::string>{}; };
        s.visa = [knappar](const Bedomning& b, const std::vector<std::string>& svar) {
            for (auto* k : *knappar) {
                gtk_widget_remove_css_class(k, "ratt"); gtk_widget_remove_css_class(k, "fel");
                if (!svar.empty() && svar[0] == gtk_button_get_label(GTK_BUTTON(k))) gtk_widget_add_css_class(k, b.ratt ? "ratt" : "fel");
            }
        };
        if (!knappar->empty()) s.fokus = knappar->front();
        return s;
    }

    if (m.typ == "tabell" || ((m.typ == "punkt" || m.typ == "linje") && !v.figur.arObjekt())) {
        GtkWidget* grid = gtk_grid_new();
        gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
        gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
        auto lasa = [falt]() { std::vector<std::string> r; for (auto* e : *falt) r.push_back(gtk_editable_get_text(GTK_EDITABLE(e))); return r; };
        for (size_t i = 0; i < v.falt.size(); ++i) {
            gtk_grid_attach(GTK_GRID(grid), etikett(v.falt[i].c_str(), "rubrik"), (int)i, 0, 1, 1);
            GtkWidget* e = gtk_entry_new();
            gtk_editable_set_width_chars(GTK_EDITABLE(e), 10);
            gtk_widget_add_css_class(e, "svarsfalt");
            vidEnter(e, [vidSvar, lasa]() { vidSvar(lasa()); });
            gtk_grid_attach(GTK_GRID(grid), e, (int)i, 1, 1, 1);
            falt->push_back(e);
        }
        gtk_box_append(GTK_BOX(box), grid);
        gtk_box_append(GTK_BOX(box), knapp(tr("Check", "Kolla"), true, [vidSvar, lasa]() { vidSvar(lasa()); }));
        s.svar = lasa;
    } else if (m.typ == "dra" && v.figur.arObjekt()) {
        std::function<std::string()> antal;
        gtk_box_append(GTK_BOX(box), dragyta(d, v.figur, antal));
        gtk_box_append(GTK_BOX(box), etikett(tr("Drag the counters into the frame. Drag one out to undo.", "Dra brickorna till ramen. Dra ut en för att ångra.")));
        gtk_box_append(GTK_BOX(box), knapp(tr("Done", "Klar"), true, [vidSvar, antal]() { vidSvar({antal()}); }));
        s.svar = [antal]() { return std::vector<std::string>{antal()}; };
    } else if ((m.typ == "valj" || m.typ == "markera" || m.typ == "klick" || m.typ == "farga" || m.typ == "punkt" || m.typ == "linje") && v.figur.arObjekt()) {
        std::string lage = m.typ == "farga" ? "klick" : m.typ;
        auto vy = std::make_shared<GtkWidget*>(nullptr);
        GtkWidget* f = figurvy(d, v.figur, lage, 0, lage == "valj" ? std::function<void()>([vidSvar, vy]() {
            auto sv = figurvySvar(*vy);   // klicka på en sak = svaret (små barn behöver ingen extra knapp)
            if (!sv.empty()) vidSvar(sv);
        }) : nullptr);
        *vy = f;
        gtk_box_append(GTK_BOX(box), f);
        const char* tips = m.typ == "valj" ? tr("Click on your answer in the picture.", "Klicka på svaret i bilden.")
                         : m.typ == "markera" ? tr("Click on everything that fits. Click again to undo.", "Klicka på allt som passar. Klicka igen för att ångra.")
                         : m.typ == "punkt" ? tr("Click where the answer should be.", "Klicka där svaret ska vara.")
                         : m.typ == "linje" ? tr("Click two points the line passes through.", "Klicka på två punkter som linjen går genom.")
                         : tr("Click the squares. Click again to undo.", "Klicka på rutorna. Klicka igen för att ångra.");
        gtk_box_append(GTK_BOX(box), etikett(tips));
        if (m.typ != "valj")
            gtk_box_append(GTK_BOX(box), knapp(tr("Check", "Kolla"), true, [vidSvar, f]() {
                auto sv = figurvySvar(f);
                if (!sv.empty()) vidSvar(sv);
            }));
        s.svar = [f]() { return figurvySvar(f); };
    } else if (m.typ == "skiss") {   // en rityta att tänka med (figuren som bakgrund) plus ett vanligt svar
        Svarsyta rit = ritYta(d, v, true, nullptr);
        gtk_box_append(GTK_BOX(box), rit.widget);
        GtkWidget* rad = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget* e = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(e), tr("Answer …", "Svar …"));
        gtk_widget_set_hexpand(e, TRUE);
        gtk_widget_add_css_class(e, "svarsfalt");
        falt->push_back(e);
        vidEnter(e, [vidSvar, e]() { vidSvar({gtk_editable_get_text(GTK_EDITABLE(e))}); });
        gtk_box_append(GTK_BOX(rad), e);
        gtk_box_append(GTK_BOX(rad), knapp(tr("Check", "Kolla"), true, [vidSvar, e]() { vidSvar({gtk_editable_get_text(GTK_EDITABLE(e))}); }));
        gtk_box_append(GTK_BOX(box), rad);
        s.svar = [e]() { return std::vector<std::string>{gtk_editable_get_text(GTK_EDITABLE(e))}; };
    } else {
        GtkWidget* rad = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget* e = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(e), m.typ == "uttryck" ? tr("e.g. 5x + 3", "t.ex. 5x + 3") : tr("Type the answer …", "Skriv svaret …"));
        gtk_widget_set_hexpand(e, TRUE);
        gtk_widget_add_css_class(e, "svarsfalt");
        falt->push_back(e);
        vidEnter(e, [vidSvar, e]() { vidSvar({gtk_editable_get_text(GTK_EDITABLE(e))}); });
        gtk_box_append(GTK_BOX(rad), e);
        gtk_box_append(GTK_BOX(rad), knapp(tr("Check", "Kolla"), true, [vidSvar, e]() { vidSvar({gtk_editable_get_text(GTK_EDITABLE(e))}); }));
        gtk_box_append(GTK_BOX(box), rad);
        s.svar = [e]() { return std::vector<std::string>{gtk_editable_get_text(GTK_EDITABLE(e))}; };
    }
    s.visa = [falt](const Bedomning& b, const std::vector<std::string>&) {
        for (size_t i = 0; i < falt->size(); ++i) {
            gtk_widget_remove_css_class((*falt)[i], "fel");
            if (std::find(b.felFalt.begin(), b.felFalt.end(), (int)i) != b.felFalt.end()) gtk_widget_add_css_class((*falt)[i], "fel");
        }
    };
    if (!falt->empty()) s.fokus = falt->front();
    return s;
}

}  // namespace matte
