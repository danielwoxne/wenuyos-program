// Matte 1.3 / etapp 6: EN sammanhållen laborationsyta, responsiv och tangentbordsstyrd.
// GTK4 utan externa grafikfiler. Svarsrättning och originaluppgifter berörs inte.
#include "LektionsytaGtk.h"
#include <Wenuy.h>

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using wenuy::tr;

namespace matte13 {
namespace {
struct Verktyg {
    const char* id;
    const char* engelska;
    const char* svenska;
    GtkWidget* panel;
};
struct Lektionsyta {
    GtkWidget* val = nullptr;
    GtkWidget* stack = nullptr;
    std::array<Verktyg, 5> verktyg{};
    std::vector<GtkWidget*> tillgangliga;
    bool uppdaterar = false;
};

Lektionsyta* state(GtkWidget* widget) {
    return widget ? static_cast<Lektionsyta*>(g_object_get_data(G_OBJECT(widget), "matte13-lektionsyta")) : nullptr;
}

void visaValt(Lektionsyta* m) {
    if (!m || m->uppdaterar || m->tillgangliga.empty()) return;
    const guint index = gtk_drop_down_get_selected(GTK_DROP_DOWN(m->val));
    if (index >= m->tillgangliga.size()) return;
    GtkWidget* valt = m->tillgangliga[index];
    gtk_stack_set_visible_child(GTK_STACK(m->stack), valt);
    // Alla modellerna är små, självständiga GtkExpander-komponenter.
    // När eleven väljer ett verktyg öppnas just det verktyget utan ett extra klick.
    if (GTK_IS_EXPANDER(valt))
        gtk_expander_set_expanded(GTK_EXPANDER(valt), TRUE);
}
} // namespace

GtkWidget* skapaLektionsyta(GtkWidget* brak, GtkWidget* hundraruta,
                           GtkWidget* tallinje, GtkWidget* geometri,
                           GtkWidget* steghjalp) {
    if (!brak || !hundraruta || !tallinje || !geometri || !steghjalp) return nullptr;
    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(root, "matte13-lektionsyta");
    auto* m = new Lektionsyta;
    g_object_set_data_full(G_OBJECT(root), "matte13-lektionsyta", m,
                           +[](gpointer p) { delete static_cast<Lektionsyta*>(p); });
    m->verktyg = {{{"brak", "Fraction model", "Bråkmodell", brak},
                   {"hundra", "100-square grid", "Hundraruta", hundraruta},
                   {"tal", "Number line", "Tallinje", tallinje},
                   {"geo", "Geometry", "Geometri", geometri},
                   {"steg", "Step-by-step help", "Hjälp steg för steg", steghjalp}}};

    GtkWidget* rubrik = gtk_label_new(tr("Explore and understand", "Utforska och förstå"));
    gtk_label_set_xalign(GTK_LABEL(rubrik), 0.0f);
    gtk_widget_add_css_class(rubrik, "matte13-lektionsyta-rubrik");
    gtk_box_append(GTK_BOX(root), rubrik);

    GtkWidget* introduktion = gtk_label_new(tr(
        "Try a visual model or get help. It will not change your answer.",
        "Prova en bildmodell eller få hjälp. Ditt svar ändras inte."));
    gtk_label_set_xalign(GTK_LABEL(introduktion), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(introduktion), TRUE);
    gtk_widget_add_css_class(introduktion, "matte13-lektionsyta-info");
    gtk_box_append(GTK_BOX(root), introduktion);

    m->val = gtk_drop_down_new(nullptr, nullptr);
    gtk_widget_set_halign(m->val, GTK_ALIGN_START);
    gtk_widget_add_css_class(m->val, "matte13-lektionsyta-val");
    gtk_accessible_update_property(GTK_ACCESSIBLE(m->val), GTK_ACCESSIBLE_PROPERTY_LABEL,
        tr("Choose a learning tool", "Välj ett lärverktyg"), -1);
    gtk_box_append(GTK_BOX(root), m->val);

    m->stack = gtk_stack_new();
    // En enda kort toning när systemet tillåter animationer, annars direkt byte.
    // GTK:s inställning kommer även från hjälpmedel och användarens önskemål.
    gboolean animationer = TRUE;
    if (GtkSettings* settings = gtk_settings_get_default())
        g_object_get(settings, "gtk-enable-animations", &animationer, nullptr);
    gtk_stack_set_transition_type(GTK_STACK(m->stack), animationer
        ? GTK_STACK_TRANSITION_TYPE_CROSSFADE : GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_stack_set_transition_duration(GTK_STACK(m->stack), 130);
    gtk_widget_set_hexpand(m->stack, TRUE);
    for (const auto& v : m->verktyg)
        gtk_stack_add_named(GTK_STACK(m->stack), v.panel, v.id);
    gtk_box_append(GTK_BOX(root), m->stack);

    g_signal_connect(m->val, "notify::selected",
        G_CALLBACK(+[](GtkDropDown*, GParamSpec*, gpointer ptr) {
            visaValt(static_cast<Lektionsyta*>(ptr));
        }), m);
    gtk_widget_set_visible(root, FALSE);
    return root;
}

void uppdateraLektionsyta(GtkWidget* root, bool brak, bool tallinje, bool geometri, bool hjalp) {
    Lektionsyta* m = state(root);
    if (!m) return;
    const std::array<bool, 5> giltiga = {{brak, brak, tallinje, geometri, hjalp}};
    // Behåll vald bildmodell om uppgiften ritas om, till exempel vid A+/A-.
    // Vid ämnesbyte väljs första verktyget som verkligen passar nya frågan.
    GtkWidget* tidigare = nullptr;
    const guint tidigareIndex = gtk_drop_down_get_selected(GTK_DROP_DOWN(m->val));
    if (tidigareIndex < m->tillgangliga.size())
        tidigare = m->tillgangliga[tidigareIndex];
    m->uppdaterar = true;
    m->tillgangliga.clear();
    GtkStringList* etiketter = gtk_string_list_new(nullptr);
    // En enda väljare: inga irrelevanta verktyg visas på andra ämneskort.
    for (std::size_t i=0; i<m->verktyg.size(); ++i) {
        GtkWidget* panel = m->verktyg[i].panel;
        gtk_widget_set_visible(panel, giltiga[i]);
        if (!giltiga[i]) continue;
        m->tillgangliga.push_back(panel);
        gtk_string_list_append(etiketter, tr(m->verktyg[i].engelska, m->verktyg[i].svenska));
        if (GTK_IS_EXPANDER(panel))
            gtk_expander_set_expanded(GTK_EXPANDER(panel), FALSE);
    }
    gtk_drop_down_set_model(GTK_DROP_DOWN(m->val), G_LIST_MODEL(etiketter));
    g_object_unref(etiketter);
    gtk_widget_set_visible(root, !m->tillgangliga.empty());
    gtk_widget_set_visible(m->val, m->tillgangliga.size() > 1);
    guint nyttIndex = GTK_INVALID_LIST_POSITION;
    for (guint i=0; i<m->tillgangliga.size(); ++i) {
        if (m->tillgangliga[i] == tidigare) { nyttIndex = i; break; }
    }
    if (nyttIndex == GTK_INVALID_LIST_POSITION && !m->tillgangliga.empty()) nyttIndex = 0;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(m->val), nyttIndex);
    m->uppdaterar = false;
    visaValt(m);
}
} // namespace matte13
