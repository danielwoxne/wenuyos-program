#include "WenuyCanvas.h"

#include <adwaita.h>

#include <algorithm>
#include <cmath>

#include "Wenuy.h"
#include "WenuyWidgets.h"

struct _WenuyCanvas {
    GtkWidget parent;
    double cw = 800, ch = 600;           // innehållets storlek
    double zoom = 1, ox = 0, oy = 0;     // innehållets origo i ytans koordinater
    double zmin = 0.05, zmax = 32;
    bool paper = true, paperBg = true, fitWidth = false, fitted = false, space = false, panning = false;
    double px = 0, py = 0;               // pekarens position
    double panX = 0, panY = 0;           // vid start av panorering
    double pinchZoom = 1;
    // animation
    AdwAnimation* anim = nullptr;
    double aFrom = 1, aTo = 1, aAx = 0, aAy = 0, aCx = 0, aCy = 0;
    WenuyCanvasDrawFunc draw = nullptr;
    gpointer drawData = nullptr;
    GDestroyNotify drawDestroy = nullptr;
};

G_DEFINE_FINAL_TYPE(WenuyCanvas, wenuy_canvas, GTK_TYPE_WIDGET)

enum { SIG_PRESS, SIG_DRAG, SIG_RELEASE, SIG_ZOOM, N_SIGS };
static guint sigs[N_SIGS];

static void clampPan(WenuyCanvas* s) {
    const double w = gtk_widget_get_width(GTK_WIDGET(s)), h = gtk_widget_get_height(GTK_WIDGET(s));
    const double cw = s->cw * s->zoom, ch = s->ch * s->zoom, keep = 64;
    // Mindre än ytan: centrera. Annars: minst 64 px av innehållet ska synas.
    if (!s->paper) {   // ark/tabell: övre vänstra hörnet (A1) stannar vid kanten
        s->ox = cw <= w ? 0 : std::clamp(s->ox, w - cw, 0.0);
        s->oy = ch <= h ? 0 : std::clamp(s->oy, h - ch, 0.0);
        return;
    }
    if (cw <= w) s->ox = (w - cw) / 2;
    else s->ox = std::clamp(s->ox, keep - cw, w - keep);
    if (ch <= h) s->oy = (h - ch) / 2;
    else s->oy = std::clamp(s->oy, keep - ch, h - keep);
}

// Zooma så att innehållspunkten (cx, cy) hamnar vid ytans punkt (ax, ay)
static void applyZoom(WenuyCanvas* s, double z, double ax, double ay, double cx, double cy) {
    s->zoom = std::clamp(z, s->zmin, s->zmax);
    s->ox = ax - cx * s->zoom;
    s->oy = ay - cy * s->zoom;
    clampPan(s);
    gtk_widget_queue_draw(GTK_WIDGET(s));
    g_signal_emit(s, sigs[SIG_ZOOM], 0);
}

// Zooma till z så att innehållspunkten (cx, cy) hamnar vid ytans punkt (ax, ay), ev. animerat
static void animateTo(WenuyCanvas* s, double z, double ax, double ay, double cx, double cy, bool animate) {
    z = std::clamp(z, s->zmin, s->zmax);
    if (s->anim) adw_animation_skip(s->anim);
    if (!animate || wenuy_motion_level() == 0) { applyZoom(s, z, ax, ay, cx, cy); return; }
    s->aFrom = s->zoom; s->aTo = z; s->aAx = ax; s->aAy = ay; s->aCx = cx; s->aCy = cy;
    if (!s->anim) {
        auto* target = adw_callback_animation_target_new(+[](double v, gpointer p) {
            auto* s = WENUY_CANVAS(p);
            // logaritmisk interpolering känns jämn både vid 10 % och 800 %
            applyZoom(s, s->aFrom * std::pow(s->aTo / s->aFrom, v), s->aAx, s->aAy, s->aCx, s->aCy);
        }, s, nullptr);
        s->anim = adw_timed_animation_new(GTK_WIDGET(s), 0, 1, 180, target);
        adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(s->anim), ADW_EASE_OUT_CUBIC);
    }
    adw_timed_animation_set_duration(ADW_TIMED_ANIMATION(s->anim), wenuy_motion_level() == 1 ? 100 : 180);
    adw_animation_play(s->anim);
}

