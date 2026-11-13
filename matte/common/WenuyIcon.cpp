#include "WenuyIcon.h"
#include "WenuyTheme.h"

#include <adwaita.h>

#include <cmath>
#include <cstring>
#include <string>

#include "IconData.h"
#include "Wenuy.h"

struct _WenuyIcon {
    GtkWidget parent_instance;
    const wenuy::icons::Def* def;
    GskPath* stroke;
    GskPath* fill;
    float length;        // streckets totala längd (för inritningen)
    int size;
    gboolean beacon, accent;
    // animationstillstånd
    double draw;         // 0..1 hur mycket av strecket som ritats
    double lift;         // 0 = vila, 1 = hover
    double press;        // 0..1
    double motion;       // 0..1 signaturrörelsens fas
    double fade;         // 1 = synlig (korstoning vid namnbyte)
    AdwAnimation *drawAnim, *liftAnim, *motionAnim, *fadeAnim;
    int motionLevel;     // 0 av, 1 reducerad, 2 full
};
G_DEFINE_FINAL_TYPE(WenuyIcon, wenuy_icon, GTK_TYPE_WIDGET)

namespace {

int motionLevel() {
    std::string m = wenuy::readLine(std::string(g_get_user_config_dir()) + "/wenuyos/motion");
    if (m == "off") return 0;
    if (m == "reduced" || wenuy::lowProfile()) return 1;
    gboolean anim = TRUE;
    g_object_get(gtk_settings_get_default(), "gtk-enable-animations", &anim, nullptr);
    return anim ? 2 : 0;
}

AdwAnimation* timed(GtkWidget* w, double* target, double from, double to, guint ms, AdwEasing ease) {
    auto* t = adw_callback_animation_target_new(+[](double v, gpointer d) { *static_cast<double*>(d) = v; }, target, nullptr);
    auto* a = adw_timed_animation_new(w, from, to, ms, t);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(a), ease);
    g_signal_connect_swapped(a, "notify::value", G_CALLBACK(gtk_widget_queue_draw), w);
    return a;
}
AdwAnimation* spring(GtkWidget* w, double* target, double from, double to) {
    auto* t = adw_callback_animation_target_new(+[](double v, gpointer d) { *static_cast<double*>(d) = v; }, target, nullptr);
    auto* a = adw_spring_animation_new(w, from, to, adw_spring_params_new(0.6, 1, 260), t);
    g_signal_connect_swapped(a, "notify::value", G_CALLBACK(gtk_widget_queue_draw), w);
    return a;
}
void stop(AdwAnimation*& a) { if (a) { adw_animation_skip(a); g_object_unref(a); a = nullptr; } }

void load(WenuyIcon* self, const char* name) {
    g_clear_pointer(&self->stroke, gsk_path_unref);
    g_clear_pointer(&self->fill, gsk_path_unref);
    self->def = wenuy::icons::find(name);
    if (!self->def) return;
    if (*self->def->stroke) self->stroke = gsk_path_parse(self->def->stroke);
    if (*self->def->fill) self->fill = gsk_path_parse(self->def->fill);
    self->length = 0;
    if (self->stroke) {
        GskPathMeasure* m = gsk_path_measure_new(self->stroke);
        self->length = gsk_path_measure_get_length(m);
        gsk_path_measure_unref(m);
    }
}

