// libwenuy – WenuyBackdrop: skrivbordsbakgrunden som delad byggsten (se WenuyBackdrop.h).
#include "WenuyBackdrop.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gstdio.h>

#include <algorithm>
#include <string>
#include <vector>

#include "Wenuy.h"

namespace {

const char* kDefault = "/usr/share/backgrounds/wenuyos/default.png";
std::string cfg(const char* f) { return std::string(g_get_user_config_dir()) + "/wenuyos/" + f; }

// Delat för alla WenuyBackdrop i programmet: användarens val, inlästa bilder och bevakningen
struct Shared {
    std::string current;                         // bildsökväg eller "#rrggbb"
    std::vector<std::pair<std::string, GdkTexture*>> cache;   // högst 2 bilder (val + förhandsvisning)
    std::vector<GtkWidget*> live;
    GFileMonitor* mon = nullptr;
    guint debounce = 0;
    // bildspel (wallpaper-slideshow = mapp, wallpaper-slideshow-min = minuter)
    std::vector<std::string> slides;
    size_t slide = 0;
    guint slideTimer = 0;
} S;

std::vector<std::string> imagesIn(const std::string& dir) {
    std::vector<std::string> out;
    GDir* d = g_dir_open(dir.c_str(), 0, nullptr);
    if (!d) return out;
    while (const char* n = g_dir_read_name(d)) {
        std::string low = n;
        for (auto& c : low) c = g_ascii_tolower(c);
        for (const char* ext : {".png", ".jpg", ".jpeg", ".webp"})
            if (g_str_has_suffix(low.c_str(), ext)) { out.push_back(dir + "/" + n); break; }
    }
    g_dir_close(d);
    std::sort(out.begin(), out.end());
    return out;
}

void redrawAll();
void setupSlideshow() {
    if (S.slideTimer) { g_source_remove(S.slideTimer); S.slideTimer = 0; }
    S.slides.clear();
    const std::string dir = wenuy::readLine(cfg("wallpaper-slideshow"));
    if (dir.empty()) return;
    S.slides = imagesIn(dir);
    if (S.slides.empty()) return;
    const int min = std::max(1, atoi(wenuy::readLine(cfg("wallpaper-slideshow-min")).c_str()) ?: 10);
    S.slide = (size_t)(g_get_real_time() / G_USEC_PER_SEC / 60 / min) % S.slides.size();   // samma bild i alla program
    S.current = S.slides[S.slide];
    S.slideTimer = g_timeout_add_seconds(min * 60, +[](gpointer) -> gboolean {
        if (S.slides.empty()) return G_SOURCE_REMOVE;
        S.slide = (S.slide + 1) % S.slides.size();
        S.current = S.slides[S.slide];
        // gamla bilder ur cachen: högst den nya och den förra
        redrawAll();
        return G_SOURCE_CONTINUE;
    }, nullptr);
}

std::string readCurrent() {
    const std::string link = cfg("wallpaper");
    const bool hasImage = g_file_test(link.c_str(), G_FILE_TEST_EXISTS);
    const std::string color = wenuy::readLine(cfg("wallpaper-color"));
    if (!hasImage && !color.empty() && color[0] == '#') return color;
    if (hasImage) {
        if (gchar* t = g_file_read_link(link.c_str(), nullptr)) { std::string r = t; g_free(t); return r; }
        return link;
    }
    return kDefault;
}

// Skärmens storlek i bildpunkter (största skärmen) – bilden läses aldrig större än så
void screenSize(int& w, int& h) {
    w = 1920; h = 1080;
    GdkDisplay* d = gdk_display_get_default();
    if (!d) return;
    GListModel* mons = gdk_display_get_monitors(d);
    int best = 0;
    for (guint i = 0; i < g_list_model_get_n_items(mons); ++i) {
        auto* m = GDK_MONITOR(g_list_model_get_item(mons, i));
        GdkRectangle g;
        gdk_monitor_get_geometry(m, &g);
        const int s = gdk_monitor_get_scale_factor(m);
        if (g.width * g.height * s * s > best) { best = g.width * g.height * s * s; w = g.width * s; h = g.height * s; }
        g_object_unref(m);
    }
}

GdkTexture* load(const std::string& path) {
    for (auto& [p, t] : S.cache) if (p == path) return t;
    int iw = 0, ih = 0, sw, sh;
    screenSize(sw, sh);
    if (!gdk_pixbuf_get_file_info(path.c_str(), &iw, &ih) || iw <= 0 || ih <= 0) return nullptr;
    // "täck hela skärmen": skala så att båda sidor räcker, aldrig större än originalet
    const double s = std::min(1.0, std::max(double(sw) / iw, double(sh) / ih));
    GError* err = nullptr;
    GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(path.c_str(), std::max(1, int(iw * s + 0.5)), std::max(1, int(ih * s + 0.5)), TRUE, &err);
    if (!pb) {
        g_warning("wenuy-backdrop: %s", err ? err->message : path.c_str());
        g_clear_error(&err);
        return nullptr;
    }
    GdkTexture* t = gdk_texture_new_for_pixbuf(pb);
    g_object_unref(pb);
    S.cache.insert(S.cache.begin(), {path, t});
    while (S.cache.size() > 2) { g_object_unref(S.cache.back().second); S.cache.pop_back(); }
    return t;
}

void redrawAll() { for (GtkWidget* w : S.live) gtk_widget_queue_draw(w); }

void ensureMonitor() {
    if (S.mon) return;
    S.current = readCurrent();
    setupSlideshow();
    GFile* dir = g_file_new_for_path((std::string(g_get_user_config_dir()) + "/wenuyos").c_str());
    S.mon = g_file_monitor_directory(dir, G_FILE_MONITOR_NONE, nullptr, nullptr);
    g_object_unref(dir);
    if (!S.mon) return;
    // temat byter länken/färgfilen → läs om (lite fördröjt, flera filer ändras på en gång)
    g_signal_connect(S.mon, "changed", G_CALLBACK(+[](GFileMonitor*, GFile* f, GFile*, GFileMonitorEvent, gpointer) {
        gchar* n = g_file_get_basename(f);
        const bool ours = n && g_str_has_prefix(n, "wallpaper");   // även wallpaper-<skärm> och bildspelet
        g_free(n);
        if (!ours) return;
        if (S.debounce) g_source_remove(S.debounce);
        S.debounce = g_timeout_add(200, +[](gpointer) -> gboolean {
            S.debounce = 0;
            S.current = readCurrent();
            setupSlideshow();
            redrawAll();
            return G_SOURCE_REMOVE;
        }, nullptr);
    }), nullptr);
}

bool parseColor(const std::string& s, GdkRGBA* c) { return !s.empty() && s[0] == '#' && gdk_rgba_parse(c, s.c_str()); }

}  // namespace

