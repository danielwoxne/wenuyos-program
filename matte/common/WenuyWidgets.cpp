#include "WenuyWidgets.h"
#include "WenuyTheme.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Wenuy.h"
#include "WenuyIcon.h"

// ================================================================ gemensamt
int wenuy_motion_level(void) {
    std::string m = wenuy::readLine(std::string(g_get_user_config_dir()) + "/wenuyos/motion");
    if (m == "off") return 0;
    if (m == "reduced" || wenuy::lowProfile()) return 1;
    gboolean anim = TRUE;
    g_object_get(gtk_settings_get_default(), "gtk-enable-animations", &anim, nullptr);
    return anim ? 2 : 0;
}

static void ensureCss() {
    static bool done = false;
    if (done || !gdk_display_get_default()) return;
    done = true;
    const char* css = R"CSS(
.wenuy-statusbar { border-top: 1px solid @borders; padding: 5px 14px; min-height: 18px; font-size: 0.86em; color: alpha(@window_fg_color, 0.65); }
wenuycard { background: alpha(@view_bg_color, 0.95); border-radius: 16px; padding: 14px 16px;
  border: 1px solid alpha(@borders, 0.8); box-shadow: 0 1px 3px alpha(black, 0.06);
  transition: transform 180ms cubic-bezier(0.2, 0.9, 0.3, 1.2), box-shadow 180ms ease, background 150ms; }
wenuycard:hover { transform: translateY(-2px); box-shadow: 0 6px 18px alpha(black, 0.12); }
wenuycard:active { transform: scale(0.98); }
wenuycard:focus-visible { outline: 2px solid @accent_bg_color; outline-offset: 2px; }
wenuycard.accent { background: alpha(@accent_bg_color, 0.12); border-color: alpha(@accent_bg_color, 0.6); }
wenuycard .wenuy-card-icon { background: alpha(@accent_bg_color, 0.14); color: @accent_color; border-radius: 12px; padding: 8px; }
wenuycard .wenuy-card-title { font-weight: 600; }
wenuycard .wenuy-card-sub { opacity: 0.7; font-size: 0.92em; }
.wenuy-reduced wenuycard, .wenuy-reduced wenuycard:hover { transform: none; transition: background 120ms; }
)CSS";
    auto* p = gtk_css_provider_new();
    gtk_css_provider_load_from_string(p, css);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION - 1);
}

// ================================================================ WenuyCard
struct _WenuyCard {
    GtkWidget parent_instance;
    GtkWidget *iconBox, *icon, *title, *subtitle, *texts;
};
G_DEFINE_FINAL_TYPE(WenuyCard, wenuy_card, GTK_TYPE_WIDGET)
static guint cardActivated = 0;

