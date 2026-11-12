// libwenuy – färgikon med rörelse. Ritas av grafikkortet (GtkSnapshot-transformer), ingen extra bild per steg.
#include "WenuyMotionIcon.h"

#include <cmath>
#include <string>

#include "WenuyTheme.h"

struct _WenuyMotionIcon {
    GtkWidget parent;
    std::string* name;
    std::string* motion;
    int size;
    double t;          // 0..1 inom en rörelse
    bool running;      // upprepa
    bool playing;
    guint tick;
    gint64 start;
};
G_DEFINE_FINAL_TYPE(WenuyMotionIcon, wenuy_motion_icon, GTK_TYPE_WIDGET)

static double durationFor(const std::string& m) {
    if (m == "spin") return 1.2;
    if (m == "ring") return 0.9;
    return 0.6;
}

static void measure(GtkWidget* w, GtkOrientation, int, int* min, int* nat, int*, int*) {
    *min = *nat = WENUY_MOTION_ICON(w)->size;
}

static void snapshot(GtkWidget* w, GtkSnapshot* s) {
    auto* self = WENUY_MOTION_ICON(w);
    const float W = gtk_widget_get_width(w), H = gtk_widget_get_height(w);
    const float sz = std::min(W, H);
    GtkIconPaintable* icon = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_for_display(gtk_widget_get_display(w)),
                                                        self->name->c_str(), nullptr, int(sz), gtk_widget_get_scale_factor(w),
                                                        gtk_widget_get_direction(w), GTK_ICON_LOOKUP_PRELOAD);
    if (!icon) return;
    const double t = self->playing ? self->t : 0;
    const std::string& m = *self->motion;
    gtk_snapshot_save(s);
    graphene_point_t c = GRAPHENE_POINT_INIT(W / 2, H / 2);
    if (m == "spin") {
        // mjuk start och stopp om den inte upprepas
        const double e = self->running ? t : 0.5 - 0.5 * std::cos(M_PI * t);
        gtk_snapshot_translate(s, &c);
        gtk_snapshot_rotate(s, float(360.0 * e));
        graphene_point_t back = GRAPHENE_POINT_INIT(-W / 2, -H / 2);
        gtk_snapshot_translate(s, &back);
    } else if (m == "ring") {
        // pendel kring överkanten, avtagande
        const double a = 18.0 * std::sin(t * M_PI * 6) * (1 - t);
        graphene_point_t top = GRAPHENE_POINT_INIT(W / 2, H * 0.12f);
        gtk_snapshot_translate(s, &top);
        gtk_snapshot_rotate(s, float(a));
        graphene_point_t back = GRAPHENE_POINT_INIT(-W / 2, -H * 0.12f);
        gtk_snapshot_translate(s, &back);
    } else if (m == "receive") {
        // tryck ihop och studsa tillbaka
        const double sq = 1 - 0.12 * std::sin(t * M_PI) * (1 - t * 0.5);
        graphene_point_t bottom = GRAPHENE_POINT_INIT(W / 2, H);
        gtk_snapshot_translate(s, &bottom);
        gtk_snapshot_scale(s, float(1 + (1 - sq) * 0.5), float(sq));
        graphene_point_t back = GRAPHENE_POINT_INIT(-W / 2, -H);
        gtk_snapshot_translate(s, &back);
    } else if (m == "open") {
        // lyft och luta lite, som när mappen öppnas
        const double k = std::sin(t * M_PI);
        graphene_point_t lift = GRAPHENE_POINT_INIT(W / 2, H / 2 - float(4 * k));
        gtk_snapshot_translate(s, &lift);
        gtk_snapshot_scale(s, float(1 + 0.08 * k), float(1 + 0.08 * k));
        gtk_snapshot_rotate(s, float(-4 * k));
        graphene_point_t back = GRAPHENE_POINT_INIT(-W / 2, -H / 2);
        gtk_snapshot_translate(s, &back);
    }
    graphene_point_t off = GRAPHENE_POINT_INIT((W - sz) / 2, (H - sz) / 2);
    gtk_snapshot_translate(s, &off);
    gdk_paintable_snapshot(GDK_PAINTABLE(icon), s, sz, sz);
    gtk_snapshot_restore(s);
    g_object_unref(icon);
    (void)c;
}

