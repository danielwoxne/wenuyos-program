#include "WenuyTimeline.h"

#include <adwaita.h>

#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>

#include "Wenuy.h"

namespace wenuy {

int Timeline::addClip(Clip c) {
    c.id = nextId_++;
    clips.push_back(c);
    return c.id;
}

Clip* Timeline::find(int id) {
    for (auto& c : clips) if (c.id == id) return &c;
    return nullptr;
}

double Timeline::duration() const {
    double d = 0;
    for (auto& c : clips) d = std::max(d, c.start + c.length);
    return d;
}

std::string Timeline::serialize() const {
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << "wtimeline 1\n";
    for (auto& t : tracks) o << "track\t" << t.height << "\t" << (t.muted ? 1 : 0) << "\t" << t.name << "\n";
    for (auto& c : clips)
        o << "clip\t" << c.track << "\t" << c.start << "\t" << c.length << "\t" << c.offset << "\t"
          << c.color.red << "\t" << c.color.green << "\t" << c.color.blue << "\t" << c.label << "\t" << c.source << "\n";
    return o.str();
}

bool Timeline::deserialize(const std::string& data) {
    std::istringstream in(data);
    std::string line;
    if (!std::getline(in, line) || line.rfind("wtimeline", 0) != 0) return false;
    tracks.clear();
    clips.clear();
    nextId_ = 1;
    while (std::getline(in, line)) {
        std::vector<std::string> f;
        std::stringstream ss(line);
        std::string x;
        while (std::getline(ss, x, '\t')) f.push_back(x);
        auto num = [](const std::string& s) { std::istringstream i(s); i.imbue(std::locale::classic()); double d = 0; i >> d; return d; };
        if (f.size() >= 4 && f[0] == "track") tracks.push_back({f[3], num(f[1]), f[2] == "1"});
        else if (f.size() >= 9 && f[0] == "clip") {
            Clip c;
            c.track = (int)num(f[1]); c.start = num(f[2]); c.length = num(f[3]); c.offset = num(f[4]);
            c.color = {(float)num(f[5]), (float)num(f[6]), (float)num(f[7]), 1};
            c.label = f[8];
            if (f.size() > 9) c.source = f[9];
            addClip(c);
        }
    }
    return true;
}

}  // namespace wenuy

