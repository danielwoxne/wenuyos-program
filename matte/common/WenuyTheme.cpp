// libwenuy – det centrala temat. Läser ~/.config/wenuyos/theme-resolved.ini (skrivs av wenuyos theme).
#include "WenuyTheme.h"

#include <adwaita.h>

#include <string>

struct _WenuyTheme {
    GObject parent;
    GKeyFile* kf;
    GFileMonitor* monitor;
    guint reloadId;
    std::string* motion;
};
G_DEFINE_FINAL_TYPE(WenuyTheme, wenuy_theme, G_TYPE_OBJECT)

enum { SIG_CHANGED, N_SIGS };
static guint themeSignals[N_SIGS];

static std::string resolvedPath() { return std::string(g_get_user_config_dir()) + "/wenuyos/theme-resolved.ini"; }

static void load(WenuyTheme* t) {
    g_key_file_free(t->kf);
    t->kf = g_key_file_new();
    g_key_file_load_from_file(t->kf, resolvedPath().c_str(), G_KEY_FILE_NONE, nullptr);
    gchar* m = g_key_file_get_string(t->kf, "Motion", "level", nullptr);
    *t->motion = m && *m ? m : "full";
    g_free(m);
}

static void wenuy_theme_class_init(WenuyThemeClass* c) {
    themeSignals[SIG_CHANGED] = g_signal_new("changed", G_TYPE_FROM_CLASS(c), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr,
                                             nullptr, G_TYPE_NONE, 0);
}

static void wenuy_theme_init(WenuyTheme* t) {
    t->kf = g_key_file_new();
    t->motion = new std::string("full");
}

WenuyTheme* wenuy_theme_get_default(void) {
    static WenuyTheme* theme = nullptr;
    if (theme) return theme;
    theme = WENUY_THEME(g_object_new(WENUY_TYPE_THEME, nullptr));
    load(theme);
    GFile* f = g_file_new_for_path(resolvedPath().c_str());
    theme->monitor = g_file_monitor_file(f, G_FILE_MONITOR_WATCH_MOVES, nullptr, nullptr);
    g_object_unref(f);
    if (theme->monitor)
        g_signal_connect(theme->monitor, "changed", G_CALLBACK(+[](GFileMonitor*, GFile*, GFile*, GFileMonitorEvent ev, gpointer p) {
            auto* t = WENUY_THEME(p);
            if (ev == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED || t->reloadId) return;
            t->reloadId = g_timeout_add(150, +[](gpointer pp) -> gboolean {
                auto* tt = WENUY_THEME(pp);
                tt->reloadId = 0;
                load(tt);
                g_signal_emit(tt, themeSignals[SIG_CHANGED], 0);
                return G_SOURCE_REMOVE;
            }, t);
        }), theme);
    return theme;
}

char* wenuy_theme_get(WenuyTheme* t, const char* section, const char* key) {
    return g_key_file_get_string(t->kf, section, key, nullptr);
}

char* wenuy_theme_get_name(WenuyTheme* t) {
    gchar* n = g_key_file_get_string(t->kf, "Theme", "name", nullptr);
    return n ? n : g_strdup("wenuyos");
}

void wenuy_theme_get_accent(WenuyTheme* t, GdkRGBA* rgba) {
    gchar* a = g_key_file_get_string(t->kf, "Theme", "accent", nullptr);
    if (!a || !gdk_rgba_parse(rgba, a)) gdk_rgba_parse(rgba, "#3B82F6");
    g_free(a);
}

int wenuy_theme_get_radius(WenuyTheme* t) {
    GError* e = nullptr;
    const int r = g_key_file_get_integer(t->kf, "Shape", "radius", &e);
    if (e) { g_error_free(e); return 12; }
    return CLAMP(r, 0, 32);
}

const char* wenuy_theme_get_motion(WenuyTheme* t) { return t->motion->c_str(); }

gboolean wenuy_theme_is_high_contrast(WenuyTheme* t) {
    return g_key_file_get_boolean(t->kf, "Accessibility", "high_contrast", nullptr);
}

/* ---- stil: accent, hörn och animationer i det egna programmet, uppdateras live */
static void refreshStyle(GtkCssProvider* p) {
    WenuyTheme* t = wenuy_theme_get_default();
    GdkRGBA a;
    wenuy_theme_get_accent(t, &a);
    gchar* hex = g_strdup_printf("#%02X%02X%02X", int(a.red * 255), int(a.green * 255), int(a.blue * 255));
    const int r = wenuy_theme_get_radius(t);
    gchar* css = g_strdup_printf(
        "@define-color accent_bg_color %s;\n@define-color accent_color %s;\n"
        ".card, .wenuy-card { border-radius: %dpx; }\nbutton, entry { border-radius: %dpx; }\n",
        hex, hex, r, MAX(r - 4, 0));
    gtk_css_provider_load_from_string(p, css);
    g_free(css);
    g_free(hex);
    if (GtkSettings* s = gtk_settings_get_default())
        // Animationer av om användaren valt det, eller om datorn saknar grafikdrivrutin (WENUYOS_GFX=soft
        // från wenuyos-session) och inget eget val finns – då kostar varje bildruta processortid.
        g_object_set(s, "gtk-enable-animations",
                     g_strcmp0(wenuy_theme_get_motion(t), "off") != 0 &&
                         !(g_strcmp0(g_getenv("WENUYOS_GFX"), "soft") == 0 && !g_getenv("WENUY_MOTION_FORCE")),
                     nullptr);
}

void wenuy_theme_apply_style(void) {
    static GtkCssProvider* provider = nullptr;
    if (provider || !gdk_display_get_default()) return;
    provider = gtk_css_provider_new();
    // under programmets egen stil (APPLICATION) men över temat
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION - 1);
    refreshStyle(provider);
    g_signal_connect_swapped(wenuy_theme_get_default(), "changed", G_CALLBACK(refreshStyle), provider);
}

/* ---- ljud: freedesktop-ljudtema, valt i wenuyos theme */
void wenuy_play_sound(const char* event) {
    if (!event || !*event) return;
    // äldre korta namn → freedesktop-namn
    static const char* alias[][2] = {{"done", "complete"}, {"step", "window-new"}, {"toggle", "button-toggle-on"},
                                     {"welcome", "desktop-login"}};
    for (auto& a : alias)
        if (g_str_equal(event, a[0])) event = a[1];
    const char* argv[] = {"/usr/lib/wenuyos/wenuyos-sound", event, nullptr};
    g_spawn_async(nullptr, (gchar**)argv, nullptr, G_SPAWN_STDOUT_TO_DEV_NULL, nullptr, nullptr, nullptr, nullptr);
}