static void card_activate(WenuyCard* self) {
    if (self->icon) wenuy_icon_play(WENUY_ICON(self->icon));
    g_signal_emit(self, cardActivated, 0);
}
static void wenuy_card_dispose(GObject* o) {
    auto* self = WENUY_CARD(o);
    g_clear_pointer(&self->iconBox, gtk_widget_unparent);
    g_clear_pointer(&self->texts, gtk_widget_unparent);
    G_OBJECT_CLASS(wenuy_card_parent_class)->dispose(o);
}
static void wenuy_card_class_init(WenuyCardClass* k) {
    G_OBJECT_CLASS(k)->dispose = wenuy_card_dispose;
    gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(k), GTK_TYPE_BOX_LAYOUT);
    gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(k), "wenuycard");
    gtk_widget_class_set_accessible_role(GTK_WIDGET_CLASS(k), GTK_ACCESSIBLE_ROLE_BUTTON);
    cardActivated = g_signal_new("activated", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
}
static void wenuy_card_init(WenuyCard* self) {
    ensureCss();
    auto* lm = gtk_widget_get_layout_manager(GTK_WIDGET(self));
    gtk_box_layout_set_spacing(GTK_BOX_LAYOUT(lm), 14);
    gtk_widget_set_focusable(GTK_WIDGET(self), TRUE);
    gtk_widget_set_cursor_from_name(GTK_WIDGET(self), "pointer");
    if (wenuy_motion_level() < 2) gtk_widget_add_css_class(GTK_WIDGET(self), "wenuy-reduced");
    auto* click = gtk_gesture_click_new();
    g_signal_connect_swapped(click, "released", G_CALLBACK(+[](WenuyCard* s) { card_activate(s); }), self);
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(click));
    auto* key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(+[](GtkEventControllerKey*, guint kv, guint, GdkModifierType, gpointer d) -> gboolean {
        if (kv == GDK_KEY_Return || kv == GDK_KEY_KP_Enter || kv == GDK_KEY_space) { card_activate(WENUY_CARD(d)); return TRUE; }
        return FALSE;
    }), self);
    gtk_widget_add_controller(GTK_WIDGET(self), key);
}
GtkWidget* wenuy_card_new(const char* icon, const char* title, const char* subtitle) {
    auto* self = WENUY_CARD(g_object_new(WENUY_TYPE_CARD, nullptr));
    if (icon && *icon) {
        self->iconBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(self->iconBox, "wenuy-card-icon");
        gtk_widget_set_valign(self->iconBox, GTK_ALIGN_CENTER);
        self->icon = wenuy_icon_new(icon, 22);
        gtk_box_append(GTK_BOX(self->iconBox), self->icon);
        gtk_widget_set_parent(self->iconBox, GTK_WIDGET(self));
    }
    self->texts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_valign(self->texts, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(self->texts, TRUE);
    self->title = gtk_label_new(title);
    gtk_label_set_xalign(GTK_LABEL(self->title), 0);
    gtk_label_set_ellipsize(GTK_LABEL(self->title), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(self->title, "wenuy-card-title");
    self->subtitle = gtk_label_new(subtitle ? subtitle : "");
    gtk_label_set_xalign(GTK_LABEL(self->subtitle), 0);
    gtk_label_set_ellipsize(GTK_LABEL(self->subtitle), PANGO_ELLIPSIZE_END);
    gtk_label_set_lines(GTK_LABEL(self->subtitle), 2);
    gtk_label_set_wrap(GTK_LABEL(self->subtitle), TRUE);
    gtk_widget_add_css_class(self->subtitle, "wenuy-card-sub");
    gtk_widget_set_visible(self->subtitle, subtitle && *subtitle);
    gtk_box_append(GTK_BOX(self->texts), self->title);
    gtk_box_append(GTK_BOX(self->texts), self->subtitle);
    gtk_widget_set_parent(self->texts, GTK_WIDGET(self));
    gtk_accessible_update_property(GTK_ACCESSIBLE(self), GTK_ACCESSIBLE_PROPERTY_LABEL, title, -1);
    return GTK_WIDGET(self);
}
void wenuy_card_set_title(WenuyCard* self, const char* t) { gtk_label_set_text(GTK_LABEL(self->title), t); }
void wenuy_card_set_subtitle(WenuyCard* self, const char* t) {
    gtk_label_set_text(GTK_LABEL(self->subtitle), t ? t : "");
    gtk_widget_set_visible(self->subtitle, t && *t);
}
void wenuy_card_set_accent(WenuyCard* self, gboolean on) {
    if (on) gtk_widget_add_css_class(GTK_WIDGET(self), "accent"); else gtk_widget_remove_css_class(GTK_WIDGET(self), "accent");
}

// ================================================================ WenuyChart
struct _WenuyChart {
    GtkWidget parent_instance;
    WenuyChartKind kind;
    std::vector<double>* values;
    std::vector<std::string>* labels;
    double grow;               // 0..1 – framväxt
    AdwAnimation* anim;
};
G_DEFINE_FINAL_TYPE(WenuyChart, wenuy_chart, GTK_TYPE_WIDGET)

static void chart_snapshot(GtkWidget* w, GtkSnapshot* s) {
    auto* self = WENUY_CHART(w);
    const float W = gtk_widget_get_width(w), H = gtk_widget_get_height(w);
    auto& v = *self->values;
    if (v.empty() || W < 40 || H < 40) return;
    GdkRGBA fg, accent, grid;
    gtk_widget_get_color(w, &fg);
    wenuy_theme_get_accent(wenuy_theme_get_default(), &accent);
    grid = fg; grid.alpha = 0.10f;
    const bool hasLabels = !self->labels->empty();
    const float left = 36, right = 8, top = 10, bottom = hasLabels ? 26 : 10;
    const float cw = W - left - right, ch = H - top - bottom;
    double maxV = *std::max_element(v.begin(), v.end());
    if (maxV <= 0) maxV = 1;
    // "snygg" skala: 1, 2, 5 × 10^n
    double step = std::pow(10, std::floor(std::log10(maxV / 4)));
    for (double m : {1.0, 2.0, 5.0, 10.0}) if (maxV / (step * m) <= 5) { step *= m; break; }
    const double top_v = std::ceil(maxV / step) * step;

    PangoLayout* lay = gtk_widget_create_pango_layout(w, "");
    PangoFontDescription* fd = pango_font_description_from_string("sans 9");
    pango_layout_set_font_description(lay, fd);
    GdkRGBA muted = fg; muted.alpha = 0.6f;
    // rutnät + värden på y-axeln
    for (double y = 0; y <= top_v + 1e-9; y += step) {
        const float py = top + ch - float(y / top_v) * ch;
        graphene_rect_t line = GRAPHENE_RECT_INIT(left, py, cw, 1);
        gtk_snapshot_append_color(s, &grid, &line);
        char buf[32];
        g_snprintf(buf, sizeof buf, "%g", y);
        pango_layout_set_text(lay, buf, -1);
        int lw, lh; pango_layout_get_pixel_size(lay, &lw, &lh);
        gtk_snapshot_save(s);
        graphene_point_t p = GRAPHENE_POINT_INIT(left - 6 - lw, py - lh / 2.0f);
        gtk_snapshot_translate(s, &p);
        gtk_snapshot_append_layout(s, lay, &muted);
        gtk_snapshot_restore(s);
    }
    const size_t n = v.size();
    const float slot = cw / n;
    const double g = self->grow;
    if (self->kind == WENUY_CHART_BARS) {
        for (size_t i = 0; i < n; ++i) {
            // staplarna växer en i taget (lätt förskjutning) – ser levande ut utan att ta tid
            double local = std::clamp((g * (1 + 0.4) - 0.4 * double(i) / n), 0.0, 1.0);
            local = 1 - std::pow(1 - local, 3);
            const float bh = float(v[i] / top_v * ch * local);
            const float bw = std::min(48.0f, slot * 0.62f);
            graphene_rect_t r = GRAPHENE_RECT_INIT(left + i * slot + (slot - bw) / 2, top + ch - bh, bw, bh);
            GskRoundedRect rr;
            gsk_rounded_rect_init_from_rect(&rr, &r, std::min(6.0f, bw / 2));
            rr.corner[2] = rr.corner[3] = graphene_size_t{0, 0};
            gtk_snapshot_push_rounded_clip(s, &rr);
            gtk_snapshot_append_color(s, &accent, &r);
            gtk_snapshot_pop(s);
        }
    } else {
        GskPathBuilder* b = gsk_path_builder_new();
        for (size_t i = 0; i < n; ++i) {
            const float x = left + i * slot + slot / 2, y = top + ch - float(v[i] / top_v * ch * g);
            if (i == 0) gsk_path_builder_move_to(b, x, y); else gsk_path_builder_line_to(b, x, y);
        }
        GskPath* path = gsk_path_builder_free_to_path(b);
        GskStroke* st = gsk_stroke_new(2.5f);
        gsk_stroke_set_line_join(st, GSK_LINE_JOIN_ROUND);
        gsk_stroke_set_line_cap(st, GSK_LINE_CAP_ROUND);
        gtk_snapshot_append_stroke(s, path, st, &accent);
        gsk_stroke_free(st);
        gsk_path_unref(path);
        for (size_t i = 0; i < n; ++i) {   // punkter; sista punkten betonad
            const float x = left + i * slot + slot / 2, y = top + ch - float(v[i] / top_v * ch * g);
            GskPathBuilder* pb = gsk_path_builder_new();
            graphene_point_t c = GRAPHENE_POINT_INIT(x, y);
            gsk_path_builder_add_circle(pb, &c, i + 1 == n ? 4.5f : 3.0f);
            GskPath* dot = gsk_path_builder_free_to_path(pb);
            gtk_snapshot_append_fill(s, dot, GSK_FILL_RULE_WINDING, &accent);
            gsk_path_unref(dot);
        }
    }
    if (hasLabels) {
        for (size_t i = 0; i < n && i < self->labels->size(); ++i) {
            pango_layout_set_text(lay, (*self->labels)[i].c_str(), -1);
            int lw, lh; pango_layout_get_pixel_size(lay, &lw, &lh);
            if (lw > slot && n > 8 && i % 2) continue;   // trångt: varannan etikett
            gtk_snapshot_save(s);
            graphene_point_t p = GRAPHENE_POINT_INIT(left + i * slot + (slot - lw) / 2, top + ch + 6);
            gtk_snapshot_translate(s, &p);
            gtk_snapshot_append_layout(s, lay, &muted);
            gtk_snapshot_restore(s);
        }
    }
    pango_font_description_free(fd);
    g_object_unref(lay);
}
static void chart_measure(GtkWidget*, GtkOrientation o, int, int* min, int* nat, int*, int*) {
    *min = o == GTK_ORIENTATION_HORIZONTAL ? 160 : 120;
    *nat = o == GTK_ORIENTATION_HORIZONTAL ? 420 : 220;
}
static void wenuy_chart_finalize(GObject* o) {
    auto* self = WENUY_CHART(o);
    delete self->values; delete self->labels;
    G_OBJECT_CLASS(wenuy_chart_parent_class)->finalize(o);
}
static void wenuy_chart_dispose(GObject* o) {
    auto* self = WENUY_CHART(o);
    if (self->anim) { adw_animation_skip(self->anim); g_clear_object(&self->anim); }
    G_OBJECT_CLASS(wenuy_chart_parent_class)->dispose(o);
}
static void wenuy_chart_class_init(WenuyChartClass* k) {
    GTK_WIDGET_CLASS(k)->snapshot = chart_snapshot;
    GTK_WIDGET_CLASS(k)->measure = chart_measure;
    G_OBJECT_CLASS(k)->finalize = wenuy_chart_finalize;
    G_OBJECT_CLASS(k)->dispose = wenuy_chart_dispose;
    gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(k), "wenuychart");
    gtk_widget_class_set_accessible_role(GTK_WIDGET_CLASS(k), GTK_ACCESSIBLE_ROLE_IMG);
}
static void wenuy_chart_init(WenuyChart* self) {
    self->values = new std::vector<double>;
    self->labels = new std::vector<std::string>;
    self->grow = 1;
    g_signal_connect_object(wenuy_theme_get_default(), "changed", G_CALLBACK(gtk_widget_queue_draw), self, G_CONNECT_SWAPPED);
}
GtkWidget* wenuy_chart_new(WenuyChartKind kind) {
    auto* self = WENUY_CHART(g_object_new(WENUY_TYPE_CHART, nullptr));
    self->kind = kind;
    return GTK_WIDGET(self);
}
void wenuy_chart_set_data(WenuyChart* self, const double* values, const char* const* labels, guint n) {
    self->values->assign(values, values + n);
    self->labels->clear();
    if (labels) for (guint i = 0; i < n; ++i) self->labels->push_back(labels[i] ? labels[i] : "");
    if (self->anim) { adw_animation_skip(self->anim); g_clear_object(&self->anim); }
    const int lvl = wenuy_motion_level();
    if (lvl == 0) { self->grow = 1; gtk_widget_queue_draw(GTK_WIDGET(self)); return; }
    auto* t = adw_callback_animation_target_new(+[](double v, gpointer d) {
        auto* c = WENUY_CHART(d); c->grow = v; gtk_widget_queue_draw(GTK_WIDGET(c));
    }, self, nullptr);
    self->anim = adw_timed_animation_new(GTK_WIDGET(self), 0, 1, lvl == 2 ? 650 : 200, t);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(self->anim), ADW_EASE_OUT_CUBIC);
    adw_animation_play(self->anim);
}

