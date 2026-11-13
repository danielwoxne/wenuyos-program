#include "MattekartaGtk.h"
#include <adwaita.h>
#include <string>

namespace matte13 {
namespace {
GtkWidget* etikett(const std::string& text, const char* css = nullptr) {
    GtkWidget* w = gtk_label_new(text.c_str());
    gtk_label_set_wrap(GTK_LABEL(w), TRUE);
    gtk_label_set_xalign(GTK_LABEL(w), 0.0f);
    if (css) gtk_widget_add_css_class(w, css);
    return w;
}
GtkWidget* kolumn(int mellan = 8) { return gtk_box_new(GTK_ORIENTATION_VERTICAL, mellan); }
GtkWidget* statusruta(const char* rubrik, int antal, const char* css) {
    GtkWidget* ruta = kolumn(4);
    gtk_widget_add_css_class(ruta, "matte13-kartastat");
    gtk_widget_add_css_class(ruta, css);
    gtk_box_append(GTK_BOX(ruta), etikett(std::to_string(antal), "matte13-kartastat-tal"));
    gtk_box_append(GTK_BOX(ruta), etikett(rubrik, "matte13-kartastat-rubrik"));
    // Status har alltid synlig text, inte bara en färgkod.
    return ruta;
}
}

GtkWidget* skapaKartaOversikt(const KartaSummering& s, bool svenska) {
    GtkWidget* panel = kolumn(13);
    gtk_widget_add_css_class(panel, "matte13-kartaoversikt");
    gtk_box_append(GTK_BOX(panel), etikett(svenska ? "Din lärandeväg" : "Your learning path", "matte13-kartaoversikt-titel"));
    gtk_box_append(GTK_BOX(panel), etikett(
        std::to_string(s.beharskar) + (svenska ? " av " : " of ") + std::to_string(s.totalt) +
        (svenska ? " regelkort i detta stadium sitter." : " lesson cards mastered in this stage."),
        "matte13-kartaoversikt-beskrivning"));
    GtkWidget* progress = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), s.andel);
    gtk_widget_add_css_class(progress, "matte13-kartaoversikt-stapel");
    gtk_accessible_update_property(GTK_ACCESSIBLE(progress), GTK_ACCESSIBLE_PROPERTY_LABEL,
        svenska ? "Andel regelkort som du behärskar i valt stadium" : "Share of cards mastered in this stage", -1);
    gtk_box_append(GTK_BOX(panel), progress);
    GtkWidget* stats = adw_wrap_box_new();
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(stats), 8);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(stats), 8);
    gtk_widget_add_css_class(stats, "matte13-kartaoversikt-rad");
    adw_wrap_box_append(ADW_WRAP_BOX(stats), statusruta(svenska ? "Behärskar" : "Mastered", s.beharskar, "klar"));
    adw_wrap_box_append(ADW_WRAP_BOX(stats), statusruta(svenska ? "Övar" : "Practising", s.ovar, "pagar"));
    adw_wrap_box_append(ADW_WRAP_BOX(stats), statusruta(svenska ? "Att repetera" : "Review", s.repetition, "repetition"));
    gtk_box_append(GTK_BOX(panel), stats);
    gtk_box_append(GTK_BOX(panel), etikett(svenska
        ? "Välj ett regelkort nedan. Du kan alltid gå tillbaka och repetera."
        : "Choose a lesson card below. You can always revisit earlier lessons.",
        "matte13-kartaoversikt-beskrivning"));
    return panel;
}
} // namespace matte13