struct _WenuyBackdrop {
    GtkWidget parent;
    double dim;
    char* preview;
    char* monitor;   // skärmens namn – egen bakgrund per skärm
};
G_DEFINE_FINAL_TYPE(WenuyBackdrop, wenuy_backdrop, GTK_TYPE_WIDGET)

static void wenuy_backdrop_snapshot(GtkWidget* w, GtkSnapshot* snap) {
    auto* self = WENUY_BACKDROP(w);
    const float W = gtk_widget_get_width(w), H = gtk_widget_get_height(w);
    if (W <= 0 || H <= 0) return;
    graphene_rect_t all = GRAPHENE_RECT_INIT(0, 0, W, H);
    std::string what = self->preview ? self->preview : S.current;
    if (!self->preview && self->monitor) {   // egen bakgrund för den här skärmen?
        const std::string own = cfg((std::string("wallpaper-") + self->monitor).c_str());
        std::string v = wenuy::readLine(own);
        if (!v.empty() && (v[0] == '#' || g_file_test(v.c_str(), G_FILE_TEST_EXISTS))) what = v;
    }
    GdkRGBA color;
    if (parseColor(what, &color)) {
        gtk_snapshot_append_color(snap, &color, &all);
    } else {
        GdkTexture* t = load(what);
        if (!t && what != kDefault) t = load(kDefault);
        GdkRGBA fallback = {0.23f, 0.37f, 0.53f, 1.0f};   // #3B5F86 om ingen bild går att läsa
        gtk_snapshot_append_color(snap, &fallback, &all);
        if (t) {
            const float tw = gdk_texture_get_width(t), th = gdk_texture_get_height(t);
            const float s = std::max(W / tw, H / th);       // täck hela ytan, beskär kanterna
            const float dw = tw * s, dh = th * s;
            graphene_rect_t r = GRAPHENE_RECT_INIT((W - dw) / 2, (H - dh) / 2, dw, dh);
            gtk_snapshot_push_clip(snap, &all);
            gtk_snapshot_append_scaled_texture(snap, t, GSK_SCALING_FILTER_LINEAR, &r);
            gtk_snapshot_pop(snap);
        }
    }
    if (self->dim > 0) {   // mörkning i samma lager – inget genomskinligt fönster ovanpå
        GdkRGBA black = {0, 0, 0, float(self->dim)};
        gtk_snapshot_append_color(snap, &black, &all);
    }
}