// ================================================================ hjälpare
GtkWidget* wenuy_empty_state(const char* icon, const char* title, const char* description,
                             const char* button_label, GCallback on_click, gpointer data) {
    auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    auto* ic = wenuy_icon_new(icon ? icon : "info", 56);
    gtk_widget_add_css_class(ic, "dim-label");
    gtk_box_append(GTK_BOX(box), ic);
    auto* t = gtk_label_new(title);
    gtk_widget_add_css_class(t, "title-2");
    gtk_box_append(GTK_BOX(box), t);
    if (description) {
        auto* d = gtk_label_new(description);
        gtk_label_set_wrap(GTK_LABEL(d), TRUE);
        gtk_label_set_justify(GTK_LABEL(d), GTK_JUSTIFY_CENTER);
        gtk_label_set_max_width_chars(GTK_LABEL(d), 46);
        gtk_widget_add_css_class(d, "dim-label");
        gtk_box_append(GTK_BOX(box), d);
    }
    if (button_label) {
        auto* b = gtk_button_new_with_label(button_label);
        gtk_widget_add_css_class(b, "suggested-action");
        gtk_widget_add_css_class(b, "pill");
        gtk_widget_set_halign(b, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(b, 8);
        if (on_click) g_signal_connect(b, "clicked", on_click, data);
        gtk_box_append(GTK_BOX(box), b);
    }
    return box;
}
GtkWidget* wenuy_page_stack_new(void) {
    auto* st = gtk_stack_new();
    const int lvl = wenuy_motion_level();
    gtk_stack_set_transition_type(GTK_STACK(st), lvl == 2 ? GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT
                                               : lvl == 1 ? GTK_STACK_TRANSITION_TYPE_CROSSFADE : GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_stack_set_transition_duration(GTK_STACK(st), lvl == 2 ? 220 : 120);
    return st;
}

GtkWidget* wenuy_window_actions_button(void) {
    auto* b = gtk_button_new();
    gtk_widget_add_css_class(b, "flat");   // som övriga titelradsknappar
    gtk_button_set_child(GTK_BUTTON(b), wenuy_icon_new("more", 18));
    gtk_widget_set_tooltip_text(b, wenuy::tr("Window functions (Super+.)", "Fönsterfunktioner (Super+.)"));
    g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) { wenuy::spawn({"wenuyos-panel", "--actions"}); }), nullptr);
    return b;
}

