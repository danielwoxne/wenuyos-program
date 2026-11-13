// Räknepappret – se Papper.h
#include "Papper.h"
#include "Penntryck.h"

#include <Wenuy.h>
#include <adwaita.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <regex>
#include <set>
#include <vector>

namespace matte {
namespace {

using wenuy::tr;

using Cell = PapperCell;
using Streck = PapperStreck;
using Pennpunkt = PapperPunkt;

struct Papper : PapperData {
    GtkWidget* w = nullptr;
    double zoom = 1;
    int rader = 10;
    std::string uppgift;
    GtkWidget *lageK[3] = {}, *litenK = nullptr, *vansterK = nullptr, *stallK = nullptr;
    bool synkar = false;
    bool stylusAktiv = false;
    bool varnadOmGrans = false;
    size_t antalPunkter = 0;
    std::function<void()> andrad, begransad;
    double ruta() const { return 34 * zoom; }
};

Papper* av(GtkWidget* w) { return w ? static_cast<Papper*>(g_object_get_data(G_OBJECT(w), "matte-papper")) : nullptr; }

// Sparning körs först efter avslutat pennstreck eller tangentändring, inte
// för varje rörelsepunkt. Därmed belastas 2 GB-datorn inte med upprepad JSON.
void markeraAndring(Papper* p) {
    if (p && p->andrad) p->andrad();
}
bool kanLaggaTill(Papper* p, bool nyttStreck) {
    constexpr size_t MAX_UI_PUNKTER = 16000; // JSON-filen kan alltid hålla sig under 2 MiB
    if (p->antalPunkter < MAX_UI_PUNKTER &&
        (!nyttStreck || p->drag.size() < PAPPER_MAX_STRECK)) return true;
    if (!p->varnadOmGrans) {
        p->varnadOmGrans = true;
        if (p->begransad) p->begransad();
    }
    return false;
}


void uppdatera(Papper* p) {
    int maxRad = std::max(p->rad, 0);
    for (auto& [k, v] : p->c) maxRad = std::max(maxRad, k.first);
    for (auto& s : p->streck) maxRad = std::max(maxRad, s.rad);
    for (auto& d : p->drag) for (auto& pt : d) maxRad = std::max(maxRad, (int)pt.second);
    p->rader = std::max(8, maxRad + 3);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(p->w), (int)(p->rader * p->ruta()) + 2);
    gtk_widget_queue_draw(p->w);
    // knapparna följer läget (utan att deras signaler ändrar tillbaka)
    p->synkar = true;
    for (int i = 0; i < 3; ++i) if (p->lageK[i]) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->lageK[i]), p->lage == i);
    if (p->litenK) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->litenK), p->liten);
    if (p->vansterK) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->vansterK), p->vanster);
    p->synkar = false;
}

void text(cairo_t* cr, const std::string& t, double x, double y, double b, double h, double storlek, bool fet) {
    PangoLayout* l = pango_cairo_create_layout(cr);
    PangoFontDescription* f = pango_font_description_from_string(fet ? "Sans Medium" : "Sans");
    pango_font_description_set_absolute_size(f, storlek * PANGO_SCALE);
    pango_layout_set_font_description(l, f);
    PangoAttrList* a = pango_attr_list_new();
    pango_attr_list_insert(a, pango_attr_font_features_new("tnum"));
    pango_layout_set_attributes(l, a);
    pango_attr_list_unref(a);
    pango_layout_set_text(l, t.c_str(), -1);
    int lb, lh;
    pango_layout_get_pixel_size(l, &lb, &lh);
    cairo_move_to(cr, x + (b - lb) / 2.0, y + (h - lh) / 2.0);
    pango_cairo_show_layout(cr, l);
    pango_font_description_free(f);
    g_object_unref(l);
}

