// Matte 1.3 etapp 5: interaktiv, tillgänglig hundraruta.
// Ritas direkt med Cairo på GTK4:s draw-signal. Ingen bildfil, GPU eller bakgrundstimer.
#include "HundrarutaGtk.h"
#include "VisuellStil.h"
#include <Wenuy.h>
#include <algorithm>
#include <cmath>
#include <string>

using wenuy::tr;
namespace matte13 {
namespace {
struct Vy {
    Hundraruta ursprung;
    GtkWidget* bild = nullptr;
    GtkWidget* valj = nullptr;
    GtkWidget* forklaring = nullptr;
    GtkWidget* aterstall = nullptr;
};

// Returvärden används också av klickhantering så att rutor och mus matchar exakt.
bool geometri(int w, int h, double& x0, double& y0, double& cell, double& mellan) {
    mellan = 2.0;
    const double tillganglig = std::min((w - 24.0), (h - 24.0));
    if (tillganglig < 80) return false;
    cell = (tillganglig - 9 * mellan) / 10.0;
    if (cell < 5.0) return false;
    const double side = cell * 10 + mellan * 9;
    x0 = (w - side) / 2.0;
    y0 = (h - side) / 2.0;
    return true;
}

void mjukRuta(cairo_t* cr, double x, double y, double w, double h, double r) {
    const double k = std::acos(-1.0)/2;
    cairo_new_sub_path(cr);
    cairo_arc(cr,x+w-r,y+r,r,-k,0);
    cairo_arc(cr,x+w-r,y+h-r,r,0,k);
    cairo_arc(cr,x+r,y+h-r,r,k,2*k);
    cairo_arc(cr,x+r,y+r,r,2*k,3*k);
    cairo_close_path(cr);
}

void rita(GtkDrawingArea*, cairo_t* cr, int w, int h, gpointer ptr) {
    auto* v = static_cast<Vy*>(ptr);
    if (!v) return;
    double x0=0, y0=0, cell=0, gap=0;
    if (!geometri(w,h,x0,y0,cell,gap)) return;
    // Ljust ritpapper och konsekvent palett, även när skrivbordet har mörkt tema.
    const auto palette = illustrationsPalett();
    const auto color = [cr](RGB c) { cairo_set_source_rgb(cr,c.r,c.g,c.b); };
    color(palette.papper); cairo_paint(cr);
    cairo_set_antialias(cr,CAIRO_ANTIALIAS_BEST);
    const int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(v->valj));
    for (int i=0; i<100; ++i) {
        const double x = x0+(i%10)*(cell+gap), y = y0+(i/10)*(cell+gap);
        mjukRuta(cr,x,y,cell,cell,std::min(cell*.14,3.8));
        color(i<n ? palette.aktiv : palette.tom);
        cairo_fill_preserve(cr);
        color(palette.kontur);
        cairo_set_line_width(cr, 0.8);
        cairo_stroke(cr);
        // Liten diagonal markering förstärker skillnaden även utan färgseende.
        if (i<n && cell >= 12) {
            cairo_move_to(cr, x + cell*0.25, y + cell*0.70);
            cairo_line_to(cr, x + cell*0.70, y + cell*0.25);
            color(palette.struktur);
            cairo_set_line_width(cr, 1.3);
            cairo_stroke(cr);
        }
    }
}

void uppdatera(Vy* v) {
    if (!v || !v->bild) return;
    const int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(v->valj));
    const bool oforandrad = n == v->ursprung.fyllda;
    const std::string text = oforandrad
        ? (wenuy::swedish() ? hundrarutaForklaringSv(v->ursprung)
                             : hundrarutaForklaringEn(v->ursprung))
        : (wenuy::swedish() ? hundrarutaEgenTextSv(n) : hundrarutaEgenTextEn(n));
    gtk_label_set_text(GTK_LABEL(v->forklaring), text.c_str());
    // Den upplästa beskrivningen finns också som vanlig text och uppdateras vid varje ändring.
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->bild), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   text.c_str(), -1);
    gtk_widget_set_sensitive(v->aterstall, !oforandrad);
    gtk_widget_queue_draw(v->bild);
}
}  // namespace