// Programmets ikonnamn från dess .desktop-fil (se.wenuyos.Settings → wenuyos-settings.desktop)
static std::string appIconName() {
    GApplication* app = g_application_get_default();
    const char* id = app ? g_application_get_application_id(app) : nullptr;
    if (!id) return "";
    std::string a = id, own = a;
    if (a.rfind("se.wenuyos.", 0) == 0) {
        own = "wenuyos-" + a.substr(11);
        for (auto& c : own) c = (char)g_ascii_tolower(c);
    }
    const std::string cands[2] = {a, own};
    for (const auto& d : cands) {
        GDesktopAppInfo* info = g_desktop_app_info_new((d + ".desktop").c_str());
        if (!info) continue;
        GIcon* ic = g_app_info_get_icon(G_APP_INFO(info));
        std::string r;
        if (G_IS_THEMED_ICON(ic)) {
            const char* const* names = g_themed_icon_get_names(G_THEMED_ICON(ic));
            if (names && names[0]) r = names[0];
        }
        g_object_unref(info);
        return r;
    }
    return "";
}

GtkWidget* wenuy_header_bar_new(void) {
    // Ikon och titel packas först i början av raden (inte som titelwidget – AdwHeaderBar centrerar den),
    // så titeln står direkt efter ikonen och programmets egna knappar kommer efter titeln.
    GtkWidget* hb = adw_header_bar_new();
    gtk_widget_add_css_class(hb, "wenuy-header");
    // ikonen ritas av oss (slot nedan) – systemets layout "icon:minimize,maximize,close" får inte rita en till
    adw_header_bar_set_decoration_layout(ADW_HEADER_BAR(hb), ":minimize,maximize,close");
    adw_header_bar_set_title_widget(ADW_HEADER_BAR(hb), gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));   // tom mitt
    GtkWidget* lead = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(lead, 6);
    gtk_widget_set_margin_end(lead, 6);
    GtkWidget* iconSlot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_valign(iconSlot, GTK_ALIGN_CENTER);
    GtkWidget* title = gtk_label_new("");
    gtk_widget_add_css_class(title, "title");
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(title), 40);
    gtk_box_append(GTK_BOX(lead), iconSlot);
    gtk_box_append(GTK_BOX(lead), title);
    adw_header_bar_pack_start(ADW_HEADER_BAR(hb), lead);
    g_object_set_data(G_OBJECT(hb), "wenuy-title", title);
    g_object_set_data(G_OBJECT(hb), "wenuy-icon-slot", iconSlot);
    // Titeln följer fönstret (sätts ofta efter att titelraden skapats); ikonen från programmets .desktop-fil
    g_signal_connect(hb, "realize", G_CALLBACK(+[](GtkWidget* w, gpointer t) {
        GtkRoot* root = gtk_widget_get_root(w);
        if (!GTK_IS_WINDOW(root)) return;
        auto* slot = GTK_WIDGET(g_object_get_data(G_OBJECT(w), "wenuy-icon-slot"));
        if (slot && !gtk_widget_get_first_child(slot)) {
            std::string icon = appIconName();
            if (!icon.empty()) {
                GtkWidget* img = gtk_image_new_from_icon_name(icon.c_str());
                gtk_image_set_pixel_size(GTK_IMAGE(img), 20);
                gtk_box_append(GTK_BOX(slot), img);
            } else {
                gtk_widget_set_visible(slot, FALSE);
            }
        }
        auto sync = +[](GObject* win, GParamSpec*, gpointer lbl) {
            if (g_object_get_data(G_OBJECT(lbl), "wenuy-fixed")) return;   // fast titel (wenuy_header_bar_set_title)
            const char* s = gtk_window_get_title(GTK_WINDOW(win));
            gtk_label_set_text(GTK_LABEL(lbl), s ? s : "");
        };
        sync(G_OBJECT(root), nullptr, t);
        g_signal_connect_object(root, "notify::title", G_CALLBACK(sync), t, (GConnectFlags)0);
    }), title);
    return hb;
}