void rita(GtkDrawingArea* da, cairo_t* cr, int b, int h, gpointer data) {
    Papper* p = static_cast<Papper*>(data);
    double r = p->ruta();
    // papperet: vitt, rundade hörn, ljusblå rutor (som ett mattehäfte – syns i ljust och mörkt tema)
    double rr = 8;
    cairo_new_sub_path(cr);
    cairo_arc(cr, b - rr, rr, rr, -M_PI / 2, 0);
    cairo_arc(cr, b - rr, h - rr, rr, 0, M_PI / 2);
    cairo_arc(cr, rr, h - rr, rr, M_PI / 2, M_PI);
    cairo_arc(cr, rr, rr, rr, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_fill_preserve(cr);
    cairo_save(cr);
    cairo_clip(cr);
    cairo_set_source_rgba(cr, 0.36, 0.56, 0.86, 0.35);
    cairo_set_line_width(cr, 1);
    for (double x = r; x < b; x += r) { cairo_move_to(cr, std::floor(x) + 0.5, 0); cairo_line_to(cr, std::floor(x) + 0.5, h); }
    for (double y = r; y < h; y += r) { cairo_move_to(cr, 0, std::floor(y) + 0.5); cairo_line_to(cr, b, std::floor(y) + 0.5); }
    cairo_stroke(cr);
    const double blk[3] = {0.11, 0.17, 0.33};   // bläck
    // markören
    bool fokus = gtk_widget_has_focus(GTK_WIDGET(da));
    if (fokus && p->lage == 0) {
        GdkRGBA ac;
        wenuy_theme_get_accent(wenuy_theme_get_default(), &ac);
        cairo_set_source_rgba(cr, ac.red, ac.green, ac.blue, 0.14);
        cairo_rectangle(cr, p->kol * r, p->rad * r, r, r);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, ac.red, ac.green, ac.blue, 1);
        cairo_set_line_width(cr, 2.5);
        if (p->liten) cairo_rectangle(cr, p->kol * r + r * 0.55, p->rad * r + 2, r * 0.42, r * 0.42);
        else cairo_rectangle(cr, p->kol * r + 1.5, p->rad * r + 1.5, r - 3, r - 3);
        cairo_stroke(cr);
        if (p->vanster) {   // en liten pil: nästa siffra hamnar till vänster
            cairo_move_to(cr, p->kol * r + 4, p->rad * r + r - 6);
            cairo_line_to(cr, p->kol * r + 10, p->rad * r + r - 9);
            cairo_line_to(cr, p->kol * r + 10, p->rad * r + r - 3);
            cairo_close_path(cr);
            cairo_fill(cr);
        }
    }
    cairo_set_source_rgb(cr, blk[0], blk[1], blk[2]);
    for (auto& [k, v] : p->c) {
        double x = k.second * r, y = k.first * r;
        if (!v.t.empty()) text(cr, v.t == "-" ? "−" : v.t, x, y, r, r, r * 0.6, true);
        if (!v.liten.empty()) {
            cairo_set_source_rgb(cr, 0.75, 0.11, 0.16);   // minnessiffror i rött, som med en rödpenna
            text(cr, v.liten, x + r * 0.55, y + 1, r * 0.42, r * 0.42, r * 0.3, true);
            cairo_set_source_rgb(cr, blk[0], blk[1], blk[2]);
        }
    }
    cairo_set_line_width(cr, 2.4);
    for (auto& s : p->streck) {
        cairo_move_to(cr, s.fran * r + 2, (s.rad + 1) * r);
        cairo_line_to(cr, (s.till + 1) * r - 2, (s.rad + 1) * r);
    }
    cairo_stroke(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    const double basBredd = std::max(2.0, r / 13);
    for (auto& d : p->drag) {
        if (d.empty()) continue;
        if (d.size() == 1) {
            cairo_arc(cr, d[0].first * r, d[0].second * r,
                pennbredd(basBredd, d[0].tryck, d[0].tryckFinns) / 2.0, 0, 2 * M_PI);
            cairo_fill(cr);
            continue;
        }
        for (size_t i = 1; i < d.size(); ++i) {
            const auto& prev = d[i - 1];
            const auto& pt = d[i];
            cairo_set_line_width(cr, pennbredd(basBredd, (prev.tryck + pt.tryck) * 0.5,
                                              prev.tryckFinns && pt.tryckFinns));
            cairo_move_to(cr, prev.first * r, prev.second * r);
            cairo_line_to(cr, pt.first * r, pt.second * r);
            cairo_stroke(cr);
        }
    }
    cairo_restore(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.12);
    cairo_set_line_width(cr, 1);
    cairo_new_sub_path(cr);
    cairo_arc(cr, b - rr, rr, rr, -M_PI / 2, 0);
    cairo_arc(cr, b - rr, h - rr, rr, 0, M_PI / 2);
    cairo_arc(cr, rr, h - rr, rr, M_PI / 2, M_PI);
    cairo_arc(cr, rr, rr, rr, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
    cairo_stroke(cr);
}

int kolumner(Papper* p) { return std::max(4, (int)(gtk_widget_get_width(p->w) / p->ruta())); }

void flytta(Papper* p, int drad, int dkol) {
    p->rad = std::max(0, p->rad + drad);
    p->kol = std::clamp(p->kol + dkol, 0, kolumner(p) - 1);
}

void sudda(Papper* p) {
    if (p->vanster) { p->kol = std::min(p->kol + 1, kolumner(p) - 1); }
    else if (p->kol > 0) --p->kol;
    p->c.erase({p->rad, p->kol});
}

void skrivTecken(Papper* p, std::string t) {
    if (t == "*" || t == "x" || t == "×") t = "·";
    if (p->lage != 0) p->lage = 0;   // att skriva betyder skriva
    Cell& cell = p->c[{p->rad, p->kol}];
    if (p->liten) { cell.liten = t; p->liten = false; }
    else {
        cell.t = t;
        if (p->vanster) { if (p->kol > 0) --p->kol; }
        else p->kol = std::min(p->kol + 1, kolumner(p) - 1);
    }
    if (cell.t.empty() && cell.liten.empty()) p->c.erase({p->rad, p->kol});
}

void strecka(Papper* p) {   // summastrecket under raden: från första till sista ifyllda rutan
    int fran = 1 << 20, till = -1;
    for (auto& [k, v] : p->c) if (k.first == p->rad && !v.t.empty()) { fran = std::min(fran, k.second); till = std::max(till, k.second); }
    if (till < 0) { fran = p->kol; till = p->kol; }
    for (auto it = p->streck.begin(); it != p->streck.end(); ++it)
        if (it->rad == p->rad) { p->streck.erase(it); return; }   // igen = bort
    p->streck.push_back({p->rad, fran, till});
}

gboolean tangent(GtkEventControllerKey*, guint key, guint, GdkModifierType mod, gpointer data) {
    Papper* p = static_cast<Papper*>(data);
    bool ctrl = mod & GDK_CONTROL_MASK;
    switch (key) {
        case GDK_KEY_Left: flytta(p, 0, -1); break;
        case GDK_KEY_Right: flytta(p, 0, 1); break;
        case GDK_KEY_Up: flytta(p, -1, 0); break;
        case GDK_KEY_Down: flytta(p, 1, 0); break;
        case GDK_KEY_Home: p->kol = p->start; break;
        case GDK_KEY_Return: case GDK_KEY_KP_Enter: p->rad++; p->kol = p->start; break;
        case GDK_KEY_BackSpace: sudda(p); break;
        case GDK_KEY_Delete: p->c.erase({p->rad, p->kol}); break;
        default: {
            if (ctrl && key >= GDK_KEY_0 && key <= GDK_KEY_9) {   // Ctrl+siffra: minnessiffra
                p->c[{p->rad, p->kol}].liten = std::string(1, (char)('0' + (key - GDK_KEY_0)));
                break;
            }
            if (ctrl || (mod & GDK_ALT_MASK)) return FALSE;
            gunichar u = gdk_keyval_to_unicode(key);
            if (!u || !g_unichar_isprint(u) || u == ' ') {
                if (u == ' ') { p->c.erase({p->rad, p->kol}); flytta(p, 0, p->vanster ? -1 : 1); break; }
                return FALSE;
            }
            if (u == '_') { strecka(p); break; }
            char b[8] = {};
            g_unichar_to_utf8(u, b);
            skrivTecken(p, b);
        }
    }
    uppdatera(p);
    markeraAndring(p);
    return TRUE;
}

void klick(GtkGestureClick* g, int, double x, double y, gpointer data) {
    Papper* p = static_cast<Papper*>(data);
    gtk_widget_grab_focus(p->w);
    if (p->lage != 0) return;
    p->rad = (int)(y / p->ruta());
    p->kol = std::clamp((int)(x / p->ruta()), 0, kolumner(p) - 1);
    p->start = p->kol;
    gtk_gesture_set_state(GTK_GESTURE(g), GTK_EVENT_SEQUENCE_CLAIMED);
    uppdatera(p);
}

void suddaVid(Papper* p, double x, double y) {
    double cx = x / p->ruta(), cy = y / p->ruta();
    p->c.erase({(int)cy, (int)cx});
    p->drag.erase(std::remove_if(p->drag.begin(), p->drag.end(), [&](auto& d) {
        for (auto& pt : d) if (std::hypot(pt.first - cx, pt.second - cy) < 0.4) {
            p->antalPunkter -= std::min(p->antalPunkter, d.size());
            return true;
        }
        return false;
    }), p->drag.end());
}

bool arStylus(GdkDevice* d) {
    if (!d) return false;
    auto src = gdk_device_get_source(d);
    return src == GDK_SOURCE_PEN;  // GTK4 distinguishes the eraser via GdkDeviceTool, not device source
}

void dragBorjar(GtkGestureDrag* g, double x, double y, gpointer data) {
    Papper* p = static_cast<Papper*>(data);
    if (arStylus(gtk_event_controller_get_current_event_device(GTK_EVENT_CONTROLLER(g)))) return;
    gtk_widget_grab_focus(p->w);
    if (p->lage == 1) {
        if (!kanLaggaTill(p, true)) return;
        p->drag.push_back({{x / p->ruta(), y / p->ruta()}}); ++p->antalPunkter;
    } else if (p->lage == 2) suddaVid(p, x, y);
    else return;
    uppdatera(p);
}

void dragFlyttas(GtkGestureDrag* g, double dx, double dy, gpointer data) {
    Papper* p = static_cast<Papper*>(data);
    if (arStylus(gtk_event_controller_get_current_event_device(GTK_EVENT_CONTROLLER(g)))) return;
    double sx, sy;
    if (p->lage == 0 || !gtk_gesture_drag_get_start_point(g, &sx, &sy)) return;
    double x = sx + dx, y = sy + dy;
    if (p->lage == 1 && !p->drag.empty()) {
        if (kanLaggaTill(p, false)) {
            p->drag.back().push_back({x / p->ruta(), y / p->ruta()});
            ++p->antalPunkter;
        }
    } else if (p->lage == 2) suddaVid(p, x, y);
    gtk_widget_queue_draw(p->w);
}

void dragSlut(GtkGestureDrag* g, double, double, gpointer data) {
    if (arStylus(gtk_event_controller_get_current_event_device(GTK_EVENT_CONTROLLER(g)))) return;
    Papper* p = static_cast<Papper*>(data);
    uppdatera(p);
    markeraAndring(p);
}

// GTK4 stylus events are separate from generic mouse/touch drag events.
void stylusNer(GtkGestureStylus* g, double x, double y, gpointer data) {
    Papper* p = static_cast<Papper*>(data);
    gtk_widget_grab_focus(p->w);
    p->stylusAktiv = true;
    if (p->lage == 0) p->lage = 1;
    double pressure = 1.0;
    bool has = gtk_gesture_stylus_get_axis(g, GDK_AXIS_PRESSURE, &pressure);
    if (p->lage == 1) {
        if (kanLaggaTill(p, true)) {
            p->drag.push_back({{x / p->ruta(), y / p->ruta(), penntryck(pressure, has), has}});
            ++p->antalPunkter;
        }
    } else if (p->lage == 2) suddaVid(p, x, y);
    gtk_gesture_set_state(GTK_GESTURE(g), GTK_EVENT_SEQUENCE_CLAIMED);
    uppdatera(p);
}
void stylusRor(GtkGestureStylus* g, double x, double y, gpointer data) {
    Papper* p = static_cast<Papper*>(data);
    if (!p->stylusAktiv) return;
    double pressure = 1.0;
    bool has = gtk_gesture_stylus_get_axis(g, GDK_AXIS_PRESSURE, &pressure);
    if (p->lage == 1 && !p->drag.empty()) {
        auto& points = p->drag.back();
        const double px = x / p->ruta(), py = y / p->ruta();
        if (points.size() >= 10000 || !kanLaggaTill(p, false)) return;
        if (!points.empty() && std::hypot(points.back().first - px, points.back().second - py) < .004) return;
        points.push_back({px, py, penntryck(pressure, has), has});
        ++p->antalPunkter;
    } else if (p->lage == 2) suddaVid(p, x, y);
    gtk_widget_queue_draw(p->w);
}
void stylusUpp(GtkGestureStylus*, double, double, gpointer data) {
    Papper* p = static_cast<Papper*>(data);
    p->stylusAktiv = false;
    uppdatera(p);
    markeraAndring(p);
}

GtkWidget* knapp(GtkWidget* rad, const char* text, const char* tips, bool toggle) {
    GtkWidget* b = toggle ? gtk_toggle_button_new_with_label(text) : gtk_button_new_with_label(text);
    gtk_widget_add_css_class(b, "flat");
    gtk_widget_set_focus_on_click(b, FALSE);   // fokus stannar i pappret
    if (tips) gtk_widget_set_tooltip_text(b, tips);
    adw_wrap_box_append(ADW_WRAP_BOX(rad), b);
    return b;
}

struct Upp { std::string a, op, b; };
bool tolka(const std::string& text, Upp* u) {
    static const std::regex re(R"((\d+(?:,\d+)?)\s*(\+|-|−|·|×|\*|⋅)\s*(\d+(?:,\d+)?))");
    // inte i bråk (8/10 − 4/10 är inget att ställa upp) och inte mitt i ett längre tal
    for (auto it = std::sregex_iterator(text.begin(), text.end(), re); it != std::sregex_iterator(); ++it) {
        const std::smatch& m = *it;
        size_t fore = m.position(0), efter = m.position(0) + m.length(0);
        if (fore > 0 && (text[fore - 1] == '/' || std::isdigit((unsigned char)text[fore - 1]) || text[fore - 1] == ',')) continue;
        if (efter < text.size() && (text[efter] == '/' || text[efter] == '^' || std::isalpha((unsigned char)text[efter]))) continue;
        if (u) {
            u->a = m[1]; u->b = m[3];
            std::string op = m[2];
            u->op = op == "+" ? "+" : (op == "-" || op == "−") ? "−" : "·";
        }
        return true;
    }
    return false;
}

}  // namespace

GtkWidget* raknepapper_new() {
    auto* p = new Papper;
    GtkWidget* w = gtk_drawing_area_new();
    p->w = w;
    g_object_set_data_full(G_OBJECT(w), "matte-papper", p, [](gpointer d) { delete static_cast<Papper*>(d); });
    gtk_widget_add_css_class(w, "matte-papper");
    gtk_widget_set_focusable(w, TRUE);
    gtk_widget_set_hexpand(w, TRUE);
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w), 200);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w), rita, p, nullptr);
    gtk_accessible_update_property(GTK_ACCESSIBLE(w), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   tr("Working paper – type one digit per square", "Räknepapper – skriv en siffra per ruta"), -1);
    GtkEventController* k = gtk_event_controller_key_new();
    g_signal_connect(k, "key-pressed", G_CALLBACK(tangent), p);
    gtk_widget_add_controller(w, k);
    GtkGesture* c = gtk_gesture_click_new();
    g_signal_connect(c, "pressed", G_CALLBACK(klick), p);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(c));
    GtkGesture* d = gtk_gesture_drag_new();
    g_signal_connect(d, "drag-begin", G_CALLBACK(dragBorjar), p);
    g_signal_connect(d, "drag-update", G_CALLBACK(dragFlyttas), p);
    g_signal_connect(d, "drag-end", G_CALLBACK(dragSlut), p);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(d));
    GtkGesture* stylus = gtk_gesture_stylus_new();
    gtk_gesture_stylus_set_stylus_only(GTK_GESTURE_STYLUS(stylus), TRUE);
    g_signal_connect(stylus, "down", G_CALLBACK(stylusNer), p);
    g_signal_connect(stylus, "motion", G_CALLBACK(stylusRor), p);
    g_signal_connect(stylus, "up", G_CALLBACK(stylusUpp), p);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(stylus));
    GtkEventController* f = gtk_event_controller_focus_new();
    g_signal_connect_swapped(f, "enter", G_CALLBACK(gtk_widget_queue_draw), w);
    g_signal_connect_swapped(f, "leave", G_CALLBACK(gtk_widget_queue_draw), w);
    gtk_widget_add_controller(w, f);
    uppdatera(p);
    return w;
}