GtkWidget* skapaHundrarutapanel() {
    GtkWidget* exp = gtk_expander_new(tr("Explore a 100-square grid", "Utforska hundrarutan"));
    gtk_widget_add_css_class(exp, "matte13-hundraruta");
    gtk_expander_set_expanded(GTK_EXPANDER(exp), FALSE);
    auto* v = new Vy;
    g_object_set_data_full(G_OBJECT(exp), "matte13-hundra-data", v,
                           +[](gpointer p){ delete static_cast<Vy*>(p); });

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget* info = gtk_label_new(tr(
        "100 squares make one whole. Tap a square or use the number control. This practice grid does not change your answer.",
        "100 rutor är en helhet. Tryck på en ruta eller använd sifferreglaget. Laborationen ändrar inte ditt svar."));
    gtk_label_set_xalign(GTK_LABEL(info), 0);
    gtk_label_set_wrap(GTK_LABEL(info), TRUE);
    gtk_box_append(GTK_BOX(box), info);

    GtkWidget* tools = adw_wrap_box_new();
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(tools), 10);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(tools), 8);
    GtkWidget* cap = gtk_label_new(tr("Coloured squares", "Färgade rutor"));
    adw_wrap_box_append(ADW_WRAP_BOX(tools), cap);
    v->valj = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->valj), GTK_ACCESSIBLE_PROPERTY_LABEL,
        tr("Number of shaded squares, between 0 and 100", "Antal färgade rutor mellan 0 och 100"), -1);
    adw_wrap_box_append(ADW_WRAP_BOX(tools), v->valj);
    v->aterstall = gtk_button_new_with_label(tr("Reset", "Återställ"));
    gtk_widget_set_tooltip_text(v->aterstall,
        tr("Return to the original fraction in the task", "Visa åter bråket från uppgiften"));
    adw_wrap_box_append(ADW_WRAP_BOX(tools), v->aterstall);
    gtk_box_append(GTK_BOX(box), tools);

    v->bild = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(v->bild), 310);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(v->bild), 300);
    gtk_widget_set_hexpand(v->bild, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(v->bild), rita, v, nullptr);
    gtk_box_append(GTK_BOX(box), v->bild);
    GtkGesture* click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed", G_CALLBACK((+[](GtkGestureClick*, int, double x, double y, gpointer ptr) {
        auto* v = static_cast<Vy*>(ptr);
        const int w = gtk_widget_get_width(v->bild), h = gtk_widget_get_height(v->bild);
        double x0=0,y0=0,cell=0,gap=0;
        if (!geometri(w,h,x0,y0,cell,gap)) return;
        if (x < x0 || y < y0 || x >= x0 + 10*cell + 9*gap || y >= y0 + 10*cell + 9*gap) return;
        const int col = (int)((x-x0)/(cell+gap));
        const int row = (int)((y-y0)/(cell+gap));
        if (col < 0 || col > 9 || row < 0 || row > 9) return;
        // Mellanrum mellan rutorna ska inte räknas som ett klick på en ruta.
        if (std::fmod(x-x0, cell+gap) > cell || std::fmod(y-y0, cell+gap) > cell) return;
        const int val = rutorTillOchMed(col,row);
        if (val >= 0) gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->valj), val);
    })), v);
    gtk_widget_add_controller(v->bild, GTK_EVENT_CONTROLLER(click));
    v->forklaring = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(v->forklaring), TRUE);
    gtk_label_set_selectable(GTK_LABEL(v->forklaring), TRUE);
    gtk_label_set_xalign(GTK_LABEL(v->forklaring), 0);
    gtk_widget_add_css_class(v->forklaring, "matte13-hundraruta-resultat");
    gtk_box_append(GTK_BOX(box), v->forklaring);
    gtk_expander_set_child(GTK_EXPANDER(exp), box);
    g_signal_connect(v->valj, "value-changed", G_CALLBACK(+[](GtkSpinButton*,gpointer ptr){
        uppdatera(static_cast<Vy*>(ptr));
    }),v);
    g_signal_connect(v->aterstall, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ptr){
        auto* v = static_cast<Vy*>(ptr);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->valj), v->ursprung.fyllda);
        uppdatera(v);
    }),v);
    stallInHundrarutapanel(exp, Brak{3,4});
    return exp;
}

void stallInHundrarutapanel(GtkWidget* panel, Brak ursprung) {
    if (!panel || !giltigt(ursprung)) return;
    auto* v = static_cast<Vy*>(g_object_get_data(G_OBJECT(panel), "matte13-hundra-data"));
    if (!v) return;
    v->ursprung = hundrarutaFranBrak(ursprung);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->valj), v->ursprung.fyllda);
    gtk_expander_set_expanded(GTK_EXPANDER(panel), FALSE);
    uppdatera(v);
}
}