void wenuy_header_bar_set_title(GtkWidget* hb, const char* title) {
    GtkWidget* t = GTK_WIDGET(g_object_get_data(G_OBJECT(hb), "wenuy-title"));
    if (!t || !GTK_IS_LABEL(t)) return;
    g_object_set_data(G_OBJECT(t), "wenuy-fixed", GINT_TO_POINTER(1));
    gtk_label_set_text(GTK_LABEL(t), title ? title : "");
}

// ================================================================ WenuyOS-fönstret
// Samma grund för alla program: titelrad med ikon + fet titel till vänster, innehåll,
// statusrad och toast – med eller utan sidopanel. Utseendet (linjer, höjder, knappgrupp)
// kommer från temat (wenuyos-theme).
namespace {
struct AppWin {
    GtkWidget *header = nullptr, *toasts = nullptr, *status = nullptr, *statusStart = nullptr, *statusEnd = nullptr;
    GtkWidget *contentTv = nullptr, *topArea = nullptr, *bottomArea = nullptr, *split = nullptr, *sideHeader = nullptr, *sideTv = nullptr, *contentTitle = nullptr, *contentPage = nullptr;
    AdwBreakpoint* bp = nullptr;
    WenuySidebarMode mode = WENUY_SIDEBAR_OVERLAY;
    ~AppWin() { if (contentTitle) g_object_remove_weak_pointer(G_OBJECT(contentTitle), reinterpret_cast<gpointer*>(&contentTitle)); }
};
AppWin* appWin(GtkWidget* w) { return w ? static_cast<AppWin*>(g_object_get_data(G_OBJECT(w), "wenuy-app-window")) : nullptr; }

GtkWidget* baseWindow(GtkApplication* app, const char* title, AppWin** out) {
    ensureCss();
    GtkWidget* win = adw_application_window_new(app);
    gtk_widget_add_css_class(win, "wenuy-window");
    if (title) gtk_window_set_title(GTK_WINDOW(win), title);
    gtk_window_set_default_size(GTK_WINDOW(win), 900, 640);
    gtk_widget_set_size_request(win, 360, 360);   // minsta storlek (krävs av brytpunkterna)
    gtk_window_set_resizable(GTK_WINDOW(win), TRUE);
    auto* d = new AppWin{};
    g_object_set_data_full(G_OBJECT(win), "wenuy-app-window", d, +[](gpointer p) { delete static_cast<AppWin*>(p); });
    *out = d;
    return win;
}
// Innehållsdelen: titelrad överst, toast-yta, statusrad nederst
GtkWidget* contentPart(AppWin* d, GtkWidget* header) {
    GtkWidget* tv = adw_toolbar_view_new();
    d->contentTv = tv;
    d->header = header;
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tv), header);
    // Extra rader (menyrad, verktyg, Tillbaka/Nästa) ligger i innehållet, inte som fler toolbar-rader:
    // varje extra toolbar-rad flyttar titelradens underkant 3 px (då hamnar linjerna ur led)
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    d->topArea = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    d->bottomArea = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    d->toasts = adw_toast_overlay_new();
    gtk_widget_set_vexpand(d->toasts, TRUE);
    gtk_box_append(GTK_BOX(box), d->topArea);
    gtk_box_append(GTK_BOX(box), d->toasts);
    gtk_box_append(GTK_BOX(box), d->bottomArea);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tv), box);
    d->status = wenuy_status_bar_new();
    d->statusStart = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(d->statusStart), 0);
    gtk_label_set_ellipsize(GTK_LABEL(d->statusStart), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(d->statusStart, TRUE);
    d->statusEnd = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(d->statusEnd), 1);
    gtk_box_append(GTK_BOX(d->status), d->statusStart);
    gtk_box_append(GTK_BOX(d->status), d->statusEnd);
    gtk_widget_set_visible(d->status, FALSE);
    adw_toolbar_view_add_bottom_bar(ADW_TOOLBAR_VIEW(tv), d->status);
    return tv;
}
}  // namespace

