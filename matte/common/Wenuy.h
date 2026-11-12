// Gemensamt för WenuyOS GTK 4-verktyg: språk, processer, stil.
#pragma once
#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>

#include <libintl.h>

#include "WenuyDiag.h"
#include "WenuyTheme.h"

#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

// G_CALLBACK med lambda: variadiskt makro så att kommatecken inuti lambdan fungerar
#define CB(...) G_CALLBACK((__VA_ARGS__))

namespace wenuy {

inline bool swedish() {
    const char* l = std::getenv("LANG");
    return l && std::string(l).rfind("sv", 0) == 0;
}
// Översättningar utöver svenska: po/<språk>.po i projektet (wenuyos app translate <språk>).
// i18nInit("se.min.app") i main() – sedan översätter tr() till användarens språk om en .mo finns.
inline std::string& i18nDomain() { static std::string d; return d; }
inline void i18nInit(const char* appId) {
    wenuy_diag_init(appId);   // mätning och buggkontroll (WenuyDiag.h)
    i18nDomain() = appId ? appId : "";
    const char* dev = std::getenv("WENUY_LOCALEDIR");   // satt av `wenuyos run`
    std::string dir = dev && *dev ? dev : std::string(g_get_user_data_dir()) + "/locale";
    if (!dev && !g_file_test((dir + "/").c_str(), G_FILE_TEST_IS_DIR)) dir = "/usr/share/locale";
    bindtextdomain(appId, dir.c_str());
    bind_textdomain_codeset(appId, "UTF-8");
}
// Engelska är basspråk; svenska alltid med; övriga språk via gettext om i18nInit() anropats.
inline const char* tr(const char* en, const char* sv) {
    if (swedish()) return sv;
    return i18nDomain().empty() ? en : dgettext(i18nDomain().c_str(), en);
}

inline std::string readLine(const std::string& path) {
    std::ifstream f(path);
    std::string s;
    std::getline(f, s);
    return s;
}
inline bool exists(const std::string& p) { return g_file_test(p.c_str(), G_FILE_TEST_EXISTS); }
inline bool liveSession() {
    std::ifstream f("/proc/cmdline");
    std::string s((std::istreambuf_iterator<char>(f)), {});
    return s.find("boot=casper") != std::string::npos || s.find("boot=live") != std::string::npos;   // Ubuntu (casper) / Debian (live-boot)
}
inline bool lowProfile() { return readLine("/run/wenuyos/props/persist.wenuyos.perf") == "low"; }

inline std::vector<const char*> cargv(const std::vector<std::string>& v) {
    std::vector<const char*> a;
    for (auto& s : v) a.push_back(s.c_str());
    a.push_back(nullptr);
    return a;
}
// Kort synkront anrop – bara för snabba statusfrågor.
inline std::string run(const std::vector<std::string>& argv) {
    auto a = cargv(argv);
    gchar* out = nullptr;
    gint st = 0;
    g_spawn_sync(nullptr, const_cast<gchar**>(a.data()), nullptr,
                 (GSpawnFlags)(G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL), nullptr, nullptr, &out, nullptr, &st, nullptr);
    std::string r = out ? out : "";
    g_free(out);
    while (!r.empty() && (r.back() == '\n' || r.back() == ' ')) r.pop_back();
    return r;
}
// Starta och glöm.
inline void spawn(const std::vector<std::string>& argv) {
    auto a = cargv(argv);
    g_spawn_async(nullptr, const_cast<gchar**>(a.data()), nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, nullptr);
}
// Asynkront anrop – gränssnittet fryser aldrig. done(ok, stdout).
inline void runAsync(const std::vector<std::string>& argv, std::function<void(bool, std::string)> done) {
    auto a = cargv(argv);
    GError* err = nullptr;
    GSubprocess* p = g_subprocess_newv(a.data(), (GSubprocessFlags)(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE), &err);
    if (!p) { done(false, err ? err->message : ""); g_clear_error(&err); return; }
    auto* cb = new std::function<void(bool, std::string)>(std::move(done));
    g_subprocess_communicate_utf8_async(p, nullptr, nullptr, +[](GObject* src, GAsyncResult* res, gpointer data) {
        auto* f = static_cast<std::function<void(bool, std::string)>*>(data);
        gchar* out = nullptr;
        bool ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(src), res, &out, nullptr, nullptr) &&
                  g_subprocess_get_successful(G_SUBPROCESS(src));
        (*f)(ok, out ? out : "");
        g_free(out);
        delete f;
        g_object_unref(src);
    }, cb);
}
inline std::vector<std::string> lines(const std::string& s) {
    std::vector<std::string> v;
    std::size_t a = 0, b;
    while ((b = s.find('\n', a)) != std::string::npos) { v.push_back(s.substr(a, b - a)); a = b + 1; }
    if (a < s.size()) v.push_back(s.substr(a));
    return v;
}
// nmcli -t: fält delas med ':', '\:' är ett kolon i värdet
inline std::vector<std::string> terse(const std::string& line) {
    std::vector<std::string> f{""};
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\\' && i + 1 < line.size()) { f.back() += line[++i]; continue; }
        if (line[i] == ':') { f.emplace_back(); continue; }
        f.back() += line[i];
    }
    return f;
}