// Zooma kring ytans punkt (ax, ay) – det som är under pekaren stannar kvar
static void zoomAt(WenuyCanvas* s, double z, double ax, double ay, bool animate) {
    animateTo(s, z, ax, ay, (ax - s->ox) / s->zoom, (ay - s->oy) / s->zoom, animate);
}

static double fitZoom(WenuyCanvas* s) {
    const double w = gtk_widget_get_width(GTK_WIDGET(s)), h = gtk_widget_get_height(GTK_WIDGET(s));
    if (w < 2 || h < 2) return 1;
    const double m = s->paper ? 48 : 0;
    return std::max(s->zmin, std::min((w - m) / s->cw, (h - m) / s->ch));
}

static void snapshotCb(GtkWidget* w, GtkSnapshot* snap) {
    auto* s = WENUY_CANVAS(w);
    const double W = gtk_widget_get_width(w), H = gtk_widget_get_height(w);
    if (!s->fitted && W > 1) {           // första gången: hela innehållet syns
        s->fitted = true;
        s->zoom = s->paper ? std::min(1.0, fitZoom(s)) : 1.0;
        if (s->fitWidth && W > 1) {   // sidor (PDF): fyll bredden, börja överst
            s->zoom = std::clamp((W - 48) / s->cw, s->zmin, s->zmax);
            s->oy = 24;
        }
        clampPan(s);
        // meddela zoomnivån efter ritningen (inte mitt i den)
        g_idle_add_full(G_PRIORITY_DEFAULT, +[](gpointer p) -> gboolean {
            g_signal_emit(p, sigs[SIG_ZOOM], 0);
            return G_SOURCE_REMOVE;
        }, g_object_ref(s), g_object_unref);
    }
    graphene_rect_t all = GRAPHENE_RECT_INIT(0, 0, (float)W, (float)H);
    gtk_snapshot_push_clip(snap, &all);
    graphene_rect_t page = GRAPHENE_RECT_INIT((float)s->ox, (float)s->oy, (float)(s->cw * s->zoom), (float)(s->ch * s->zoom));
    if (s->paper && s->paperBg) {
        GskRoundedRect rr;
        gsk_rounded_rect_init_from_rect(&rr, &page, 2);
        GdkRGBA shadow = {0, 0, 0, 0.18f};
        gtk_snapshot_append_outset_shadow(snap, &rr, &shadow, 0, 2, 0, 10);
        GdkRGBA white = {1, 1, 1, 1};
        gtk_snapshot_append_color(snap, &white, &page);
        gtk_snapshot_push_clip(snap, &page);
    }
    if (s->draw) {
        // synlig del i innehållskoordinater
        const double x0 = std::max(0.0, -s->ox / s->zoom), y0 = std::max(0.0, -s->oy / s->zoom);
        const double x1 = (W - s->ox) / s->zoom, y1 = (H - s->oy) / s->zoom;
        graphene_rect_t vis = GRAPHENE_RECT_INIT((float)x0, (float)y0, (float)std::max(0.0, x1 - x0), (float)std::max(0.0, y1 - y0));
        gtk_snapshot_save(snap);
        graphene_point_t o = GRAPHENE_POINT_INIT((float)s->ox, (float)s->oy);
        gtk_snapshot_translate(snap, &o);
        gtk_snapshot_scale(snap, (float)s->zoom, (float)s->zoom);
        s->draw(s, snap, &vis, s->zoom, s->drawData);
        gtk_snapshot_restore(snap);
    }
    if (s->paper && s->paperBg) gtk_snapshot_pop(snap);
    gtk_snapshot_pop(snap);
}

static void sizeAllocateCb(GtkWidget* w, int, int, int) { clampPan(WENUY_CANVAS(w)); }

static double pressureOf(GtkGesture* g) {
    double p = 1;
    if (GdkEvent* e = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(g)))
        if (!gdk_event_get_axis(e, GDK_AXIS_PRESSURE, &p)) p = 1;
    return p;
}