GtkWidget* wenuy_status_bar_new(void) {
    ensureCss();
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(bar, "wenuy-statusbar");
    return bar;
}

GtkWidget* wenuy_app_window_new(GtkApplication* app, const char* title) {
    AppWin* d;
    GtkWidget* win = baseWindow(app, title, &d);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(win), contentPart(d, wenuy_header_bar_new()));
    return win;
}

GtkWidget* wenuy_app_window_new_with_sidebar(GtkApplication* app, const char* title, WenuySidebarMode mode) {
    AppWin* d;
    GtkWidget* win = baseWindow(app, title, &d);
    d->mode = mode;
    gtk_window_set_default_size(GTK_WINDOW(win), 980, 640);
    // sidopanelen: ikon + programnamn
    d->sideTv = adw_toolbar_view_new();
    d->sideHeader = wenuy_header_bar_new();
    wenuy_header_bar_set_title(d->sideHeader, title);
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(d->sideTv), d->sideHeader);
    // innehållet: egen titelrad (sidans titel i mitten, programmets knappar)
    GtkWidget* header = adw_header_bar_new();
    adw_header_bar_set_decoration_layout(ADW_HEADER_BAR(header), ":minimize,maximize,close");
    d->contentTitle = adw_window_title_new("", nullptr);
    adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), d->contentTitle);
    // programmet kan byta titelwidget (t.ex. Filers adressfält) – då ska pekaren bli NULL, inte peka på borttagen widget
    g_object_add_weak_pointer(G_OBJECT(d->contentTitle), reinterpret_cast<gpointer*>(&d->contentTitle));
    GtkWidget* contentTv = contentPart(d, header);
    // titelraderna alltid exakt lika höga → underkanterna bildar en obruten linje
    GtkSizeGroup* hg = gtk_size_group_new(GTK_SIZE_GROUP_VERTICAL);
    gtk_size_group_add_widget(hg, d->sideHeader);
    gtk_size_group_add_widget(hg, header);
    g_object_unref(hg);

    GValue on = G_VALUE_INIT;
    g_value_init(&on, G_TYPE_BOOLEAN);
    g_value_set_boolean(&on, TRUE);
    d->bp = adw_breakpoint_new(adw_breakpoint_condition_parse("max-width: 640sp"));
    if (mode == WENUY_SIDEBAR_PAGES) {
        d->split = adw_navigation_split_view_new();
        auto* side = adw_navigation_page_new(d->sideTv, title ? title : "");
        d->contentPage = GTK_WIDGET(adw_navigation_page_new(contentTv, title ? title : ""));
        adw_navigation_split_view_set_sidebar(ADW_NAVIGATION_SPLIT_VIEW(d->split), side);
        adw_navigation_split_view_set_content(ADW_NAVIGATION_SPLIT_VIEW(d->split), ADW_NAVIGATION_PAGE(d->contentPage));
        adw_navigation_split_view_set_min_sidebar_width(ADW_NAVIGATION_SPLIT_VIEW(d->split), 220);
        adw_navigation_split_view_set_max_sidebar_width(ADW_NAVIGATION_SPLIT_VIEW(d->split), 260);
        adw_breakpoint_add_setter(d->bp, G_OBJECT(d->split), "collapsed", &on);
    } else {
        d->split = adw_overlay_split_view_new();
        adw_overlay_split_view_set_sidebar(ADW_OVERLAY_SPLIT_VIEW(d->split), d->sideTv);
        adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(d->split), contentTv);
        adw_overlay_split_view_set_min_sidebar_width(ADW_OVERLAY_SPLIT_VIEW(d->split), 200);
        adw_overlay_split_view_set_max_sidebar_width(ADW_OVERLAY_SPLIT_VIEW(d->split), 240);
        adw_breakpoint_add_setter(d->bp, G_OBJECT(d->split), "collapsed", &on);
        // hopfällt: knapp i titelraden tar fram sidopanelen
        GtkWidget* toggle = gtk_toggle_button_new();
        gtk_button_set_icon_name(GTK_BUTTON(toggle), "sidebar-show-symbolic");
        gtk_widget_add_css_class(toggle, "flat");
        gtk_widget_set_tooltip_text(toggle, wenuy::tr("Show sidebar", "Visa sidopanelen"));
        adw_header_bar_pack_start(ADW_HEADER_BAR(header), toggle);
        g_object_bind_property(d->split, "collapsed", toggle, "visible", G_BINDING_SYNC_CREATE);
        g_object_bind_property(d->split, "show-sidebar", toggle, "active", GBindingFlags(G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE));
    }
    g_value_unset(&on);
    adw_application_window_add_breakpoint(ADW_APPLICATION_WINDOW(win), d->bp);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(win), d->split);
    return win;
}