// Systemljud ur det valda ljudtemat ("done", "toggle" … eller freedesktop-namn som "message-new-instant")
inline void playSound(const char* name) { wenuy_play_sound(name); }

// Mörkt tema valt? Samma fil som "wenuyos get dark" läser (~/.config/gtk-3.0/settings.ini)
inline bool prefersDark() {
    GKeyFile* k = g_key_file_new();
    const std::string f = std::string(g_get_user_config_dir()) + "/gtk-3.0/settings.ini";
    bool dark = false;
    if (g_key_file_load_from_file(k, f.c_str(), G_KEY_FILE_NONE, nullptr)) {
        gchar* v = g_key_file_get_value(k, "Settings", "gtk-application-prefer-dark-theme", nullptr);
        dark = v && (g_strcmp0(g_strstrip(v), "true") == 0 || g_strcmp0(v, "1") == 0);
        g_free(v);
    }
    g_key_file_free(k);
    return dark;
}

// Ljust/mörkt: läses ur filen när programmet startar och följer sedan ändringar direkt (Inställningar,
// snabbinställningarna, wenuyos set dark) – förut stannade panelen, Start och öppna program i det gamla läget.
inline void applyDarkSetting() {
    adw_style_manager_set_color_scheme(adw_style_manager_get_default(), prefersDark() ? ADW_COLOR_SCHEME_FORCE_DARK : ADW_COLOR_SCHEME_FORCE_LIGHT);
}
inline void followDarkSetting() {
    applyDarkSetting();
    static GFileMonitor* mon = nullptr;
    if (mon) return;
    GFile* f = g_file_new_for_path((std::string(g_get_user_config_dir()) + "/gtk-3.0/settings.ini").c_str());
    mon = g_file_monitor_file(f, G_FILE_MONITOR_NONE, nullptr, nullptr);   // lever lika länge som programmet
    g_object_unref(f);
    if (mon) g_signal_connect(mon, "changed", G_CALLBACK(+[](GFileMonitor*, GFile*, GFile*, GFileMonitorEvent e, gpointer) {
        if (e == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT || e == G_FILE_MONITOR_EVENT_CREATED || e == G_FILE_MONITOR_EVENT_MOVED_IN)
            applyDarkSetting();
    }), nullptr);
}

// WenuyOS stil: accent och hörn kommer från det centrala temat (wenuyos theme)
inline void loadStyle(const char* extra = "") {
    std::string css = std::string(R"CSS(
.wenuy-hint { opacity: 0.7; font-size: 0.92em; }
.wenuy-signal { font-family: monospace; letter-spacing: -1px; color: @accent_color; }
row.wenuy-active { background: alpha(@accent_bg_color, 0.10); }
)CSS") + extra;
    auto* p = gtk_css_provider_new();
    gtk_css_provider_load_from_string(p, css.c_str());
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    wenuy_diag_init(g_application_get_default() ? g_application_get_application_id(g_application_get_default()) : nullptr);
    wenuy_theme_apply_style();   // accent, hörn och animationer från wenuyos theme – följer ändringar direkt
    followDarkSetting();   // ljust/mörkt ur filen – och följer när det ändras (förut bara vid start)
    if (readLine(std::string(g_get_user_config_dir()) + "/wenuyos/motion") == "off" || lowProfile())
        g_object_set(gtk_settings_get_default(), "gtk-enable-animations", FALSE, nullptr);
}