static void wenuy_backdrop_dispose(GObject* o) {
    S.live.erase(std::remove(S.live.begin(), S.live.end(), GTK_WIDGET(o)), S.live.end());
    G_OBJECT_CLASS(wenuy_backdrop_parent_class)->dispose(o);
}
static void wenuy_backdrop_finalize(GObject* o) {
    g_free(WENUY_BACKDROP(o)->preview);
    g_free(WENUY_BACKDROP(o)->monitor);
    G_OBJECT_CLASS(wenuy_backdrop_parent_class)->finalize(o);
}
static void wenuy_backdrop_class_init(WenuyBackdropClass* k) {
    G_OBJECT_CLASS(k)->dispose = wenuy_backdrop_dispose;
    G_OBJECT_CLASS(k)->finalize = wenuy_backdrop_finalize;
    GTK_WIDGET_CLASS(k)->snapshot = wenuy_backdrop_snapshot;
    gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(k), "wenuybackdrop");
}
static void wenuy_backdrop_init(WenuyBackdrop* self) {
    self->dim = 0;
    self->preview = nullptr;
    self->monitor = nullptr;
    gtk_widget_set_hexpand(GTK_WIDGET(self), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(self), TRUE);
    ensureMonitor();
    S.live.push_back(GTK_WIDGET(self));
}

GtkWidget* wenuy_backdrop_new(void) { return GTK_WIDGET(g_object_new(WENUY_TYPE_BACKDROP, nullptr)); }

void wenuy_backdrop_set_dim(WenuyBackdrop* self, double amount) {
    g_return_if_fail(WENUY_IS_BACKDROP(self));
    self->dim = std::clamp(amount, 0.0, 0.8);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void wenuy_backdrop_preview(WenuyBackdrop* self, const char* what) {
    g_return_if_fail(WENUY_IS_BACKDROP(self));
    g_free(self->preview);
    self->preview = what && *what ? g_strdup(what) : nullptr;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

char* wenuy_backdrop_get_current(void) {
    ensureMonitor();
    return g_strdup(S.current.c_str());
}

void wenuy_backdrop_set_current(const char* what) {
    if (!what || !*what) return;
    S.current = what;   // visas direkt i det här programmet; temat sprider det till resten
    redrawAll();
    wenuy::spawn({"/usr/lib/wenuyos/wenuyos-theme", "set", "desktop.wallpaper", what});
}

void wenuy_backdrop_set_monitor(WenuyBackdrop* self, const char* connector) {
    g_return_if_fail(WENUY_IS_BACKDROP(self));
    g_free(self->monitor);
    self->monitor = connector && *connector ? g_strdup(connector) : nullptr;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void wenuy_backdrop_set_for_monitor(const char* connector, const char* what) {
    if (!connector || !*connector) return;
    const std::string f = cfg((std::string("wallpaper-") + connector).c_str());
    g_mkdir_with_parents((std::string(g_get_user_config_dir()) + "/wenuyos").c_str(), 0755);
    if (what && *what) g_file_set_contents(f.c_str(), what, -1, nullptr);
    else g_unlink(f.c_str());
    redrawAll();
}

void wenuy_backdrop_set_slideshow(const char* folder, int minutes) {
    g_mkdir_with_parents((std::string(g_get_user_config_dir()) + "/wenuyos").c_str(), 0755);
    if (folder && *folder) {
        g_file_set_contents(cfg("wallpaper-slideshow").c_str(), folder, -1, nullptr);
        g_file_set_contents(cfg("wallpaper-slideshow-min").c_str(), std::to_string(std::max(1, minutes)).c_str(), -1, nullptr);
    } else {
        g_unlink(cfg("wallpaper-slideshow").c_str());
    }
    ensureMonitor();
    setupSlideshow();
    if (!(folder && *folder)) S.current = readCurrent();
    redrawAll();
}