GtkWidget* wenuy_app_window_get_header_bar(GtkWidget* w) { AppWin* d = appWin(w); return d ? d->header : nullptr; }
GtkWidget* wenuy_app_window_get_sidebar_header_bar(GtkWidget* w) { AppWin* d = appWin(w); return d ? d->sideHeader : nullptr; }
GObject* wenuy_app_window_get_breakpoint(GtkWidget* w) { AppWin* d = appWin(w); return d && d->bp ? G_OBJECT(d->bp) : nullptr; }

void wenuy_app_window_set_content(GtkWidget* w, GtkWidget* content) {
    if (AppWin* d = appWin(w)) adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(d->toasts), content);
}

void wenuy_app_window_set_sidebar(GtkWidget* w, GtkWidget* sidebar) {
    if (AppWin* d = appWin(w); d && d->sideTv) adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(d->sideTv), sidebar);
}

void wenuy_app_window_set_content_title(GtkWidget* w, const char* title) {
    AppWin* d = appWin(w);
    if (!d || !d->contentTitle) return;
    adw_window_title_set_title(ADW_WINDOW_TITLE(d->contentTitle), title ? title : "");
    if (d->contentPage) adw_navigation_page_set_title(ADW_NAVIGATION_PAGE(d->contentPage), title ? title : "");
}