void snapshot(GtkWidget* w, GtkSnapshot* s) {
    auto* self = WENUY_ICON(w);
    if (!self->def) return;
    const float W = gtk_widget_get_width(w), H = gtk_widget_get_height(w);
    const float sz = std::min(W, H);
    GdkRGBA color;
    gtk_widget_get_color(w, &color);
    if (self->accent) wenuy_theme_get_accent(wenuy_theme_get_default(), &color);
    color.alpha *= (float)self->fade;

    // Rörelse: lyft, tryck, signatur
    double scale = 1.0 + 0.10 * self->lift - 0.08 * self->press;
    double rot = 0, dy = 0;
    if (strcmp(self->def->motion, "spin") == 0) rot = 360.0 * self->motion;
    if (strcmp(self->def->motion, "pulse") == 0) scale *= 1.0 + 0.12 * std::sin(self->motion * M_PI);
    if (strcmp(self->def->motion, "drop") == 0) dy = -3.0 * std::sin(self->motion * M_PI) * (sz / 24.0);

    gtk_snapshot_save(s);
    graphene_point_t c1 = GRAPHENE_POINT_INIT(W / 2, H / 2 + (float)dy);
    gtk_snapshot_translate(s, &c1);
    gtk_snapshot_rotate(s, (float)rot);
    gtk_snapshot_scale(s, (float)(sz / 24.0 * scale), (float)(sz / 24.0 * scale));
    graphene_point_t c2 = GRAPHENE_POINT_INIT(-12, -12);
    gtk_snapshot_translate(s, &c2);

    if (self->stroke) {
        GskStroke* st = gsk_stroke_new(2.0f);
        gsk_stroke_set_line_cap(st, GSK_LINE_CAP_ROUND);
        gsk_stroke_set_line_join(st, GSK_LINE_JOIN_ROUND);
        if (self->draw < 1.0 && self->length > 0) {   // inritning: synlig del = streckade mönstret
            float dash[2] = {(float)(self->length * self->draw), self->length};
            gsk_stroke_set_dash(st, dash, 2);
        }
        gtk_snapshot_append_stroke(s, self->stroke, st, &color);
        gsk_stroke_free(st);
    }
    if (self->fill) {
        GdkRGBA c = color;
        c.alpha *= (float)std::min(1.0, self->draw * 1.5);   // fyllningar tonar in sist
        gtk_snapshot_append_fill(s, self->fill, GSK_FILL_RULE_WINDING, &c);
    }
    if (self->def->bx >= 0) {                                  // fyren
        GdkRGBA c;
        gdk_rgba_parse(&c, self->beacon ? "#10B981" : "#9AA0AC");
        c.alpha = color.alpha;
        GskPathBuilder* b = gsk_path_builder_new();
        graphene_point_t bc = GRAPHENE_POINT_INIT(self->def->bx, self->def->by);
        gsk_path_builder_add_circle(b, &bc, 2.0f);
        GskPath* p = gsk_path_builder_free_to_path(b);
        gtk_snapshot_append_fill(s, p, GSK_FILL_RULE_WINDING, &c);
        gsk_path_unref(p);
    }
    gtk_snapshot_restore(s);
}

void measure(GtkWidget* w, GtkOrientation, int, int* min, int* nat, int*, int*) {
    *min = *nat = WENUY_ICON(w)->size;
}

void map(GtkWidget* w) {
    GTK_WIDGET_CLASS(wenuy_icon_parent_class)->map(w);
    auto* self = WENUY_ICON(w);
    self->motionLevel = motionLevel();
    stop(self->drawAnim);
    if (self->motionLevel == 2) {
        self->draw = 0;
        self->drawAnim = timed(w, &self->draw, 0, 1, 320, ADW_EASE_OUT_CUBIC);
        adw_animation_play(self->drawAnim);
    } else if (self->motionLevel == 1) {
        self->draw = 1; self->fade = 0;
        stop(self->fadeAnim);
        self->fadeAnim = timed(w, &self->fade, 0, 1, 180, ADW_EASE_OUT_CUBIC);
        adw_animation_play(self->fadeAnim);
    } else {
        self->draw = 1; self->fade = 1;
    }
}

void hover(WenuyIcon* self, bool in) {
    if (self->motionLevel < 2) return;
    stop(self->liftAnim);
    self->liftAnim = spring(GTK_WIDGET(self), &self->lift, self->lift, in ? 1 : 0);
    adw_animation_play(self->liftAnim);
    if (in) wenuy_icon_play(self);
}

void dispose(GObject* o) {
    auto* self = WENUY_ICON(o);
    stop(self->drawAnim); stop(self->liftAnim); stop(self->motionAnim); stop(self->fadeAnim);
    g_clear_pointer(&self->stroke, gsk_path_unref);
    g_clear_pointer(&self->fill, gsk_path_unref);
    G_OBJECT_CLASS(wenuy_icon_parent_class)->dispose(o);
}

}  // namespace

static void wenuy_icon_class_init(WenuyIconClass* k) {
    GTK_WIDGET_CLASS(k)->snapshot = snapshot;
    GTK_WIDGET_CLASS(k)->measure = measure;
    GTK_WIDGET_CLASS(k)->map = map;
    G_OBJECT_CLASS(k)->dispose = dispose;
}
static void wenuy_icon_init(WenuyIcon* self) {
    self->size = 24; self->draw = 1; self->fade = 1; self->motionLevel = 2;
    g_signal_connect_object(wenuy_theme_get_default(), "changed", G_CALLBACK(gtk_widget_queue_draw), self, G_CONNECT_SWAPPED);
    gtk_widget_set_halign(GTK_WIDGET(self), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(self), GTK_ALIGN_CENTER);
    auto* hc = gtk_event_controller_motion_new();
    g_signal_connect_swapped(hc, "enter", G_CALLBACK(+[](WenuyIcon* s) { hover(s, true); }), self);
    g_signal_connect_swapped(hc, "leave", G_CALLBACK(+[](WenuyIcon* s) { hover(s, false); }), self);
    gtk_widget_add_controller(GTK_WIDGET(self), hc);
    auto* click = gtk_gesture_click_new();
    g_signal_connect_swapped(click, "pressed", G_CALLBACK(+[](WenuyIcon* s) {
        if (s->motionLevel < 2) return;
        s->press = 1; gtk_widget_queue_draw(GTK_WIDGET(s));
    }), self);
    g_signal_connect_swapped(click, "released", G_CALLBACK(+[](WenuyIcon* s) {
        stop(s->liftAnim); s->press = 0;
        s->liftAnim = spring(GTK_WIDGET(s), &s->lift, 1.3, 0); adw_animation_play(s->liftAnim);
    }), self);
    gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(click), FALSE);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(click), GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(click));
}