GtkWidget* raknepapper_verktyg(GtkWidget* w) {
    Papper* p = av(w);
    GtkWidget* rad = adw_wrap_box_new();   // radbryts i en smal kolumn
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(rad), 2);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(rad), 2);
    gtk_widget_add_css_class(rad, "matte-papper-verktyg");
    const char* namn[3] = {tr("Type", "Skriv"), tr("Pen", "Penna"), tr("Eraser", "Sudd")};
    const char* tips[3] = {tr("Type with the keyboard – one digit per square. Enter: next line. _ draws a line under the row.",
                              "Skriv med tangentbordet – en siffra per ruta. Enter: nästa rad. _ drar ett streck under raden."),
                           tr("Draw and write by hand", "Rita och skriv för hand"),
                           tr("Erase what you touch", "Sudda det du rör vid")};
    for (int i = 0; i < 3; ++i) {
        p->lageK[i] = knapp(rad, namn[i], tips[i], true);
        if (i) gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(p->lageK[i]), GTK_TOGGLE_BUTTON(p->lageK[0]));
        g_object_set_data(G_OBJECT(p->lageK[i]), "lage", GINT_TO_POINTER(i));
        g_signal_connect(p->lageK[i], "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer d) {
            Papper* p = static_cast<Papper*>(d);
            if (p->synkar || !gtk_toggle_button_get_active(b)) return;
            p->lage = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "lage"));
            gtk_widget_grab_focus(p->w);
            uppdatera(p);
        }), p);
    }
    adw_wrap_box_append(ADW_WRAP_BOX(rad), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    p->litenK = knapp(rad, tr("Small digit", "Minnessiffra"), tr("The next digit is written small in the corner (carry). Also Ctrl+digit.",
                                                                  "Nästa siffra skrivs liten i hörnet (minnessiffra). Också Ctrl+siffra."), true);
    g_signal_connect(p->litenK, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer d) {
        Papper* p = static_cast<Papper*>(d);
        if (p->synkar) return;
        p->liten = gtk_toggle_button_get_active(b); p->lage = 0;
        gtk_widget_grab_focus(p->w); uppdatera(p);
    }), p);
    p->vansterK = knapp(rad, tr("← Right to left", "← Åt vänster"), tr("Write the answer from the right, as when you calculate in columns",
                                                                   "Skriv svaret från höger, som när man räknar uppställt"), true);
    g_signal_connect(p->vansterK, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer d) {
        Papper* p = static_cast<Papper*>(d);
        if (p->synkar) return;
        p->vanster = gtk_toggle_button_get_active(b); p->lage = 0;
        gtk_widget_grab_focus(p->w); uppdatera(p);
    }), p);
    GtkWidget* li = knapp(rad, tr("Line", "Streck"), tr("A line under the row (also _)", "Ett streck under raden (också _)"), false);
    g_signal_connect(li, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        Papper* p = static_cast<Papper*>(d); strecka(p); gtk_widget_grab_focus(p->w); uppdatera(p); markeraAndring(p);
    }), p);
    adw_wrap_box_append(ADW_WRAP_BOX(rad), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    p->stallK = knapp(rad, tr("Set it up", "Ställ upp"), tr("Write the numbers in the task in columns", "Skriv talen i uppgiften i kolumner"), false);
    gtk_widget_add_css_class(p->stallK, "matte-stall-upp");
    gtk_widget_set_visible(p->stallK, FALSE);
    g_signal_connect(p->stallK, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        Papper* p = static_cast<Papper*>(d); raknepapper_stall_upp(p->w, p->uppgift); gtk_widget_grab_focus(p->w);
    }), p);
    GtkWidget* undo = knapp(rad, tr("Undo pen stroke", "Ångra pennstreck"),
                            tr("Remove your most recent handwritten stroke", "Ta bort det senaste handskrivna strecket"), false);
    g_signal_connect(undo, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        Papper* p = static_cast<Papper*>(d);
        if (!p->drag.empty()) {
            p->antalPunkter -= std::min(p->antalPunkter, p->drag.back().size());
            p->drag.pop_back();
        }
        p->stylusAktiv = false;
        gtk_widget_grab_focus(p->w);
        uppdatera(p);
        markeraAndring(p);
    }), p);
    GtkWidget* re = knapp(rad, tr("Clear", "Rensa"),
                            tr("Erase this working paper after confirmation", "Sudda hela räknepappret efter bekräftelse"),
                            false);
    g_signal_connect(re, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        Papper* p = static_cast<Papper*>(d);
        if (p->c.empty() && p->drag.empty() && p->streck.empty()) return;
        GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(p->w));
        if (!parent) return;
        AdwAlertDialog* dialog = adw_alert_dialog_new(
            tr("Clear working paper?", "Rensa räknepappret?"),
            tr("The handwritten calculations on this page will be erased.",
               "De handskrivna uträkningarna på den här sidan tas bort."));
        adw_alert_dialog_add_responses(dialog,
            "cancel", tr("Cancel", "Avbryt"),
            "clear", tr("Clear", "Rensa"), nullptr);
        adw_alert_dialog_set_default_response(dialog, "cancel");
        adw_alert_dialog_set_close_response(dialog, "cancel");
        adw_alert_dialog_set_response_appearance(dialog, "clear", ADW_RESPONSE_DESTRUCTIVE);
        g_signal_connect(dialog, "response", G_CALLBACK(+[](AdwAlertDialog*, const char* response, gpointer d) {
            if (response && std::string(response) == "clear") {
                Papper* p = static_cast<Papper*>(d);
                raknepapper_rensa(p->w);
                gtk_widget_grab_focus(p->w);
            }
        }), p);
        adw_dialog_present(ADW_DIALOG(dialog), parent);
    }), p);
    uppdatera(p);
    return rad;
}