inline GtkWidget* switchRow(const char* title, const char* subtitle, bool on) {
    auto* r = adw_switch_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(r), title);
    if (subtitle) adw_action_row_set_subtitle(ADW_ACTION_ROW(r), subtitle);
    adw_switch_row_set_active(ADW_SWITCH_ROW(r), on);
    return r;
}
inline GtkWidget* actionRow(const char* title, const char* subtitle, const char* icon = nullptr) {
    auto* r = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(r), title);
    if (subtitle) adw_action_row_set_subtitle(ADW_ACTION_ROW(r), subtitle);
    if (icon) adw_action_row_add_prefix(ADW_ACTION_ROW(r), gtk_image_new_from_icon_name(icon));
    return r;
}
inline GtkWidget* button(const char* label, bool suggested, std::function<void()> fn) {
    auto* b = gtk_button_new_with_label(label);
    gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
    if (suggested) gtk_widget_add_css_class(b, "suggested-action");
    auto* f = new std::function<void()>(std::move(fn));
    g_signal_connect_data(b, "clicked", CB(+[](GtkButton*, gpointer d) { (*static_cast<std::function<void()>*>(d))(); }), f,
                          +[](gpointer d, GClosure*) { delete static_cast<std::function<void()>*>(d); }, (GConnectFlags)0);
    return b;
}
inline GtkWidget* group(const char* title, const char* desc = nullptr) {
    auto* g = adw_preferences_group_new();
    if (title) adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(g), title);
    if (desc) adw_preferences_group_set_description(ADW_PREFERENCES_GROUP(g), desc);
    return g;
}
inline void add(GtkWidget* g, GtkWidget* w) { adw_preferences_group_add(ADW_PREFERENCES_GROUP(g), w); }


// Starta ett program från dess .desktop-fil utan en extra "gio launch"-process: Exec-raden tolkas här
// och körs direkt i ett eget programområde (wenuyos-app-run). Sparar 50–150 ms på en svag dator.
// Fält som %f/%u (filer) tas bort; Terminal=true och D-Bus-aktivering går den vanliga vägen.
inline void launchApp(GAppInfo* info) {
    GDesktopAppInfo* d = G_IS_DESKTOP_APP_INFO(info) ? G_DESKTOP_APP_INFO(info) : nullptr;
    const char* file = d ? g_desktop_app_info_get_filename(d) : nullptr;
    if (d && !g_desktop_app_info_get_boolean(d, "Terminal") && !g_desktop_app_info_get_boolean(d, "DBusActivatable")) {
        if (const char* cmd = g_app_info_get_commandline(info)) {
            gint argc = 0;
            gchar** argv = nullptr;
            if (g_shell_parse_argv(cmd, &argc, &argv, nullptr)) {
                std::vector<std::string> a = {"wenuyos-app-run"};
                for (int i = 0; i < argc; ++i) {
                    std::string x = argv[i];
                    if (x.size() == 2 && x[0] == '%') continue;            // %f %F %u %U %i %c %k
                    std::string y;
                    for (size_t j = 0; j < x.size(); ++j) {                 // %% → %
                        if (x[j] == '%' && j + 1 < x.size() && x[j + 1] == '%') { y += '%'; ++j; } else y += x[j];
                    }
                    a.push_back(y);
                }
                g_strfreev(argv);
                if (a.size() > 1) { spawn(a); return; }
            }
        }
    }
    if (file) { spawn({"wenuyos-app-run", "gio", "launch", file}); return; }
    g_app_info_launch(info, nullptr, nullptr, nullptr);
}