// ================================================================ widget
namespace {

constexpr double kHead = 132, kRuler = 30;

struct TL {
    wenuy::Timeline* m = nullptr;
    wenuy::History* history = nullptr;
    WenuyClipDrawFunc clipDraw = nullptr;
    gpointer clipData = nullptr;
    double pps = 80;          // pixlar per sekund
    double scrollX = 0;       // sekunder i vänsterkanten
    double scrollY = 0;       // pixlar
    double px = 0, py = 0;    // pekare
    // drag
    enum { None, Move, Trim, Playhead } mode = None;
    int dragId = -1;
    wenuy::Clip orig;
    double grabOffset = 0;
};

guint sigPlayhead, sigSelection, sigClips;

TL* tl(GtkWidget* w) { return static_cast<TL*>(g_object_get_data(G_OBJECT(w), "wenuy-tl")); }

double xOf(TL* t, double sec) { return kHead + (sec - t->scrollX) * t->pps; }
double secOf(TL* t, double x) { return t->scrollX + (x - kHead) / t->pps; }

double trackY(TL* t, int i) {
    double y = kRuler - t->scrollY;
    for (int k = 0; k < i && k < (int)t->m->tracks.size(); ++k) y += t->m->tracks[k].height;
    return y;
}

int trackAt(TL* t, double y) {
    double ty = kRuler - t->scrollY;
    for (int k = 0; k < (int)t->m->tracks.size(); ++k) {
        if (y >= ty && y < ty + t->m->tracks[k].height) return k;
        ty += t->m->tracks[k].height;
    }
    return y < kRuler ? 0 : (int)t->m->tracks.size() - 1;
}

std::string fmtTime(double s, bool fine) {
    const int m = (int)(s / 60);
    const double r = s - m * 60;
    char b[32];
    if (fine) std::snprintf(b, sizeof b, "%d:%04.1f", m, r);
    else std::snprintf(b, sizeof b, "%d:%02d", m, (int)std::round(r));
    return b;
}

void rect(GtkSnapshot* s, double x, double y, double w, double h, const GdkRGBA& c) {
    graphene_rect_t r = GRAPHENE_RECT_INIT((float)x, (float)y, (float)w, (float)h);
    gtk_snapshot_append_color(s, &c, &r);
}

void label(GtkWidget* w, GtkSnapshot* s, const std::string& text, double x, double y, const GdkRGBA& c, bool small = false) {
    auto* l = gtk_widget_create_pango_layout(w, text.c_str());
    if (small) {
        auto* a = pango_attr_list_new();
        pango_attr_list_insert(a, pango_attr_scale_new(0.85));
        pango_layout_set_attributes(l, a);
        pango_attr_list_unref(a);
    }
    gtk_snapshot_save(s);
    graphene_point_t p = GRAPHENE_POINT_INIT((float)x, (float)y);
    gtk_snapshot_translate(s, &p);
    gtk_snapshot_append_layout(s, l, &c);
    gtk_snapshot_restore(s);
    g_object_unref(l);
}

void snapshotTL(GtkWidget* w, GtkSnapshot* s) {
    TL* t = tl(w);
    const double W = gtk_widget_get_width(w), H = gtk_widget_get_height(w);
    const bool dark = adw_style_manager_get_dark(adw_style_manager_get_default());
    const GdkRGBA bg = dark ? GdkRGBA{0.11f, 0.12f, 0.14f, 1} : GdkRGBA{0.97f, 0.975f, 0.98f, 1};
    const GdkRGBA lane2 = dark ? GdkRGBA{1, 1, 1, 0.025f} : GdkRGBA{0, 0, 0, 0.025f};
    const GdkRGBA line = dark ? GdkRGBA{1, 1, 1, 0.08f} : GdkRGBA{0, 0, 0, 0.08f};
    const GdkRGBA fg = dark ? GdkRGBA{0.9f, 0.91f, 0.93f, 1} : GdkRGBA{0.12f, 0.13f, 0.16f, 1};
    const GdkRGBA dim = dark ? GdkRGBA{0.65f, 0.67f, 0.72f, 1} : GdkRGBA{0.42f, 0.44f, 0.48f, 1};
    const GdkRGBA head = dark ? GdkRGBA{0.15f, 0.16f, 0.19f, 1} : GdkRGBA{1, 1, 1, 1};
    const GdkRGBA accent = {0.24f, 0.47f, 0.96f, 1};
    const GdkRGBA white = {1, 1, 1, 1};

    graphene_rect_t all = GRAPHENE_RECT_INIT(0, 0, (float)W, (float)H);
    gtk_snapshot_push_clip(s, &all);
    rect(s, 0, 0, W, H, bg);

    // spår
    for (int i = 0; i < (int)t->m->tracks.size(); ++i) {
        const double y = trackY(t, i), h = t->m->tracks[i].height;
        if (y > H || y + h < kRuler) continue;
        if (i % 2) rect(s, kHead, y, W - kHead, h, lane2);
        rect(s, 0, y + h - 1, W, 1, line);
    }
    // tidsrutnät: välj ett steg som ger ~90 px mellan etiketter
    const double steps[] = {0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600};
    double step = steps[0];
    for (double st : steps) { step = st; if (st * t->pps >= 90) break; }
    const double t0 = std::floor(t->scrollX / step) * step;
    for (double sec = t0; xOf(t, sec) < W; sec += step) {
        const double x = xOf(t, sec);
        if (x < kHead) continue;
        rect(s, x, kRuler, 1, H - kRuler, line);
    }

    // klipp
    graphene_rect_t lanes = GRAPHENE_RECT_INIT((float)kHead, (float)kRuler, (float)(W - kHead), (float)(H - kRuler));
    gtk_snapshot_push_clip(s, &lanes);
    for (auto& c : t->m->clips) {
        if (c.track < 0 || c.track >= (int)t->m->tracks.size()) continue;
        const double x = xOf(t, c.start), cw = std::max(2.0, c.length * t->pps);
        const double y = trackY(t, c.track) + 4, h = t->m->tracks[c.track].height - 8;
        if (x > W || x + cw < kHead) continue;
        graphene_rect_t r = GRAPHENE_RECT_INIT((float)x, (float)y, (float)cw, (float)h);
        GskRoundedRect rr;
        gsk_rounded_rect_init_from_rect(&rr, &r, 7);
        GdkRGBA fill = c.color;
        fill.alpha = t->m->tracks[c.track].muted ? 0.35f : 0.9f;
        gtk_snapshot_push_rounded_clip(s, &rr);
        rect(s, x, y, cw, h, fill);
        if (t->clipDraw) t->clipDraw(s, &c, (float)x, (float)y + 18, (float)cw, (float)h - 18, 1.0 / t->pps, t->clipData);
        if (cw > 30) label(w, s, c.label, x + 8, y + 3, white, true);
        gtk_snapshot_pop(s);
        if (c.id == t->m->selected) {   // markerat: tydlig ring utanför klippet
            graphene_rect_t o = GRAPHENE_RECT_INIT((float)x - 3, (float)y - 3, (float)cw + 6, (float)h + 6);
            GskRoundedRect orr;
            gsk_rounded_rect_init_from_rect(&orr, &o, 9);
            const GdkRGBA ringC = dark ? white : fg;
            GdkRGBA ring[4] = {ringC, ringC, ringC, ringC};
            float wd[4] = {2.5f, 2.5f, 2.5f, 2.5f};
            gtk_snapshot_append_border(s, &orr, wd, ring);
        }
    }
    gtk_snapshot_pop(s);

    // linjal
    rect(s, 0, 0, W, kRuler, head);
    rect(s, 0, kRuler - 1, W, 1, line);
    for (double sec = t0; xOf(t, sec) < W; sec += step) {
        const double x = xOf(t, sec);
        if (x < kHead - 1) continue;
        rect(s, x, kRuler - 8, 1, 7, dim);
        label(w, s, fmtTime(sec, step < 1), x + 4, 6, dim, true);
    }
    // spårnamn
    for (int i = 0; i < (int)t->m->tracks.size(); ++i) {
        const double y = trackY(t, i), h = t->m->tracks[i].height;
        if (y + h < kRuler || y > H) continue;
        rect(s, 0, std::max(y, kRuler), kHead, h, head);
        if (y + 8 > kRuler - 4) label(w, s, t->m->tracks[i].name, 12, y + (h - 18) / 2, t->m->tracks[i].muted ? dim : fg);
        rect(s, 0, y + h - 1, kHead, 1, line);
    }
    rect(s, 0, 0, kHead, kRuler, head);
    label(w, s, fmtTime(t->m->playhead, true), 12, 6, accent, true);
    rect(s, kHead - 1, 0, 1, H, line);

    // uppspelningsmarkör
    const double px = xOf(t, t->m->playhead);
    if (px >= kHead - 1 && px <= W) {
        rect(s, px - 1, 0, 2, H, accent);
        auto* b = gsk_path_builder_new();
        gsk_path_builder_move_to(b, (float)px - 7, 0);
        gsk_path_builder_line_to(b, (float)px + 7, 0);
        gsk_path_builder_line_to(b, (float)px, 10);
        gsk_path_builder_close(b);
        auto* path = gsk_path_builder_free_to_path(b);
        gtk_snapshot_append_fill(s, path, GSK_FILL_RULE_WINDING, &accent);
        gsk_path_unref(path);
    }
    gtk_snapshot_pop(s);
}

// Fäst mot närmaste kant (andra klipp, markören, början) inom 8 px
double snap(TL* t, double sec, int ignoreId) {
    double best = sec, bestD = 8.0 / t->pps;
    auto consider = [&](double cand) { if (std::fabs(cand - sec) < bestD) { bestD = std::fabs(cand - sec); best = cand; } };
    consider(0);
    consider(t->m->playhead);
    for (auto& c : t->m->clips) if (c.id != ignoreId) { consider(c.start); consider(c.start + c.length); }
    return best;
}

void commitClipChange(GtkWidget* w, const wenuy::Clip& before, const wenuy::Clip& after) {
    TL* t = tl(w);
    if (!t->history) return;
    auto* m = t->m;
    auto apply = [w, m](const wenuy::Clip& c) {
        if (auto* p = m->find(c.id)) *p = c;
        g_signal_emit(w, sigClips, 0);
        gtk_widget_queue_draw(w);
    };
    t->history->record(wenuy::tr("Move clip", "Flytta klipp"), [apply, after]() { apply(after); }, [apply, before]() { apply(before); });
}

}  // namespace