void raknepapper_rensa(GtkWidget* w) {
    Papper* p = av(w);
    if (!p) return;
    p->c.clear(); p->streck.clear(); p->drag.clear(); p->stylusAktiv = false;
    p->antalPunkter = 0; p->varnadOmGrans = false;
    p->rad = 1; p->kol = p->start = 2; p->liten = p->vanster = false; p->lage = 0;
    uppdatera(p);
    markeraAndring(p);
}

void raknepapper_skriv(GtkWidget* w, const std::string& t) {
    Papper* p = av(w);
    if (!p) return;
    if (t == "⌫") sudda(p);
    else skrivTecken(p, t == "−" ? "-" : t);
    uppdatera(p);
    markeraAndring(p);
}

void raknepapper_zoom(GtkWidget* w, double zoom) {
    if (Papper* p = av(w)) { p->zoom = std::clamp(zoom, 0.6, 2.5); uppdatera(p); }
}

int raknepapper_rader(GtkWidget* w) {
    Papper* p = av(w);
    if (!p) return 0;
    std::set<int> r;
    for (auto& [k, v] : p->c) if (!v.t.empty() || !v.liten.empty()) r.insert(k.first);
    return (int)r.size() + (p->drag.empty() ? 0 : 1);
}

void raknepapper_andringar(GtkWidget* w, std::function<void()> spara,
                            std::function<void()> begransning) {
    if (Papper* p = av(w)) {
        p->andrad = std::move(spara);
        p->begransad = std::move(begransning);
    }
}

