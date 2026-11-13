// Visuell bråkmodell för Matte 1.3: GTK4 + Cairo, inga animationer eller bakgrundstrådar.
// Eleven kan ändra antalet delar. Påverkar INTE uppgiften eller elevens svar.
#include "BrakmodellGtk.h"
#include "VisuellStil.h"
#include <Wenuy.h>
#include <algorithm>
#include <cmath>

using wenuy::tr;

namespace matte13 {
namespace {
struct Vy {
    GtkWidget* bild = nullptr;
    GtkWidget* taljare = nullptr;
    GtkWidget* namnare = nullptr;
    GtkWidget* text = nullptr;
};

// Etapp 8: skarpa figurer, gemensam färgpalett och extra struktur i fyllda delar.
// Inga bildfiler eller renderloopar; målas om endast vid ändrat värde/fönster.
void ritaBrak(GtkDrawingArea*, cairo_t* cr, int bredd, int hojd, gpointer person) {
    auto* v = static_cast<Vy*>(person);
    Brak b{gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(v->taljare)),
           gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(v->namnare))};
    if (!giltigt(b) || bredd < 160 || hojd < 140) return;
    const auto palette = illustrationsPalett();
    auto source = [cr](RGB col) { cairo_set_source_rgb(cr,col.r,col.g,col.b); };
    source(palette.papper); cairo_paint(cr);
    const auto pos = brakPlacering(bredd,hojd);
    const double pi = std::acos(-1.0);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);
    for (int i=0; i<b.namnare; ++i) {
        const double a0 = -pi*.5 + 2*pi*i/b.namnare;
        const double a1 = -pi*.5 + 2*pi*(i+1)/b.namnare;
        cairo_new_path(cr);
        cairo_move_to(cr,pos.cirkelX,pos.cirkelY);
        cairo_arc(cr,pos.cirkelX,pos.cirkelY,pos.radie,a0,a1);
        cairo_close_path(cr);
        const bool aktiv = i < b.taljare;
        source(aktiv ? palette.aktiv : palette.tom);
        cairo_fill_preserve(cr);
        source(palette.kontur);
        cairo_set_line_width(cr,1.6);
        cairo_stroke(cr);
        if (aktiv) {
            const double medel = (a0+a1)*.5;
            const double markR = pos.radie*.67;
            source(palette.struktur);
            cairo_arc(cr,pos.cirkelX + std::cos(medel)*markR,
                      pos.cirkelY + std::sin(medel)*markR,
                      std::min(3.5,pos.radie*.075),0,2*pi);
            cairo_fill(cr);
        }
    }
    const double bit = pos.stapelBredd / b.namnare;
    for (int i=0; i<b.namnare; ++i) {
        const double x = pos.stapelX + i*bit;
        const bool aktiv = i < b.taljare;
        cairo_rectangle(cr,x,pos.stapelY,bit,pos.stapelHojd);
        source(aktiv ? palette.aktiv : palette.tom);
        cairo_fill_preserve(cr);
        source(palette.kontur); cairo_set_line_width(cr,1.5); cairo_stroke(cr);
        // Två ljusa streck är en andra, icke-färgberoende, markering.
        if (aktiv && bit >= 14) {
            source(palette.struktur);
            cairo_set_line_width(cr,1.5);
            const double cy = pos.stapelY + pos.stapelHojd*.50;
            for (int j=0;j<2;++j) {
                const double cx = x + bit*(.38 + j*.18);
                cairo_move_to(cr,cx-3,cy+5); cairo_line_to(cr,cx+3,cy-5);
            }
            cairo_stroke(cr);
        }
    }
}