void wenuy_app_window_show_content(GtkWidget* w) {
    AppWin* d = appWin(w);
    if (!d || !d->split) return;
    if (d->mode == WENUY_SIDEBAR_PAGES) adw_navigation_split_view_set_show_content(ADW_NAVIGATION_SPLIT_VIEW(d->split), TRUE);
    else if (adw_overlay_split_view_get_collapsed(ADW_OVERLAY_SPLIT_VIEW(d->split)))
        adw_overlay_split_view_set_show_sidebar(ADW_OVERLAY_SPLIT_VIEW(d->split), FALSE);
}

void wenuy_app_window_add_top_bar(GtkWidget* w, GtkWidget* bar) {
    if (AppWin* d = appWin(w)) gtk_box_append(GTK_BOX(d->topArea), bar);
}

void wenuy_app_window_add_bottom_bar(GtkWidget* w, GtkWidget* bar) {
    if (AppWin* d = appWin(w)) gtk_box_append(GTK_BOX(d->bottomArea), bar);   // ovanför statusraden
}

void wenuy_app_window_set_status(GtkWidget* w, const char* start, const char* end) {
    AppWin* d = appWin(w);
    if (!d) return;
    gtk_label_set_text(GTK_LABEL(d->statusStart), start ? start : "");
    gtk_label_set_text(GTK_LABEL(d->statusEnd), end ? end : "");
    gtk_widget_set_visible(d->status, start || end);
}

void wenuy_app_window_toast(GtkWidget* w, const char* text) {
    if (AppWin* d = appWin(w)) adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(d->toasts), adw_toast_new(text));
}
void wenuy_app_window_toast_action(GtkWidget* w, const char* text, const char* button, GCallback clicked, gpointer data) {
    AppWin* d = appWin(w);
    if (!d) return;
    AdwToast* t = adw_toast_new(text);
    if (button && clicked) {
        adw_toast_set_button_label(t, button);
        g_signal_connect(t, "button-clicked", clicked, data);
        adw_toast_set_timeout(t, 8);
    }
    adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(d->toasts), t);
}