PapperData raknepapper_hamta(GtkWidget* w) {
    if (Papper* p = av(w)) return static_cast<const PapperData&>(*p);
    return PapperData{};
}

void raknepapper_ny_session(GtkWidget* w, const std::string& id,
                           const std::string& kort, const std::string& fraga,
                           int mall, int variant) {
    if (Papper* p = av(w)) {
        static_cast<PapperData&>(*p) = PapperData{};
        p->id = id; p->kort = kort; p->fraga = fraga;
        p->mallIndex = mall; p->variantIndex = variant;
        p->antalPunkter = 0; p->varnadOmGrans = false; p->stylusAktiv = false;
        uppdatera(p); // ingen sparsignal vid byte av session
    }
}

bool raknepapper_ladda_session(GtkWidget* w, const PapperData& v) {
    Papper* p = av(w);
    if (!p || !papperKontroll(v)) return false;
    static_cast<PapperData&>(*p) = v;
    p->antalPunkter = 0;
    for (const auto& line : p->drag) p->antalPunkter += line.size();
    p->varnadOmGrans = false; p->stylusAktiv = false;
    uppdatera(p); // återläsning är inte en redigering
    return true;
}

bool raknepapper_tomt(GtkWidget* w) {
    Papper* p = av(w);
    return !p || (p->c.empty() && p->drag.empty() && p->streck.empty());
}