static void wenuy_canvas_init(WenuyCanvas* s) {
    s->cw = 800; s->ch = 600;   // GObject nollställer structen – sätt standardvärden här s->zoom = 1; s->ox = s->oy = 0; s->zmin = 0.05; s->zmax = 32;
    s->paper = s->paperBg = true; s->fitWidth = s->fitted = s->space = s->panning = false; s->anim = nullptr; s->draw = nullptr;
    gtk_widget_set_focusable(GTK_WIDGET(s), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(s), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(s), TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(s), "wenuy-canvas");

    // pekarens position (för zoom vid pekaren)
    auto* motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", CB(+[](GtkEventControllerMotion*, double x, double y, gpointer p) {
        WENUY_CANVAS(p)->px = x; WENUY_CANVAS(p)->py = y;
    }), s);
    gtk_widget_add_controller(GTK_WIDGET(s), motion);

    // scroll: panorera, Ctrl = zooma
    auto* scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll, "scroll", CB(+[](GtkEventControllerScroll* c, double dx, double dy, gpointer p) -> gboolean {
        auto* s = WENUY_CANVAS(p);
        const auto mods = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(c));
        const bool smooth = gtk_event_controller_scroll_get_unit(c) == GDK_SCROLL_UNIT_SURFACE;
        if (mods & GDK_CONTROL_MASK) {
            zoomAt(s, s->zoom * std::pow(1.0015, -dy * (smooth ? 1 : 60)), s->px, s->py, !smooth);
        } else {
            const double step = smooth ? 1 : 48;
            if (mods & GDK_SHIFT_MASK) std::swap(dx, dy);
            s->ox -= dx * step; s->oy -= dy * step;
            clampPan(s);
            gtk_widget_queue_draw(GTK_WIDGET(s));
        }
        return TRUE;
    }), s);
    gtk_widget_add_controller(GTK_WIDGET(s), scroll);

    // nyp (pekplatta/pekskärm)
    auto* pinch = gtk_gesture_zoom_new();
    g_signal_connect(pinch, "begin", CB(+[](GtkGesture*, GdkEventSequence*, gpointer p) {
        WENUY_CANVAS(p)->pinchZoom = WENUY_CANVAS(p)->zoom;
    }), s);
    g_signal_connect(pinch, "scale-changed", CB(+[](GtkGestureZoom* g, double scale, gpointer p) {
        auto* s = WENUY_CANVAS(p);
        double x, y;
        if (!gtk_gesture_get_bounding_box_center(GTK_GESTURE(g), &x, &y)) { x = s->px; y = s->py; }
        zoomAt(s, s->pinchZoom * scale, x, y, false);
    }), s);
    gtk_widget_add_controller(GTK_WIDGET(s), GTK_EVENT_CONTROLLER(pinch));

    // mittenknapp: panorera
    auto* mid = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(mid), GDK_BUTTON_MIDDLE);
    g_signal_connect(mid, "drag-begin", CB(+[](GtkGestureDrag*, double, double, gpointer p) {
        auto* s = WENUY_CANVAS(p); s->panX = s->ox; s->panY = s->oy;
        gtk_widget_set_cursor_from_name(GTK_WIDGET(s), "grabbing");
    }), s);
    g_signal_connect(mid, "drag-update", CB(+[](GtkGestureDrag*, double dx, double dy, gpointer p) {
        auto* s = WENUY_CANVAS(p); s->ox = s->panX + dx; s->oy = s->panY + dy;
        clampPan(s); gtk_widget_queue_draw(GTK_WIDGET(s));
    }), s);
    g_signal_connect(mid, "drag-end", CB(+[](GtkGestureDrag*, double, double, gpointer p) {
        gtk_widget_set_cursor(GTK_WIDGET(p), nullptr);
    }), s);
    gtk_widget_add_controller(GTK_WIDGET(s), GTK_EVENT_CONTROLLER(mid));

    // vänsterknapp/penna/finger: rita (eller panorera med mellanslag nedtryckt)
    auto* drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), 0);   // alla knappar utom de som tas ovan
    g_signal_connect(drag, "drag-begin", CB(+[](GtkGestureDrag* g, double x, double y, gpointer p) {
        auto* s = WENUY_CANVAS(p);
        gtk_widget_grab_focus(GTK_WIDGET(s));
        const guint b = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(g));
        if (b == GDK_BUTTON_MIDDLE) { gtk_gesture_set_state(GTK_GESTURE(g), GTK_EVENT_SEQUENCE_DENIED); return; }
        s->panning = s->space;
        if (s->panning) { s->panX = s->ox; s->panY = s->oy; gtk_widget_set_cursor_from_name(GTK_WIDGET(s), "grabbing"); return; }
        double cx, cy;
        wenuy_canvas_to_content(s, x, y, &cx, &cy);
        g_signal_emit(s, sigs[SIG_PRESS], 0, cx, cy, pressureOf(GTK_GESTURE(g)), b ? b : 1u);
    }), s);
    g_signal_connect(drag, "drag-update", CB(+[](GtkGestureDrag* g, double dx, double dy, gpointer p) {
        auto* s = WENUY_CANVAS(p);
        if (s->panning) { s->ox = s->panX + dx; s->oy = s->panY + dy; clampPan(s); gtk_widget_queue_draw(GTK_WIDGET(s)); return; }
        double sx, sy, cx, cy;
        gtk_gesture_drag_get_start_point(g, &sx, &sy);
        wenuy_canvas_to_content(s, sx + dx, sy + dy, &cx, &cy);
        g_signal_emit(s, sigs[SIG_DRAG], 0, cx, cy, pressureOf(GTK_GESTURE(g)));
    }), s);
    g_signal_connect(drag, "drag-end", CB(+[](GtkGestureDrag* g, double dx, double dy, gpointer p) {
        auto* s = WENUY_CANVAS(p);
        if (s->panning) { s->panning = false; gtk_widget_set_cursor(GTK_WIDGET(s), nullptr); return; }
        double sx, sy, cx, cy;
        gtk_gesture_drag_get_start_point(g, &sx, &sy);
        wenuy_canvas_to_content(s, sx + dx, sy + dy, &cx, &cy);
        g_signal_emit(s, sigs[SIG_RELEASE], 0, cx, cy);
    }), s);
    gtk_widget_add_controller(GTK_WIDGET(s), GTK_EVENT_CONTROLLER(drag));

    // tangenter: mellanslag (panorera), Ctrl +/−/0
    auto* keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", CB(+[](GtkEventControllerKey*, guint kv, guint, GdkModifierType m, gpointer p) -> gboolean {
        auto* s = WENUY_CANVAS(p);
        const double W = gtk_widget_get_width(GTK_WIDGET(s)) / 2.0, H = gtk_widget_get_height(GTK_WIDGET(s)) / 2.0;
        if (kv == GDK_KEY_space && !(m & GDK_CONTROL_MASK) && s->paper) {   // i ark/tabeller är mellanslag text
            if (!s->space) gtk_widget_set_cursor_from_name(GTK_WIDGET(s), "grab");
            s->space = true; return TRUE;
        }
        if (!(m & GDK_CONTROL_MASK)) return FALSE;
        if (kv == GDK_KEY_plus || kv == GDK_KEY_equal || kv == GDK_KEY_KP_Add) { zoomAt(s, s->zoom * 1.25, W, H, true); return TRUE; }
        if (kv == GDK_KEY_minus || kv == GDK_KEY_KP_Subtract) { zoomAt(s, s->zoom / 1.25, W, H, true); return TRUE; }
        if (kv == GDK_KEY_0 || kv == GDK_KEY_KP_0) { wenuy_canvas_zoom_to_fit(s, TRUE); return TRUE; }
        if (kv == GDK_KEY_1 || kv == GDK_KEY_KP_1) { zoomAt(s, 1.0, W, H, true); return TRUE; }
        return FALSE;
    }), s);
    g_signal_connect(keys, "key-released", CB(+[](GtkEventControllerKey*, guint kv, guint, GdkModifierType, gpointer p) {
        if (kv == GDK_KEY_space) { WENUY_CANVAS(p)->space = false; if (!WENUY_CANVAS(p)->panning) gtk_widget_set_cursor(GTK_WIDGET(p), nullptr); }
    }), s);
    gtk_widget_add_controller(GTK_WIDGET(s), keys);
}