static gboolean onTick(GtkWidget* w, GdkFrameClock* clock, gpointer) {
    auto* self = WENUY_MOTION_ICON(w);
    const double d = durationFor(*self->motion);
    double t = (gdk_frame_clock_get_frame_time(clock) - self->start) / 1e6 / d;
    if (t >= 1) {
        if (self->running) { self->start = gdk_frame_clock_get_frame_time(clock); t = 0; }
        else { self->playing = false; self->tick = 0; gtk_widget_queue_draw(w); return G_SOURCE_REMOVE; }
    }
    self->t = t;
    gtk_widget_queue_draw(w);
    return G_SOURCE_CONTINUE;
}

static void startTick(WenuyMotionIcon* self) {
    const char* level = wenuy_theme_get_motion(wenuy_theme_get_default());
    if (g_str_equal(level, "off") || !gtk_widget_get_mapped(GTK_WIDGET(self))) return;   // tyst och stilla
    self->playing = true;
    self->t = 0;
    GdkFrameClock* clock = gtk_widget_get_frame_clock(GTK_WIDGET(self));
    self->start = clock ? gdk_frame_clock_get_frame_time(clock) : g_get_monotonic_time();
    if (!self->tick) self->tick = gtk_widget_add_tick_callback(GTK_WIDGET(self), onTick, nullptr, nullptr);
}

void wenuy_motion_icon_play(WenuyMotionIcon* self) { startTick(self); }

void wenuy_motion_icon_set_running(WenuyMotionIcon* self, gboolean running) {
    self->running = running;
    if (running) startTick(self);   // stannar när varvet är klart om running = FALSE
}

void wenuy_motion_icon_set_icon_name(WenuyMotionIcon* self, const char* name) {
    *self->name = name ? name : "";
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void finalize(GObject* o) {
    auto* self = WENUY_MOTION_ICON(o);
    delete self->name;
    delete self->motion;
    G_OBJECT_CLASS(wenuy_motion_icon_parent_class)->finalize(o);
}

static void wenuy_motion_icon_class_init(WenuyMotionIconClass* k) {
    G_OBJECT_CLASS(k)->finalize = finalize;
    GTK_WIDGET_CLASS(k)->measure = measure;
    GTK_WIDGET_CLASS(k)->snapshot = snapshot;
    gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(k), "wenuy-motion-icon");
}

static void wenuy_motion_icon_init(WenuyMotionIcon* self) {
    self->name = new std::string;
    self->motion = new std::string;
    self->size = 48;
    g_signal_connect_object(wenuy_theme_get_default(), "changed", G_CALLBACK(gtk_widget_queue_draw), self, G_CONNECT_SWAPPED);
    // en kort rörelse när pekaren kommer in (som de andra Wenuy-ikonerna)
    GtkEventController* hover = gtk_event_controller_motion_new();
    g_signal_connect_swapped(hover, "enter", G_CALLBACK(+[](WenuyMotionIcon* s) {
        if (!s->running && g_strcmp0(wenuy_theme_get_motion(wenuy_theme_get_default()), "full") == 0) startTick(s);
    }), self);
    gtk_widget_add_controller(GTK_WIDGET(self), hover);
}

GtkWidget* wenuy_motion_icon_new(const char* icon_name, int size, const char* motion) {
    auto* self = WENUY_MOTION_ICON(g_object_new(WENUY_TYPE_MOTION_ICON, nullptr));
    *self->name = icon_name ? icon_name : "image-missing";
    self->size = size > 0 ? size : 48;
    std::string m = motion ? motion : "";
    if (m.empty()) {
        const std::string& n = *self->name;
        if (n.find("refresh") != std::string::npos || n.find("update") != std::string::npos || n.find("sync") != std::string::npos) m = "spin";
        else if (n.find("notification") != std::string::npos || n.find("bell") != std::string::npos || n == "alarm") m = "ring";
        else if (n.find("trash") != std::string::npos) m = "receive";
        else m = "open";
    }
    *self->motion = m;
    return GTK_WIDGET(self);
}