// ---- Senaste filer i Start (förenkling 0.3.18) ----
// Bara filer som öppnats i WenuyOS egna program syns i Start: programmen anropar noteRecent() med
// programnamnet "wenuyos". Av/på (`wenuyos set recent off`) och "Rensa listan" i Inställningar.
// Öppna en fil eller mapp med standardprogrammet (mimeapps.list) direkt – inte via portalen. I Debian 13 går
// GtkFileLauncher via portalen, vars val-ruta inte hittar WenuyOS program och föreslår "Programvara".
// false = inget standardprogram / gick inte (låt då GtkFileLauncher fråga).
inline bool openWithDefault(GtkWidget* parent, GFile* f) {
    GFileInfo* fi = g_file_query_info(f, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
    const char* ct = fi ? g_file_info_get_content_type(fi) : nullptr;
    GAppInfo* app = ct ? g_app_info_get_default_for_type(ct, FALSE) : nullptr;
    if (fi) g_object_unref(fi);
    if (!app) return false;
    GdkAppLaunchContext* ctx = gdk_display_get_app_launch_context(parent ? gtk_widget_get_display(parent) : gdk_display_get_default());
    GList* files = g_list_append(nullptr, f);
    const gboolean ok = g_app_info_launch(app, files, G_APP_LAUNCH_CONTEXT(ctx), nullptr);
    g_list_free(files);
    g_object_unref(ctx);
    g_object_unref(app);
    return ok;
}
inline bool recentEnabled() { return readLine(std::string(g_get_user_config_dir()) + "/wenuyos/recent") != "off"; }
inline void noteRecent(GFile* f) {
    if (!f || !recentEnabled()) return;
    gchar* uri = g_file_get_uri(f);
    if (!uri) return;
    gchar* type = nullptr;
    if (GFileInfo* i = g_file_query_info(f, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, nullptr, nullptr)) {
        if (const char* ct = g_file_info_get_content_type(i)) type = g_content_type_get_mime_type(ct);
        g_object_unref(i);
    }
    std::string exec = std::string(g_get_prgname() ? g_get_prgname() : "xdg-open") + " %u";
    GtkRecentData d = {};
    d.mime_type = type ? type : const_cast<char*>("application/octet-stream");
    d.app_name = const_cast<char*>("wenuyos");
    d.app_exec = const_cast<char*>(exec.c_str());
    gtk_recent_manager_add_full(gtk_recent_manager_get_default(), uri, &d);
    g_free(type);
    g_free(uri);
}
inline void noteRecentPath(const std::string& path) {
    GFile* f = g_file_new_for_path(path.c_str());
    noteRecent(f);
    g_object_unref(f);
}

// ---- Fästa program i dockan: ~/.config/wenuyos/dock-pins, ett .desktop-namn per rad ----
inline std::string pinsFile() { return std::string(g_get_user_config_dir()) + "/wenuyos/dock-pins"; }
inline std::vector<std::string> dockPins() {
    std::vector<std::string> v;
    std::ifstream f(pinsFile());
    for (std::string l; std::getline(f, l);) if (!l.empty() && l[0] != '#') v.push_back(l);
    return v;
}
inline bool isPinned(const std::string& id) {
    for (auto& p : dockPins()) if (p == id) return true;
    return false;
}
inline void setPinOrder(const std::vector<std::string>& v) {
    g_mkdir_with_parents((std::string(g_get_user_config_dir()) + "/wenuyos").c_str(), 0755);
    std::string data;
    for (auto& p : v) data += p + "\n";
    g_file_set_contents(pinsFile().c_str(), data.c_str(), -1, nullptr);
}
// ---- Fästa filer i Start: ~/.config/wenuyos/start-files, en adress (file://…) per rad (Start bevakar filen) ----
inline std::string startFilesFile() { return std::string(g_get_user_config_dir()) + "/wenuyos/start-files"; }
inline std::vector<std::string> startFiles() {
    std::vector<std::string> v;
    std::ifstream f(startFilesFile());
    for (std::string l; std::getline(f, l);) if (!l.empty()) v.push_back(l);
    return v;
}
inline bool isStartFile(const std::string& uri) {
    for (auto& p : startFiles()) if (p == uri) return true;
    return false;
}
inline void writeStartFiles(const std::vector<std::string>& v) {
    std::string data;
    for (auto& x : v) data += x + "\n";
    g_mkdir_with_parents((std::string(g_get_user_config_dir()) + "/wenuyos").c_str(), 0755);
    g_file_set_contents(startFilesFile().c_str(), data.c_str(), -1, nullptr);
}
inline void setPinned(const std::string& id, bool on) {
    auto v = dockPins();
    std::vector<std::string> out;
    for (auto& p : v) if (p != id) out.push_back(p);
    if (on) out.push_back(id);
    g_mkdir_with_parents((std::string(g_get_user_config_dir()) + "/wenuyos").c_str(), 0755);
    std::string data;
    for (auto& p : out) data += p + "\n";
    g_file_set_contents(pinsFile().c_str(), data.c_str(), -1, nullptr);   // panelen bevakar filen
}

// Utvecklarverktyg (WenuyOS Studio, guiden …) syns först när SDK:n är installerad (`wenuyos install dev`).
// Märks i .desktop-filen med X-WenuyOS-SDK=true.
inline bool sdkInstalled() { return g_file_test("/var/lib/wenuyos/sdk-installed", G_FILE_TEST_EXISTS); }
inline bool needsSdk(GAppInfo* a) {
    if (!G_IS_DESKTOP_APP_INFO(a)) return false;
    return g_desktop_app_info_get_boolean(G_DESKTOP_APP_INFO(a), "X-WenuyOS-SDK");
}

}  // namespace wenuy