bool raknepapper_kan_stalla_upp(const std::string& text) { return tolka(text, nullptr); }

void raknepapper_satt_uppgift(GtkWidget* w, const std::string& text) {
    Papper* p = av(w);
    if (!p) return;
    p->uppgift = text;
    if (p->stallK) gtk_widget_set_visible(p->stallK, tolka(text, nullptr));
}

bool raknepapper_stall_upp(GtkWidget* w, const std::string& text) {
    Papper* p = av(w);
    Upp u;
    if (!p || !tolka(text, &u)) return false;
    raknepapper_rensa(w);
    auto dela = [](const std::string& s, std::string& hel, std::string& dec) {
        size_t k = s.find(',');
        hel = s.substr(0, k);
        dec = k == std::string::npos ? "" : s.substr(k + 1);
    };
    std::string ah, ad, bh, bd;
    dela(u.a, ah, ad);
    dela(u.b, bh, bd);
    int D = (int)std::max(ad.size(), bd.size());
    int L = 1;                                                   // operatorn längst till vänster
    int heltalsSlut = L + (int)std::max(ah.size(), bh.size());   // sista heltalssiffran
    int sista = heltalsSlut + (D ? 1 + D : 0);
    auto skriv = [&](int rad, const std::string& hel, const std::string& dec) {
        for (size_t i = 0; i < hel.size(); ++i) p->c[{rad, heltalsSlut - (int)hel.size() + 1 + (int)i}].t = std::string(1, hel[i]);
        if (!dec.empty()) {
            p->c[{rad, heltalsSlut + 1}].t = ",";
            for (size_t i = 0; i < dec.size(); ++i) p->c[{rad, heltalsSlut + 2 + (int)i}].t = std::string(1, dec[i]);
        }
    };
    skriv(1, ah, ad);   // rad 0 är tom: plats för minnessiffrorna
    skriv(2, bh, bd);
    p->c[{2, L}].t = u.op;
    p->streck.push_back({2, L, sista});
    p->rad = 3; p->kol = p->start = sista; p->vanster = true; p->lage = 0;
    uppdatera(p);
    markeraAndring(p);
    return true;
}

void raknepapper_fyll(GtkWidget* w, const std::string& rader) {
    Papper* p = av(w);
    if (!p) return;
    int rad = 1;
    size_t a = 0;
    while (a <= rader.size()) {
        size_t b = rader.find('|', a);
        std::string r = rader.substr(a, b == std::string::npos ? std::string::npos : b - a);
        int kol = 2;
        for (const char* q = r.c_str(); *q; q = g_utf8_next_char(q)) {
            std::string t(q, g_utf8_next_char(q) - q);
            if (t == "_") { p->streck.push_back({rad, 1, kol}); continue; }
            if (t != " ") p->c[{rad, kol}].t = t;
            ++kol;
        }
        ++rad;
        if (b == std::string::npos) break;
        a = b + 1;
    }
    p->rad = rad; p->kol = p->start = 2;
    uppdatera(p);
    markeraAndring(p);  // används även av CI:s återställningstest
}

}  // namespace matte