void uppdatera(Vy* v) {
    int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(v->namnare));
    auto* tal = GTK_SPIN_BUTTON(v->taljare);
    gtk_spin_button_set_range(tal, 0, n);
    int t = std::min(gtk_spin_button_get_value_as_int(tal), n);
    gtk_spin_button_set_value(tal, t);
    Brak b{t,n};
    const std::string s = std::to_string(t) + "/" + std::to_string(n) + " = " + procentText(b);
    gtk_label_set_text(GTK_LABEL(v->text), s.c_str());
    std::string a = tr("Coloured parts: ", "Färgade delar: ");
    a += std::to_string(t) + tr(" of ", " av ") + std::to_string(n);
    a += ". " + s;
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->bild), GTK_ACCESSIBLE_PROPERTY_LABEL, a.c_str(), -1);
    gtk_widget_queue_draw(v->bild);
}
}

GtkWidget* skapaBrakpanel() {
    GtkWidget* exp = gtk_expander_new(tr("Explore fractions visually", "Utforska bråk med bilder"));
    gtk_widget_add_css_class(exp, "matte13-brakpanel");
    gtk_expander_set_expanded(GTK_EXPANDER(exp), FALSE);
    auto* v = new Vy;
    g_object_set_data_full(G_OBJECT(exp), "matte13-vy", v,
                           +[](gpointer ptr) { delete static_cast<Vy*>(ptr); });
    GtkWidget* in = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(in, 10);
    GtkWidget* intro = gtk_label_new(tr(
        "Change the coloured parts and the number of equal parts. This picture does not change your answer.",
        "Ändra antalet färgade delar och antalet lika stora delar. Bilden ändrar inte ditt svar."));
    gtk_label_set_wrap(GTK_LABEL(intro), TRUE);
    gtk_label_set_xalign(GTK_LABEL(intro), 0.0f);
    gtk_box_append(GTK_BOX(in), intro);
    GtkWidget* rad = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(rad), 12);
    gtk_grid_set_row_spacing(GTK_GRID(rad), 8);
    GtkWidget* t = gtk_label_new(tr("Coloured", "Färgade"));
    gtk_grid_attach(GTK_GRID(rad), t, 0, 0, 1, 1);
    v->taljare = gtk_spin_button_new_with_range(0, 12, 1);
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->taljare), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   tr("Number of coloured parts", "Antal färgade delar"), -1);
    gtk_grid_attach(GTK_GRID(rad), v->taljare, 1, 0, 1, 1);
    GtkWidget* n = gtk_label_new(tr("Equal parts", "Lika delar"));
    gtk_grid_attach(GTK_GRID(rad), n, 0, 1, 1, 1);
    v->namnare = gtk_spin_button_new_with_range(2, 12, 1);
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->namnare), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   tr("Total number of equal parts", "Antal lika stora delar"), -1);
    gtk_grid_attach(GTK_GRID(rad), v->namnare, 1, 1, 1, 1);
    gtk_box_append(GTK_BOX(in), rad);
    v->bild = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(v->bild), 320);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(v->bild), 240);
    gtk_widget_set_hexpand(v->bild, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(v->bild), ritaBrak, v, nullptr);
    gtk_box_append(GTK_BOX(in), v->bild);
    v->text = gtk_label_new("");
    gtk_widget_add_css_class(v->text, "matte13-brakresultat");
    gtk_box_append(GTK_BOX(in), v->text);
    gtk_expander_set_child(GTK_EXPANDER(exp), in);
    g_signal_connect(v->taljare, "value-changed", G_CALLBACK(+[](GtkSpinButton*, gpointer p){uppdatera(static_cast<Vy*>(p));}), v);
    g_signal_connect(v->namnare, "value-changed", G_CALLBACK(+[](GtkSpinButton*, gpointer p){uppdatera(static_cast<Vy*>(p));}), v);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->namnare), 4);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->taljare), 3);
    uppdatera(v);
    return exp;
}

void stallInBrakpanel(GtkWidget* panel, Brak b) {
    if (!panel || !giltigt(b)) return;
    auto* v = static_cast<Vy*>(g_object_get_data(G_OBJECT(panel), "matte13-vy"));
    if (!v) return;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->namnare), b.namnare);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->taljare), b.taljare);
    uppdatera(v);
}
}