static void wenuy_canvas_dispose(GObject* o) {
    auto* s = WENUY_CANVAS(o);
    g_clear_object(&s->anim);
    if (s->drawDestroy && s->drawData) s->drawDestroy(s->drawData);
    s->drawDestroy = nullptr; s->drawData = nullptr; s->draw = nullptr;
    G_OBJECT_CLASS(wenuy_canvas_parent_class)->dispose(o);
}

static void wenuy_canvas_class_init(WenuyCanvasClass* k) {
    G_OBJECT_CLASS(k)->dispose = wenuy_canvas_dispose;
    GTK_WIDGET_CLASS(k)->snapshot = snapshotCb;
    GTK_WIDGET_CLASS(k)->size_allocate = sizeAllocateCb;
    gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(k), "wenuycanvas");
    sigs[SIG_PRESS] = g_signal_new("press", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr,
                                   G_TYPE_NONE, 4, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_UINT);
    sigs[SIG_DRAG] = g_signal_new("drag", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr,
                                  G_TYPE_NONE, 3, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
    sigs[SIG_RELEASE] = g_signal_new("release", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr,
                                     G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
    sigs[SIG_ZOOM] = g_signal_new("zoom-changed", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr,
                                  G_TYPE_NONE, 0);
}

GtkWidget* wenuy_canvas_new(double w, double h) {
    auto* s = WENUY_CANVAS(g_object_new(WENUY_TYPE_CANVAS, nullptr));
    s->cw = std::max(1.0, w); s->ch = std::max(1.0, h);
    return GTK_WIDGET(s);
}

void wenuy_canvas_set_content_size(WenuyCanvas* s, double w, double h) {
    s->cw = std::max(1.0, w); s->ch = std::max(1.0, h);
    clampPan(s);
    gtk_widget_queue_draw(GTK_WIDGET(s));
}

void wenuy_canvas_set_draw_func(WenuyCanvas* s, WenuyCanvasDrawFunc fn, gpointer data, GDestroyNotify destroy) {
    if (s->drawDestroy && s->drawData) s->drawDestroy(s->drawData);
    s->draw = fn; s->drawData = data; s->drawDestroy = destroy;
    gtk_widget_queue_draw(GTK_WIDGET(s));
}

void wenuy_canvas_set_paper_background(WenuyCanvas* s, gboolean bg) {
    s->paperBg = bg;
    gtk_widget_queue_draw(GTK_WIDGET(s));
}

void wenuy_canvas_set_pages(WenuyCanvas* s, gboolean pages) {
    s->paper = true;
    s->paperBg = !pages;
    s->fitWidth = pages;
    s->fitted = false;
    gtk_widget_queue_draw(GTK_WIDGET(s));
}

void wenuy_canvas_set_paper(WenuyCanvas* s, gboolean paper) { s->paper = paper; clampPan(s); gtk_widget_queue_draw(GTK_WIDGET(s)); }
double wenuy_canvas_get_zoom(WenuyCanvas* s) { return s->zoom; }
void wenuy_canvas_set_zoom_limits(WenuyCanvas* s, double mn, double mx) { s->zmin = mn; s->zmax = mx; }

void wenuy_canvas_set_zoom(WenuyCanvas* s, double z, gboolean animate) {
    s->fitted = true;
    zoomAt(s, z, gtk_widget_get_width(GTK_WIDGET(s)) / 2.0, gtk_widget_get_height(GTK_WIDGET(s)) / 2.0, animate);
}

void wenuy_canvas_zoom_to_fit(WenuyCanvas* s, gboolean animate) {
    s->fitted = true;
    const double W = gtk_widget_get_width(GTK_WIDGET(s)), H = gtk_widget_get_height(GTK_WIDGET(s));
    animateTo(s, fitZoom(s), W / 2, H / 2, s->cw / 2, s->ch / 2, animate);   // innehållets mitt i ytans mitt
}

void wenuy_canvas_to_content(WenuyCanvas* s, double x, double y, double* cx, double* cy) {
    if (cx) *cx = (x - s->ox) / s->zoom;
    if (cy) *cy = (y - s->oy) / s->zoom;
}

void wenuy_canvas_queue_redraw(WenuyCanvas* s) { gtk_widget_queue_draw(GTK_WIDGET(s)); }