GtkWidget* wenuy_timeline_new(wenuy::Timeline* model) {
    static bool registered = false;
    // En vanlig GtkWidget-underklass med egen ritning: GtkDrawingArea räcker inte för GskPath-
    // noder, så vi använder en "snapshot"-funktion via en minimal underklass.
    static GType type = 0;
    if (!registered) {
        registered = true;
        GTypeInfo info = {};
        info.class_size = sizeof(GtkWidgetClass);
        info.instance_size = sizeof(GtkWidget);
        info.class_init = +[](gpointer k, gpointer) {
            GTK_WIDGET_CLASS(k)->snapshot = snapshotTL;
            gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(k), "wenuytimeline");
            sigPlayhead = g_signal_new("playhead-moved", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 1, G_TYPE_DOUBLE);
            sigSelection = g_signal_new("selection-changed", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 1, G_TYPE_INT);
            sigClips = g_signal_new("clips-changed", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
        };
        type = g_type_register_static(GTK_TYPE_WIDGET, "WenuyTimelineWidget", &info, GTypeFlags(0));
    }
    auto* w = GTK_WIDGET(g_object_new(type, nullptr));
    auto* t = new TL;
    t->m = model;
    g_object_set_data_full(G_OBJECT(w), "wenuy-tl", t, +[](gpointer p) { delete static_cast<TL*>(p); });
    gtk_widget_set_hexpand(w, TRUE);
    gtk_widget_set_vexpand(w, TRUE);
    gtk_widget_set_focusable(w, TRUE);
    gtk_widget_set_size_request(w, 300, 140);

    auto* motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", CB(+[](GtkEventControllerMotion*, double x, double y, gpointer p) {
        auto* w = GTK_WIDGET(p);
        TL* t = tl(w);
        t->px = x; t->py = y;
        // markören visar vad man kan göra
        const char* cur = nullptr;
        if (y > kRuler && x > kHead) {
            const double sec = secOf(t, x);
            const int tr = trackAt(t, y);
            for (auto& c : t->m->clips)
                if (c.track == tr && sec >= c.start && sec <= c.start + c.length)
                    cur = std::fabs(xOf(t, c.start + c.length) - x) < 7 ? "col-resize" : "grab";
        }
        gtk_widget_set_cursor_from_name(w, cur);
    }), w);
    gtk_widget_add_controller(w, motion);

    auto* scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll, "scroll", CB(+[](GtkEventControllerScroll* c, double dx, double dy, gpointer p) -> gboolean {
        auto* w = GTK_WIDGET(p);
        TL* t = tl(w);
        const auto mods = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(c));
        const bool smooth = gtk_event_controller_scroll_get_unit(c) == GDK_SCROLL_UNIT_SURFACE;
        const double k = smooth ? 1 : 40;
        if (mods & GDK_CONTROL_MASK) {   // zooma kring pekaren
            const double at = secOf(t, t->px);
            t->pps = std::clamp(t->pps * std::pow(1.0015, -dy * (smooth ? 1 : 60)), 2.0, 2000.0);
            t->scrollX = std::max(0.0, at - (t->px - kHead) / t->pps);
        } else {
            if (mods & GDK_SHIFT_MASK) std::swap(dx, dy);
            double total = 0;
            for (auto& tr : t->m->tracks) total += tr.height;
            const double room = gtk_widget_get_height(w) - kRuler;
            if (total > room && dx == 0) t->scrollY = std::clamp(t->scrollY + dy * k, 0.0, total - room);
            else t->scrollX = std::max(0.0, t->scrollX + (dx + (total > room ? 0 : dy)) * k / t->pps);
        }
        gtk_widget_queue_draw(w);
        return TRUE;
    }), w);
    gtk_widget_add_controller(w, scroll);

    auto* drag = gtk_gesture_drag_new();
    g_signal_connect(drag, "drag-begin", CB(+[](GtkGestureDrag*, double x, double y, gpointer p) {
        auto* w = GTK_WIDGET(p);
        TL* t = tl(w);
        gtk_widget_grab_focus(w);
        t->mode = TL::None;
        if (y < kRuler || x < kHead) {   // linjalen: flytta markören
            t->mode = TL::Playhead;
            t->m->playhead = std::max(0.0, secOf(t, std::max(x, kHead)));
            g_signal_emit(w, sigPlayhead, 0, t->m->playhead);
            gtk_widget_queue_draw(w);
            return;
        }
        const double sec = secOf(t, x);
        const int tr = trackAt(t, y);
        int hit = -1;
        for (auto& c : t->m->clips)
            if (c.track == tr && sec >= c.start && sec <= c.start + c.length) hit = c.id;
        if (hit != t->m->selected) { t->m->selected = hit; g_signal_emit(w, sigSelection, 0, hit); }
        if (hit >= 0) {
            auto* c = t->m->find(hit);
            t->orig = *c;
            t->dragId = hit;
            t->mode = std::fabs(xOf(t, c->start + c->length) - x) < 7 ? TL::Trim : TL::Move;
            t->grabOffset = sec - c->start;
        }
        gtk_widget_queue_draw(w);
    }), w);
    g_signal_connect(drag, "drag-update", CB(+[](GtkGestureDrag* g, double dx, double dy, gpointer p) {
        auto* w = GTK_WIDGET(p);
        TL* t = tl(w);
        double sx, sy;
        gtk_gesture_drag_get_start_point(g, &sx, &sy);
        const double x = sx + dx, y = sy + dy;
        if (t->mode == TL::Playhead) {
            t->m->playhead = std::max(0.0, secOf(t, std::max(x, kHead)));
            g_signal_emit(w, sigPlayhead, 0, t->m->playhead);
        } else if (auto* c = t->m->find(t->dragId)) {
            if (t->mode == TL::Move) {
                double start = std::max(0.0, secOf(t, x) - t->grabOffset);
                const double s1 = snap(t, start, c->id), s2 = snap(t, start + c->length, c->id) - c->length;
                start = std::fabs(s1 - start) <= std::fabs(s2 - start) ? s1 : s2;
                c->start = std::max(0.0, start);
                c->track = std::clamp(trackAt(t, y), 0, (int)t->m->tracks.size() - 1);
            } else {
                c->length = std::max(0.05, snap(t, secOf(t, x), c->id) - c->start);
            }
        }
        gtk_widget_queue_draw(w);
    }), w);
    g_signal_connect(drag, "drag-end", CB(+[](GtkGestureDrag*, double, double, gpointer p) {
        auto* w = GTK_WIDGET(p);
        TL* t = tl(w);
        if ((t->mode == TL::Move || t->mode == TL::Trim) && t->m->find(t->dragId)) {
            const wenuy::Clip after = *t->m->find(t->dragId);
            if (after.start != t->orig.start || after.length != t->orig.length || after.track != t->orig.track) {
                commitClipChange(w, t->orig, after);
                g_signal_emit(w, sigClips, 0);
            }
        }
        t->mode = TL::None;
        t->dragId = -1;
    }), w);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(drag));

    auto* keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", CB(+[](GtkEventControllerKey*, guint kv, guint, GdkModifierType, gpointer p) -> gboolean {
        auto* w = GTK_WIDGET(p);
        TL* t = tl(w);
        if ((kv == GDK_KEY_Delete || kv == GDK_KEY_BackSpace) && t->m->find(t->m->selected)) {
            const wenuy::Clip gone = *t->m->find(t->m->selected);
            auto* m = t->m;
            auto remove = [w, m, id = gone.id]() {
                m->clips.erase(std::remove_if(m->clips.begin(), m->clips.end(), [id](const wenuy::Clip& c) { return c.id == id; }), m->clips.end());
                if (m->selected == id) m->selected = -1;
                g_signal_emit(w, sigClips, 0);
                gtk_widget_queue_draw(w);
            };
            auto restore = [w, m, gone]() { m->clips.push_back(gone); g_signal_emit(w, sigClips, 0); gtk_widget_queue_draw(w); };
            if (t->history) t->history->perform(wenuy::tr("Delete clip", "Ta bort klipp"), remove, restore);
            else remove();
            return TRUE;
        }
        if (kv == GDK_KEY_Home) { t->m->playhead = 0; t->scrollX = 0; g_signal_emit(w, sigPlayhead, 0, 0.0); gtk_widget_queue_draw(w); return TRUE; }
        return FALSE;
    }), w);
    gtk_widget_add_controller(w, keys);
    return w;
}

void wenuy_timeline_set_history(GtkWidget* w, wenuy::History* h) { tl(w)->history = h; }
void wenuy_timeline_set_clip_draw_func(GtkWidget* w, WenuyClipDrawFunc fn, gpointer data) { tl(w)->clipDraw = fn; tl(w)->clipData = data; gtk_widget_queue_draw(w); }
void wenuy_timeline_set_zoom(GtkWidget* w, double pps) { tl(w)->pps = std::clamp(pps, 2.0, 2000.0); gtk_widget_queue_draw(w); }
void wenuy_timeline_changed(GtkWidget* w) {
    // följ markören under uppspelning: rulla när den går utanför
    TL* t = tl(w);
    const double x = xOf(t, t->m->playhead), W = gtk_widget_get_width(w);
    if (W > kHead + 40 && (x > W - 24 || x < kHead)) t->scrollX = std::max(0.0, t->m->playhead - 1.0);
    gtk_widget_queue_draw(w);
}