GtkWidget* wenuy_icon_new(const char* name, int size) {
    auto* self = WENUY_ICON(g_object_new(WENUY_TYPE_ICON, nullptr));
    self->size = size;
    load(self, name);
    return GTK_WIDGET(self);
}
void wenuy_icon_set_name(WenuyIcon* self, const char* name) {
    if (self->def && strcmp(self->def->name, name) == 0) return;
    load(self, name);
    if (self->motionLevel > 0 && gtk_widget_get_mapped(GTK_WIDGET(self))) {
        stop(self->fadeAnim);
        self->fade = 0.2;
        self->fadeAnim = timed(GTK_WIDGET(self), &self->fade, 0.2, 1, 200, ADW_EASE_OUT_CUBIC);
        adw_animation_play(self->fadeAnim);
    }
    gtk_widget_queue_draw(GTK_WIDGET(self));
}
void wenuy_icon_set_beacon(WenuyIcon* self, gboolean on) { self->beacon = on; gtk_widget_queue_draw(GTK_WIDGET(self)); }
void wenuy_icon_set_accent(WenuyIcon* self, gboolean on) { self->accent = on; gtk_widget_queue_draw(GTK_WIDGET(self)); }
void wenuy_icon_play(WenuyIcon* self) {
    if (self->motionLevel < 2 || !self->def || strcmp(self->def->motion, "lift") == 0) return;
    stop(self->motionAnim);
    self->motionAnim = timed(GTK_WIDGET(self), &self->motion, 0, 1, strcmp(self->def->motion, "spin") == 0 ? 600 : 400, ADW_EASE_IN_OUT_CUBIC);
    adw_animation_play(self->motionAnim);
}

// ---- färgikoner (lerstil, ikontemat Wenuy/color): samma ikonnamn som streckikonerna
namespace {
struct ColorMap { const char* w; const char* color; };
const ColorMap kColor[] = {
    {"globe", "browser"}, {"image", "pictures"}, {"headphones", "headphones"}, {"note", "text"},
    {"code", "studio"}, {"phone", "phone"}, {"settings", "settings"}, {"install", "installer"},
    {"wifi", "wifi"}, {"wifi-3", "wifi"}, {"bluetooth", "bluetooth"}, {"display", "computer"},
    {"laptop", "laptop"}, {"volume", "volume"}, {"volume-2", "volume"}, {"mouse", "mouse"},
    {"keyboard", "keyboard"}, {"printer", "printer"}, {"battery", "battery"}, {"battery-2", "battery"},
    {"bell", "notification"}, {"language", "language"}, {"apps", "blocks"}, {"users", "users"},
    {"accessibility", "accessibility"}, {"drive", "drive"}, {"gauge", "stopwatch"}, {"shield", "security"},
    {"update", "update"}, {"computer", "computer"}, {"folder", "files"}, {"terminal", "terminal"},
    {"lock", "lock"}, {"power", "power"}, {"search", "search"}, {"help", "help"}, {"info", "info"},
    {"trash", "trash"}, {"mail", "mail"}, {"calendar", "calendar"}, {"calculator", "calculator"},
    {"music", "music"}, {"video", "video"}, {"camera", "camera"}, {"home", "folder-home"},
    {"documents", "folder-documents"}, {"downloads", "folder-downloads"}, {"tools", "dock-tools"},
    {"palette", "palette"}, {"privacy", "security"}, {"window", "laptop"}, {"controller", "controller"},
    {"storage", "drive"}, {"chess", "chess"}, {"android", "phone"},
};
}  // namespace

GtkWidget* wenuy_color_icon_new(const char* name, int size) {
    g_return_val_if_fail(name, nullptr);
    const char* id = nullptr;
    for (const auto& m : kColor) if (!strcmp(m.w, name)) { id = m.color; break; }
    std::string themed = std::string("wenuy-") + (id ? id : name);
    GdkDisplay* d = gdk_display_get_default();
    if (d && gtk_icon_theme_has_icon(gtk_icon_theme_get_for_display(d), themed.c_str())) {
        GtkWidget* img = gtk_image_new_from_icon_name(themed.c_str());
        gtk_image_set_pixel_size(GTK_IMAGE(img), size);
        gtk_widget_add_css_class(img, "wenuy-color-icon");
        return img;
    }
    return wenuy_icon_new(name, size);   // ingen färgikon: streckikonen
}
