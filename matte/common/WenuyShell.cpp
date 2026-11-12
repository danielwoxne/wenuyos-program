// libwenuy – skalets byggstenar: Start (en enda yta sedan 0.3.18) och aktivitetsfältets knappar.
//
// Start (0.4.0): till vänster en plats för skalets mini-terminal, till höger öppna fönster, fästa program och
// senaste filer (eller appnätet), under det Program/Filer · Internet · Inställningar · På/Av och längst ned
// sökfältet. Skriver man byts vyn mot förslag i fast ordning (öppna fönster → program → inställningar → filer
// och mappar → räknare/webbadress → webben); börjar texten med ett kommando visas bara "Kör i terminalen".
// Biblioteket bygger innehållet; den som visar det (panelen) lägger det i ett fönster och stänger
// fönstret när wenuy_shell_set_done_func() anropas.
#include "WenuyShell.h"
#include "WenuyCalc.h"
#include "WenuyShellParse.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gstdio.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <memory>

#include "Wenuy.h"
#include "WenuyIcon.h"

namespace wenuy {
namespace {

WenuyShellLaunchFunc gLaunched = nullptr;
gpointer gLaunchedData = nullptr;

// ---- Ofta använda (0.4.0): ~/.local/state/wenuyos/usage och choices – bara på datorn, av med Integritet → senaste
std::string stateFile(const char* name) { return std::string(g_get_user_state_dir()) + "/wenuyos/" + name; }
std::string readAll(const std::string& path) {
    gchar* t = nullptr;
    std::string r;
    if (g_file_get_contents(path.c_str(), &t, nullptr, nullptr)) { r = t; g_free(t); }
    return r;
}
void writeAll(const std::string& path, const std::string& text) {
    gchar* dir = g_path_get_dirname(path.c_str());
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
    g_file_set_contents(path.c_str(), text.c_str(), -1, nullptr);
}
long nowSec() { return (long)(g_get_real_time() / G_USEC_PER_SEC); }
std::map<std::string, Use> loadUsage() { return parseUsage(readAll(stateFile("usage"))); }
std::map<std::string, std::string> loadChoices() { return parseChoices(readAll(stateFile("choices"))); }
std::string gLastQuery;   // det som stod i sökfältet när en träff valdes
void noteUse(const std::string& key) {
    if (key.empty() || !recentEnabled()) return;
    auto u = loadUsage();
    recordUse(u, key, nowSec());
    writeAll(stateFile("usage"), usageText(u));
}
void noteChoice(const std::string& key) {
    std::string q = gLastQuery;
    while (!q.empty() && q.back() == ' ') q.pop_back();
    if (key.empty() || q.empty() || q.size() > 60 || q.find_first_of("\t\n") != std::string::npos || !recentEnabled()) return;
    gchar* l = g_utf8_strdown(q.c_str(), -1);
    auto c = loadChoices();
    c[l] = key;
    g_free(l);
    if (c.size() > 300) c.erase(c.begin());
    writeAll(stateFile("choices"), choicesText(c));
}
std::string appKey(GAppInfo* a) { const char* id = g_app_info_get_id(a); return id ? std::string("app:") + id : ""; }

// Vad menyerna gör – samma som panelen gjorde förut, nu inbyggt i biblioteket
struct Hooks {
    void spawn(const std::vector<std::string>& c) { wenuy::spawn(c); }
    void launch(GAppInfo* info) {
        noteUse(appKey(info));   // ofta använda
        wenuy::launchApp(info);
        if (gLaunched) gLaunched(info, gLaunchedData);   // panelen visar "startar …" direkt
    }
    std::vector<GAppInfo*> apps() {   // synliga program, sorterade (läses om när program installeras)
        static std::vector<GAppInfo*> cache;
        static bool watching = false;
        if (!watching) {
            watching = true;
            g_signal_connect(g_app_info_monitor_get(), "changed", G_CALLBACK(+[](GAppInfoMonitor*, gpointer) {
                for (auto* a : cache) g_object_unref(a);
                cache.clear();
            }), nullptr);
        }
        if (cache.empty()) {
            GList* all = g_app_info_get_all();
            for (GList* l = all; l; l = l->next)
                if (g_app_info_should_show(G_APP_INFO(l->data))) cache.push_back(G_APP_INFO(g_object_ref(l->data)));
            g_list_free_full(all, g_object_unref);
            std::sort(cache.begin(), cache.end(), [](GAppInfo* a, GAppInfo* b) { return g_utf8_collate(g_app_info_get_name(a), g_app_info_get_name(b)) < 0; });
        }
        return cache;
    }
    void lockScreen() { spawn({"gtklock", "-d"}); }
    void logout() { spawn({"sh", "-c", "loginctl terminate-session \"$XDG_SESSION_ID\" || labwc --exit"}); }
    void windowActions() { spawn({"wenuyos-panel", "--actions"}); }
} H;
WenuyShellDoneFunc gDone = nullptr;
gpointer gDoneData = nullptr;
void shellDone() { if (gDone) gDone(gDoneData); }
WenuyShellSwitchFunc gOpenSearch = nullptr, gOpenStart = nullptr;
gpointer gSwitchData = nullptr;
// Öppna fönster (från skalet) och mini-terminalen (0.4.0)
WenuyShellWindowsFunc gWindows = nullptr;
WenuyShellActivateFunc gActivate = nullptr;
gpointer gWindowsData = nullptr;
WenuyShellCommandFunc gCommand = nullptr;
gpointer gCommandData = nullptr;
WenuyShellTabListFunc gTabListFunc = nullptr;
WenuyShellTabListFunc gTypeFunc = nullptr;   // skriv text i mini-terminalen utan Enter (AI-svarets kommandon)
std::string gTabList;
void showTabList() { if (gTabListFunc) gTabListFunc(gTabList.c_str(), gCommandData); }
std::vector<OpenWindow> openWindows() {
    if (!gWindows) return {};
    gchar* txt = gWindows(gWindowsData);
    if (!txt) return {};
    auto r = parseWindowLines(txt);
    g_free(txt);
    return r;
}
// Ett kommando från sökfältet: till mini-terminalen om skalet har en, annars Konsol
// Senaste kommandona från sökfältet (~/.local/state/wenuyos/commands, nyast först, högst 30) – föreslås igen
std::string historyFile() { return std::string(g_get_user_state_dir()) + "/wenuyos/commands"; }
std::vector<std::string> commandHistory() {
    std::vector<std::string> r;
    gchar* txt = nullptr;
    if (g_file_get_contents(historyFile().c_str(), &txt, nullptr, nullptr)) {
        r = parseHistory(txt);
        g_free(txt);
    }
    return r;
}
void rememberCommand(const std::string& q) {
    if (!recentEnabled()) return;   // samma val som senaste filer (Integritet)
    const std::string out = historyText(addToHistory(commandHistory(), q, 30));
    gchar* dir = g_path_get_dirname(historyFile().c_str());
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
    g_file_set_contents(historyFile().c_str(), out.c_str(), -1, nullptr);
}
void runCommand(const std::string& q) {
    rememberCommand(q);
    if (gCommand && gCommand(q.c_str(), gCommandData)) return;
    H.spawn({"wenuyos-terminal", "-H", "sh", "-c", q});
}

bool sv() { const char* l = g_getenv("LANG"); return l && g_str_has_prefix(l, "sv"); }
const char* T(const char* en, const char* s) { return sv() ? s : en; }
std::string lower(const std::string& s) {
    gchar* l = g_utf8_strdown(s.c_str(), -1);
    std::string r = l;
    g_free(l);
    return r;
}

// ------------------------------------------------------------------ sökträffar
struct Result {
    std::string title, subtitle, icon;   // icon: ikonnamn ur temat eller wenuy-ikon ("w:namn")
    std::function<void()> run;
    int section = 0;            // Section: rubriken träffen står under
    std::string desktop = {};   // programmets .desktop-fil (högerklick → Fäst i dockan, Avinstallera …)
    int rank = 0;               // ordning inom avsnittet
    bool likely = false;        // markeras från början
    bool keep = false;          // Start stängs inte (kommandot visas i mini-terminalen)
    std::string key = {};       // för "ofta använda": vad som valdes för den här sökningen
    std::string file = {};      // filens adress (högerklick → Fäst i Start, dra till terminalen)
};

struct SettingsPage { const char* id; const char* en; const char* sv; const char* icon; const char* kw; };
const SettingsPage kPages[] = {   // samma sidor som i wenuyos-settings (kSections)
    {"wifi", "Network", "Nätverk", "w:wifi-3", "wifi wi-fi nätverk network internet trådlöst vpn ethernet kabel proxy"},
    {"bluetooth", "Bluetooth", "Bluetooth", "w:bluetooth", "bluetooth hörlurar headphones mus mouse högtalare parkoppla"},
    {"display", "Screen", "Skärm", "w:display", "skärm screen ljusstyrka brightness skala scale upplösning släck lås vrid rotation"},
    {"sound", "Sound", "Ljud", "w:volume-2", "ljud sound volym volume mikrofon microphone högtalare"},
    {"mouse", "Mouse and touchpad", "Mus och pekplatta", "w:mouse", "mus mouse pekplatta touchpad klick rullning hastighet vänsterhänt"},
    {"keyboard", "Keyboard", "Tangentbord", "w:keyboard", "tangentbord keyboard layout upprepning kortkommandon"},
    {"printers", "Printers", "Skrivare", "w:printer", "skrivare printer utskrift"},
    {"power", "Power and battery", "Ström och batteri", "w:battery-2", "ström power batteri battery strömspar lock viloläge"},
    {"look", "Appearance", "Utseende", "w:palette", "tema theme mörkt dark bakgrund wallpaper färg accent textstorlek"},
    {"notifications", "Notifications", "Aviseringar", "w:bell", "aviseringar notifications stör ej"},
    {"language", "Language and region", "Språk och region", "w:language", "språk language svenska tidszon timezone klocka datum 24"},
    {"apps", "Apps", "Appar", "w:apps", "program appar flatpak android webbappar aurora microg"},
    {"android", "Android", "Android", "w:android", "android waydroid appar apk mus finger delade mappar aurora microg fdroid"},
    {"defaults", "Default programs", "Standardprogram", "w:apps", "standardprogram default öppna med webbläsare e-post pdf"},
    {"users", "Users", "Användare", "w:users", "användare users konto lösenord password inloggning"},
    {"access", "Accessibility", "Tillgänglighet", "w:accessibility", "tillgänglighet kontrast större text muspekare förstoring skärmläsare"},
    {"storage", "Storage", "Lagring", "w:storage", "lagring disk utrymme rensa papperskorg"},
    {"performance", "Performance", "Prestanda", "w:gauge", "prestanda minne frysa grafik schemaläggare"},
    {"privacy", "Privacy", "Integritet", "w:privacy", "integritet privacy säkerhet mätdata senaste filer recent"},
    {"updates", "Updates", "Uppdateringar", "w:update", "uppdatera update usb ångra felrapport"},
    {"system", "About this computer", "Om datorn", "w:computer", "system om about version kärna licenser modell"},
};

// Räknare och enhetsomvandling i sökfältet (wenuy::calc, samma som Kalkylatorn):
// "2^10+5", "sqrt(2)", "200 + 10 %", "5 km i mil", "100 f i c"
bool calculate(const std::string& q, std::string& out) {
    if (q.find_first_of("0123456789") == std::string::npos) return false;
    if (wenuy::calc::convert(q, out, sv())) return true;
    // bara uttryck med en räkneoperation eller funktion (inte ett ensamt tal eller en vanlig sökning)
    const std::string l = lower(q);
    bool op = q.find_first_of("+*/^%!×÷") != std::string::npos || (q.find('-') != std::string::npos && q.find('-') > 0);
    for (const char* f : {"sqrt", "rot", "sin", "cos", "tan", "log", "ln", "pi", "π", "mod", "exp", "abs"}) if (l.find(f) != std::string::npos) op = true;
    if (!op) return false;
    for (unsigned char c : l) if (isalpha(c) && !strchr("sqrtoinclgpmdexab", c)) return false;   // vanliga ord är ingen räkning
    double v;
    if (!wenuy::calc::evaluate(q, v)) return false;
    out = wenuy::calc::format(v, sv());
    return true;
}

int matchScore(const std::string& hay, const std::string& name, const std::string& q) {
    const std::string n = lower(name);
    if (n == q) return 100;
    if (n.rfind(q, 0) == 0) return 80;
    if (n.find(" " + q) != std::string::npos) return 60;
    if (hay.rfind(q, 0) == 0 || hay.find(" " + q) != std::string::npos) return 45;   // ord i beskrivning/nyckelord ("ter" → Terminal)
    if (n.find(q) != std::string::npos) return 40;
    if (hay.find(q) != std::string::npos) return 25;
    return 0;
}

// "svt.se", "www.dn.se/nyheter", "https://…" → adressen att öppna (annars tomt). Filnamn som "brev.pdf" räknas inte.
std::string webAddress(const std::string& q) {
    if (q.empty() || q.find(' ') != std::string::npos) return "";
    const std::string l = lower(q);
    if (l.rfind("http://", 0) == 0 || l.rfind("https://", 0) == 0) return q;
    std::string host = l.substr(0, l.find_first_of("/?#:"));
    if (host.rfind("www.", 0) == 0 && host.size() > 4) return "https://" + q;
    const auto dot = host.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= host.size()) return "";
    const std::string tld = host.substr(dot + 1);
    if (tld.size() < 2 || tld.size() > 24) return "";
    for (char c : tld) if (c < 'a' || c > 'z') return "";
    for (char c : host) if (!(isalnum((unsigned char)c) || c == '.' || c == '-')) return "";
    static const char* files[] = {"txt", "pdf", "png", "jpg", "jpeg", "gif", "webp", "svg", "doc", "docx", "odt", "ods", "odp", "xls", "xlsx",
                                  "ppt", "pptx", "zip", "tar", "gz", "xz", "bz2", "zst", "rar", "mp3", "mp4", "mkv", "avi", "wav", "ogg", "flac",
                                  "cpp", "hpp", "py", "sh", "md", "json", "xml", "csv", "iso", "deb", "desktop", "lha", "adf", "prg", "exe",
                                  "js", "css", "log", "conf", "ini", "html", "htm", "wup", "rs", "kt", "java", "lua", "toml", "yaml", "yml"};
    if (l.find('/') == std::string::npos)
        for (const char* f : files) if (tld == f) return "";
    return "https://" + q;
}

// Sökningen i Start. Fast ordning: program → inställningar → filer och mappar → räknare/webbadress →
// webbsökning. Det troligaste (likely) markeras; börjar texten med ett kommando visas bara "Kör i Konsol".
enum Section { SecTop = 0, SecWindows, SecApps, SecSettings, SecFiles, SecPages, SecCalc, SecWeb };

// Webbsidor ur Firefox historik/bokmärken (~/.cache/wenuyos/webb-historik.tsv, skrivs av wenuyos-webb --export-history
// när Start öppnas och Firefox har ändrat något) – läses om bara när filen ändrats
const std::vector<Page>& firefoxPages() {
    static std::vector<Page> cache;
    static gint64 seen = -1;
    const std::string f = std::string(g_get_user_cache_dir()) + "/wenuyos/webb-historik.tsv";
    GStatBuf st;
    const gint64 m = g_stat(f.c_str(), &st) == 0 ? (gint64)st.st_mtime : 0;
    if (m != seen) {
        seen = m;
        cache.clear();
        gchar* t = nullptr;
        if (m && g_file_get_contents(f.c_str(), &t, nullptr, nullptr)) { cache = parsePages(t); g_free(t); }
    }
    return cache;
}
// Snabbwebben (wenuyos-webb): Firefox i ett fönster nere till vänster, lika högt som skärmen ovanför fältet
void openWeb(const std::string& url) {
    std::string size;
    GdkDisplay* d = gdk_display_get_default();
    GListModel* mons = d ? gdk_display_get_monitors(d) : nullptr;
    if (mons && g_list_model_get_n_items(mons) > 0) {
        GdkMonitor* m = GDK_MONITOR(g_list_model_get_item(mons, 0));
        GdkRectangle g;
        gdk_monitor_get_geometry(m, &g);
        g_object_unref(m);
        const int w = std::clamp(g.width * 55 / 100, 560, 1000);
        const int h = std::max(400, g.height - 64 - 2 * 6 - 34);   // fältet, glappen, titelraden
        size = std::to_string(w) + "x" + std::to_string(h);
    }
    gchar* webb = g_find_program_in_path("wenuyos-webb");
    if (!webb) { H.spawn({"wenuyos-app-run", "xdg-open", url}); return; }
    g_free(webb);
    if (size.empty()) H.spawn({"wenuyos-webb", url});
    else H.spawn({"wenuyos-webb", url, "--size", size});
}

// ---- Mini-terminalen (0.4.0): korta svar, knappar, Tab-komplettering
// Korta svar: bara de här kommandona (de visar bara något), högst 1 s, aldrig medan man skriver något annat
std::string quickAnswer(const std::string& q) {
    static const char* kSafe[] = {"date", "uptime", "uptime -p", "free -h", "df -h", "df -h /", "whoami", "hostname", "uname -r", "uname -a",
                                  "nproc", "ip -br a", "ip -br addr", "cal", "lsblk", "id", "locale", "timedatectl"};
    bool ok = false;
    for (auto* c : kSafe) if (q == c) ok = true;
    if (!ok) return "";
    static std::map<std::string, std::pair<gint64, std::string>> cache;   // samma sökning inom 5 s: samma svar
    const gint64 t = g_get_monotonic_time();
    if (auto it = cache.find(q); it != cache.end() && t - it->second.first < 5 * G_USEC_PER_SEC) return it->second.second;
    gchar* out = nullptr;
    gint status = 0;
    const std::string cmd = "timeout 1 " + q;
    std::string r;
    if (g_spawn_command_line_sync(cmd.c_str(), &out, nullptr, &status, nullptr) && out) r = out;
    g_free(out);
    while (!r.empty() && (r.back() == '\n' || r.back() == ' ')) r.pop_back();
    if (r.size() > 600) r = r.substr(0, 600);
    cache[q] = {t, r};
    return r;
}
std::string buttonsFile() { return std::string(g_get_user_config_dir()) + "/wenuyos/terminal-buttons"; }
void saveTerminalButton(const std::string& cmd) {
    auto v = parseHistory(readAll(buttonsFile()));
    for (auto& c : v) if (c == cmd) return;
    v.push_back(cmd);
    writeAll(buttonsFile(), historyText(v));   // mini-terminalen bevakar filen
}
// Tab: kommandon (första ordet) eller filer och mappar (sökvägen i sista ordet, ~ = hemmappen)
std::vector<std::string> tabCandidates(const std::string& line) {
    std::vector<std::string> r;
    const size_t sp = line.find_last_of(' ');
    const std::string word = sp == std::string::npos ? line : line.substr(sp + 1);
    if (sp == std::string::npos) {   // kommandon i PATH
        static std::vector<std::string> cmds;
        if (cmds.empty()) {
            std::set<std::string> all;
            const char* path = g_getenv("PATH");
            gchar** dirs = g_strsplit(path ? path : "/usr/bin:/bin", ":", -1);
            for (gchar** d = dirs; *d; ++d)
                if (GDir* dir = g_dir_open(*d, 0, nullptr)) {
                    while (const char* n = g_dir_read_name(dir)) all.insert(n);
                    g_dir_close(dir);
                }
            g_strfreev(dirs);
            cmds.assign(all.begin(), all.end());
        }
        for (auto& c : cmds) if (c.rfind(word, 0) == 0) r.push_back(c);
        if (r.size() > 200) r.resize(200);
        return r;
    }
    std::string w = word, home = g_get_home_dir();
    const bool tilde = w.rfind("~", 0) == 0;
    if (tilde) w = home + w.substr(1);
    const size_t slash = w.find_last_of('/');
    const std::string dir = slash == std::string::npos ? "." : (slash == 0 ? "/" : w.substr(0, slash));
    const std::string base = slash == std::string::npos ? w : w.substr(slash + 1);
    const std::string shownDir = slash == std::string::npos ? "" : word.substr(0, word.find_last_of('/') + 1);
    std::string cwd = home;   // mini-terminalen börjar i hemmappen
    const std::string full = dir.rfind("/", 0) == 0 ? dir : cwd + "/" + dir;
    if (GDir* d = g_dir_open(full.c_str(), 0, nullptr)) {
        while (const char* n = g_dir_read_name(d)) {
            if (!g_str_has_prefix(n, base.c_str()) || (n[0] == '.' && base.empty())) continue;
            const bool isDir = g_file_test((full + "/" + n).c_str(), G_FILE_TEST_IS_DIR);
            r.push_back(shownDir + n + (isDir ? "/" : ""));
        }
        g_dir_close(d);
    }
    std::sort(r.begin(), r.end());
    return r;
}

// Snabbkommandona: titel, förklaring och vad som körs
bool quickResult(const std::string& lq, Result& out) {
    const QuickCmd c = parseQuick(lq);
    if (c.kind == Quick::None) return false;
    const std::string n = std::to_string(c.value);
    auto mk = [&](const std::string& title, const char* sub, const char* icon, std::vector<std::string> cmd) {
        out = Result{title, sub, icon, [cmd] { H.spawn(cmd); }, SecTop};
        out.key = "quick:" + lq;
    };
    const char* now = T("Enter does it now", "Enter gör det direkt");
    switch (c.kind) {
        case Quick::Volume: mk(std::string(T("Volume ", "Volym ")) + n + " %", now, "w:volume-2",
                               {"sh", "-c", "wpctl set-mute @DEFAULT_AUDIO_SINK@ 0; wpctl set-volume @DEFAULT_AUDIO_SINK@ " + n + "%"}); break;
        case Quick::Mute: mk(T("Sound off", "Ljud av"), now, "w:volume-2", {"wpctl", "set-mute", "@DEFAULT_AUDIO_SINK@", "1"}); break;
        case Quick::Unmute: mk(T("Sound on", "Ljud på"), now, "w:volume-2", {"wpctl", "set-mute", "@DEFAULT_AUDIO_SINK@", "0"}); break;
        case Quick::WifiOn: mk(T("Turn Wi-Fi on", "Slå på Wi-Fi"), now, "w:wifi-3", {"nmcli", "radio", "wifi", "on"}); break;
        case Quick::WifiOff: mk(T("Turn Wi-Fi off", "Stäng av Wi-Fi"), now, "w:wifi-3", {"nmcli", "radio", "wifi", "off"}); break;
        case Quick::BtOn: mk(T("Turn Bluetooth on", "Slå på Bluetooth"), now, "w:bluetooth", {"sh", "-c", "rfkill unblock bluetooth; bluetoothctl power on"}); break;
        case Quick::BtOff: mk(T("Turn Bluetooth off", "Stäng av Bluetooth"), now, "w:bluetooth", {"bluetoothctl", "power", "off"}); break;
        case Quick::Dark: mk(T("Dark appearance", "Mörkt utseende"), now, "w:moon", {"wenuyos", "set", "dark", "true"}); break;
        case Quick::Light: mk(T("Light appearance", "Ljust utseende"), now, "w:sun", {"wenuyos", "set", "dark", "false"}); break;
        case Quick::Screenshot: mk(T("Screenshot (drag an area)", "Skärmbild (dra upp ett område)"), now, "w:camera",
                                   {"sh", "-c", "sleep 0.4; exec wenuyos-window screenshot"}); break;
        case Quick::DndOn: mk(T("Do not disturb on", "Stör ej på"), now, "w:bell", {"wenuyos", "set", "dnd", "on"}); break;
        case Quick::DndOff: mk(T("Do not disturb off", "Stör ej av"), now, "w:bell", {"wenuyos", "set", "dnd", "off"}); break;
        case Quick::Brightness: mk(std::string(T("Brightness ", "Ljusstyrka ")) + n + " %", now, "w:sun",
                                   {"brightnessctl", "-q", "set", std::to_string(std::max(3, c.value)) + "%"}); break;
        case Quick::NightOn: mk(T("Night light on", "Nattljus på"), now, "w:sun",
                                {"sh", "-c", "mkdir -p ~/.config/wenuyos; echo on > ~/.config/wenuyos/nightlight; pkill -x wlsunset; exec wlsunset -t 3600 -T 6500 -S 07:00 -s 20:00"}); break;
        case Quick::NightOff: mk(T("Night light off", "Nattljus av"), now, "w:sun",
                                 {"sh", "-c", "mkdir -p ~/.config/wenuyos; echo off > ~/.config/wenuyos/nightlight; pkill -x wlsunset"}); break;
        case Quick::AirOn: mk(T("Airplane mode on", "Flygplansläge på"), now, "w:wifi-3", {"sh", "-c", "nmcli radio all off; rfkill block all"}); break;
        case Quick::AirOff: mk(T("Airplane mode off", "Flygplansläge av"), now, "w:wifi-3", {"sh", "-c", "rfkill unblock all; nmcli radio all on"}); break;
        case Quick::Lock: out = Result{T("Lock the computer", "Lås datorn"), now, "w:lock", [] { H.lockScreen(); }, SecTop}; break;
        case Quick::Logout: out = Result{T("Log out", "Logga ut"), T("Enter logs out – save your work first", "Enter loggar ut – spara först"), "w:logout", [] { H.logout(); }, SecTop}; break;
        case Quick::Restart: mk(T("Restart the computer", "Starta om datorn"), T("Enter restarts – save your work first", "Enter startar om – spara först"), "w:restart", {"systemctl", "reboot"}); break;
        case Quick::Poweroff: mk(T("Shut down the computer", "Stäng av datorn"), T("Enter shuts down – save your work first", "Enter stänger av – spara först"), "w:power", {"systemctl", "poweroff"}); break;
        case Quick::Sleep: mk(T("Sleep", "Viloläge"), now, "w:moon", {"systemctl", "suspend"}); break;
        default: return false;
    }
    return true;
}

// ---- AI i Start (0.4.0): wenuyos ai setup väljer tjänst och nyckel; inget skickas förrän "Fråga AI" väljs
struct AiInfo { std::string id, name, company; bool ready = false; };
AiInfo aiInfo() {
    AiInfo a;
    a.id = readAll(std::string(g_get_user_config_dir()) + "/wenuyos/ai.provider");
    while (!a.id.empty() && (a.id.back() == '\n' || a.id.back() == ' ')) a.id.pop_back();
    if (a.id == "claude") { a.name = "Claude"; a.company = "Anthropic"; }
    else if (a.id == "openai") { a.name = "ChatGPT"; a.company = "OpenAI"; }
    else if (a.id == "gemini") { a.name = "Gemini"; a.company = "Google"; }
    else if (a.id == "ollama") { a.name = "Ollama"; a.company = T("this computer", "den här datorn"); }
    else return a;
    a.ready = a.id == "ollama" || g_file_test((std::string(g_get_user_config_dir()) + "/wenuyos/ai/" + a.id + ".key").c_str(), G_FILE_TEST_EXISTS);
    return a;
}
void askAI(const std::string& q);
std::vector<Result> search(const std::string& raw) {
    wenuy::Measure measure("search");   // WenuyDiag: sökningens tid
    std::vector<Result> r;
    std::string q = raw;
    while (!q.empty() && q.back() == ' ') q.pop_back();
    while (!q.empty() && q.front() == ' ') q.erase(0, 1);
    if (q.empty()) return r;
    std::string lq = lower(q);
    bool uninstallWord = false;   // "avinstallera X" → bara X söks, med Avinstallera-rader
    for (const char* w : {"avinstallera ", "uninstall ", "ta bort ", "remove "})
        if (lq.rfind(w, 0) == 0 && lq.size() > strlen(w)) { lq = lq.substr(strlen(w)); uninstallWord = true; break; }
    // Inställningar direkt (0.4.0): "volym 40", "wifi av", "mörkt läge", "skärmbild", "starta om" … – överst
    if (Result x; quickResult(lq, x)) { x.likely = true; r.push_back(x); }
    // urklipp: "urklipp" / "clipboard" (+ ord för att filtrera) → det man kopierat senast, Enter kopierar det igen
    for (const char* w : {"urklipp", "clipboard", "kopierat"}) {
        if (lq.rfind(w, 0) != 0) continue;
        std::string filter = lq.substr(strlen(w));
        while (!filter.empty() && filter.front() == ' ') filter.erase(0, 1);
        int n = 0;
        for (auto& c : parseClips(readAll(stateFile("clipboard")))) {
            if (!filter.empty() && lower(c.text).find(filter) == std::string::npos) continue;
            GDateTime* d = g_date_time_new_from_unix_local(c.when);
            gchar* hm = d ? g_date_time_format(d, "%H:%M") : nullptr;
            int lines = 1;
            for (char ch : c.text) if (ch == '\n') ++lines;
            std::string sub = std::string(T("Copied ", "Kopierat ")) + (hm ? hm : "") + (lines > 1 ? "  ·  " + std::to_string(lines) + T(" lines", " rader") : "");
            g_free(hm);
            if (d) g_date_time_unref(d);
            const std::string text = c.text;
            Result x{clipPreview(c.text, 70), sub, "w:copy",
                     [text] { gdk_clipboard_set_text(gdk_display_get_clipboard(gdk_display_get_default()), text.c_str()); }, SecTop};
            if (!n) x.likely = true;
            r.push_back(x);
            if (++n == 12) break;
        }
        if (n) r.push_back({T("Clear the clipboard history", "Töm urklippshistoriken"), T("Removes everything saved", "Tar bort allt sparat"), "w:trash",
                            [] { H.spawn({"/usr/lib/wenuyos/wenuyos-clip", "clear"}); }, SecTop});
        else r.push_back({T("The clipboard history is empty", "Urklippshistoriken är tom"),
                          T("Copy text (Ctrl+C) and it shows up here – off: wenuyos set clipboard.history off", "Kopiera text (Ctrl+C) så syns den här – av: wenuyos set clipboard.history off"),
                          "w:copy", nullptr, SecTop});
        return r;
    }
    // Kommandoläge: ett kommando i PATH ("ls -la", "sudo apt update"), "./program" eller "~/…" – bara en rad,
    // Enter kör det i Konsol (fönstret står kvar så att man ser vad som hände)
    if (!uninstallWord) {
        const auto sp = q.find(' ');
        const std::string first = q.substr(0, sp);
        const bool local = first.rfind("./", 0) == 0 || first.rfind("../", 0) == 0 || (first.rfind("/", 0) == 0 && g_file_test(first.c_str(), G_FILE_TEST_IS_EXECUTABLE) && !g_file_test(first.c_str(), G_FILE_TEST_IS_DIR));
        gchar* exe = local || first.empty() || first.find('/') != std::string::npos || first.find('.') != std::string::npos ? nullptr : g_find_program_in_path(first.c_str());
        bool appNamed = false;   // "firefox" ensamt är ett program i listan, inte ett kommando
        if (exe && sp == std::string::npos)
            for (GAppInfo* a : H.apps()) if (lower(g_app_info_get_name(a)).rfind(lq, 0) == 0) { appNamed = true; break; }
        const bool cmd = local || (exe && (sp != std::string::npos || !appNamed));
        g_free(exe);
        if (cmd) {
            const bool mini = gCommand != nullptr;
            Result c{std::string(mini ? T("Run in the terminal: ", "Kör i terminalen: ") : T("Run in Console: ", "Kör i Konsol: ")) + q,
                     mini ? T("Enter runs the command – the result is shown to the left", "Enter kör kommandot – svaret syns till vänster")
                          : T("Enter runs the command", "Enter kör kommandot"), "w:terminal",
                     [q] { runCommand(q); }, SecTop};
            c.likely = true;
            c.keep = mini;
            r.push_back(c);
            for (auto& h : historyMatches(commandHistory(), q, 4)) {   // samma början, körda förut
                Result x{std::string(T("Run again: ", "Kör igen: ")) + h, T("A command you ran before", "Ett kommando du kört förut"), "w:terminal",
                         [h] { runCommand(h); }, SecTop};
                x.keep = mini;
                r.push_back(x);
            }
            // korta svar direkt (bara ofarliga kommandon som bara visar något)
            std::string answer = quickAnswer(q);
            if (!answer.empty()) {
                // tabeller (df, free): första raden är rubrik – värdena som titel, rubriken under
                std::vector<std::string> ls;
                for (size_t a = 0; a <= answer.size();) { size_t e = answer.find('\n', a); if (e == std::string::npos) e = answer.size(); if (e > a) ls.push_back(answer.substr(a, e - a)); a = e + 1; }
                std::string title = ls.empty() ? "" : ls.size() >= 2 ? ls[1] : ls[0];
                std::string sub = ls.size() >= 2 ? ls[0] : T("Enter copies", "Enter kopierar");
                if (ls.size() > 2) sub += "  ·  " + std::to_string(ls.size()) + T(" lines – Enter copies all", " rader – Enter kopierar allt");
                Result x{"= " + title, sub, "w:info",
                         [answer] { gdk_clipboard_set_text(gdk_display_get_clipboard(gdk_display_get_default()), answer.c_str()); }, SecTop};
                r.insert(r.begin(), x);
            }
            if (mini) {   // spara som knapp ovanför mini-terminalen
                Result x{std::string(T("Save as a button: ", "Spara som knapp: ")) + q, T("Shown above the terminal – right click removes it", "Visas ovanför terminalen – högerklick tar bort den"),
                         "w:pin", [q] { saveTerminalButton(q); }, SecTop};
                x.keep = true;
                r.push_back(x);
            }
            return r;
        }
    }
    // "installera github.com/namn/program" (eller namn/program) → installera från internet
    for (const char* w : {"installera ", "install "}) {
        if (lq.rfind(w, 0) != 0) continue;
        std::string what = q.substr(strlen(w));
        while (!what.empty() && what.front() == ' ') what.erase(0, 1);
        if (what.find('/') == std::string::npos || what.find(' ') != std::string::npos) break;
        Result x{std::string(T("Install from the internet: ", "Installera från internet: ")) + what,
                 T("Program, game or anything you put on GitHub, Codeberg, GitLab or a web page", "Program, spel eller annat du lagt på GitHub, Codeberg, GitLab eller en webbsida"),
                 "w:install", [what] { H.spawn({"/usr/lib/wenuyos/wenuyos-get", "--gui", what}); }, SecTop};
        x.likely = true;
        r.push_back(x);
        return r;
    }
    const std::string web = webAddress(q);
    // en adress till ett förråd på GitHub/Codeberg/GitLab: överst "Installera"
    if (!web.empty()) {
        const std::string l = lower(web);
        for (const char* h : {"://github.com/", "://codeberg.org/", "://gitlab.com/"}) {
            const auto p = l.find(h);
            if (p == std::string::npos) continue;
            const std::string path = l.substr(p + strlen(h));
            const auto slash = path.find('/');
            if (slash == std::string::npos || slash == 0 || slash + 1 >= path.size()) break;
            r.push_back({std::string(T("Install ", "Installera ")) + q, T("Download and install it on this computer", "Hämta och installera det på den här datorn"),
                         "w:install", [web] { H.spawn({"/usr/lib/wenuyos/wenuyos-get", "--gui", web}); }, SecTop});
            break;
        }
    }
    // Öppna fönster (Alt+Tab eller sökfältet – det finns ingen aktivitetsrad)
    if (!uninstallWord)
        for (auto& w : openWindows()) {
            const std::string hay = lower(w.title + " " + w.appId);
            if (hay.find(lq) == std::string::npos) continue;
            GIcon* gi = wenuy_app_icon_for_id(w.appId.c_str());
            gchar* icon = gi ? g_icon_to_string(gi) : nullptr;
            if (gi) g_object_unref(gi);
            const std::string id = w.id;
            r.push_back({w.title, T("Open window – Enter switches to it", "Öppet fönster – Enter byter till det"), icon ? icon : "application-x-executable",
                         [id] { if (gActivate) gActivate(id.c_str(), gWindowsData); }, SecWindows});
            g_free(icon);
        }
    const bool sdk = sdkInstalled();
    const auto usage = loadUsage();
    const auto choices = loadChoices();
    const long now = nowSec();
    // Program
    std::vector<Result> apps;
    std::vector<Result> actions;   // programmens egna åtgärder ("Nytt privat fönster" i Firefox …)
    for (GAppInfo* a : H.apps()) {
        if (!sdk && needsSdk(a)) continue;
        std::string name = g_app_info_get_name(a), hay = lower(name);
        if (const char* d = g_app_info_get_description(a)) hay += " " + lower(d);
        if (G_IS_DESKTOP_APP_INFO(a)) {
            if (const char* const* kw = g_desktop_app_info_get_keywords(G_DESKTOP_APP_INFO(a)))
                for (; *kw; ++kw) hay += " " + lower(*kw);
            if (const char* gn = g_desktop_app_info_get_generic_name(G_DESKTOP_APP_INFO(a))) hay += " " + lower(gn);
        }
        if (int s = matchScore(hay, name, lq)) {
            const char* d = g_app_info_get_description(a);
            gchar* icon = nullptr;
            if (GIcon* gi = g_app_info_get_icon(a)) icon = g_icon_to_string(gi);
            std::shared_ptr<GAppInfo> ref(G_APP_INFO(g_object_ref(a)), g_object_unref);   // släpps med raden
            const char* df = G_IS_DESKTOP_APP_INFO(a) ? g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(a)) : nullptr;
            Result x{name, d ? d : T("Program", "Program"), icon ? icon : "application-x-executable",
                     [ref] { H.launch(ref.get()); }, SecApps, df ? df : ""};
            x.key = appKey(a);
            x.rank = s;
            if (auto it = usage.find(x.key); it != usage.end()) x.rank += std::min(40, frecency(it->second, now) / 3);   // ofta använda först
            if (auto it = choices.find(lq); it != choices.end() && it->second == x.key) { x.rank += 100; x.likely = true; }   // samma sökning förut
            apps.push_back(x);
            // "avinstallera spel" / "uninstall x" / "ta bort x" → avinstallera direkt härifrån
            if (df && uninstallWord) {
                const std::string file = df;
                Result u{std::string(T("Uninstall ", "Avinstallera ")) + name, T("Asks first", "Frågar först"), "w:trash",
                         [file] { H.spawn({"/usr/lib/wenuyos/wenuyos-uninstall", file}); }, SecTop};
                u.rank = s;
                r.push_back(u);
            }
            g_free(icon);
        }
        if (G_IS_DESKTOP_APP_INFO(a) && lq.size() >= 3)
            if (const gchar* const* acts = g_desktop_app_info_list_actions(G_DESKTOP_APP_INFO(a)))
                for (; *acts; ++acts) {
                    gchar* an = g_desktop_app_info_get_action_name(G_DESKTOP_APP_INFO(a), *acts);
                    if (!an) continue;
                    const std::string aname = an;
                    g_free(an);
                    const int sa = matchScore(lower(aname + " " + name), aname, lq);
                    if (!sa) continue;
                    gchar* icon = nullptr;
                    if (GIcon* gi = g_app_info_get_icon(a)) icon = g_icon_to_string(gi);
                    std::shared_ptr<GAppInfo> ref(G_APP_INFO(g_object_ref(a)), g_object_unref);
                    const std::string act = *acts;
                    Result x{aname, name, icon ? icon : "application-x-executable",
                             [ref, act] {
                                 noteUse(appKey(ref.get()));
                                 GdkAppLaunchContext* ctx = gdk_display_get_app_launch_context(gdk_display_get_default());
                                 g_desktop_app_info_launch_action(G_DESKTOP_APP_INFO(ref.get()), act.c_str(), G_APP_LAUNCH_CONTEXT(ctx));
                                 g_object_unref(ctx);
                             }, SecApps};
                    x.key = appKey(a) + "#" + act;
                    x.rank = sa - 5;
                    if (auto it = choices.find(lq); it != choices.end() && it->second == x.key) { x.rank += 100; x.likely = true; }
                    actions.push_back(x);
                    g_free(icon);
                }
    }
    std::stable_sort(apps.begin(), apps.end(), [](const Result& a, const Result& b) { return a.rank > b.rank; });
    if (apps.size() > 5) apps.resize(5);
    std::stable_sort(actions.begin(), actions.end(), [](const Result& a, const Result& b) { return a.rank > b.rank; });
    if (actions.size() > 3) actions.resize(3);
    apps.insert(apps.end(), actions.begin(), actions.end());
    std::stable_sort(apps.begin(), apps.end(), [](const Result& a, const Result& b) { return a.rank > b.rank; });
    // Inställningar
    std::vector<Result> sets;
    for (auto& p : kPages) {
        const std::string name = T(p.en, p.sv);
        const std::string hay = lower(std::string(p.en) + " " + p.sv + " " + p.kw + " inställningar settings");
        if (int s = matchScore(hay, name, lq)) {
            const std::string id = p.id;
            Result x{name, T("Settings", "Inställningar"), p.icon,
                     [id] { H.spawn({"wenuyos-app-run", "wenuy-launch", "wenuyos-settings", "--page", id}); }, SecSettings};
            x.rank = s;
            sets.push_back(x);
        }
    }
    std::stable_sort(sets.begin(), sets.end(), [](const Result& a, const Result& b) { return a.rank > b.rank; });
    if (sets.size() > 3) sets.resize(3);
    // Filer och mappar: senaste (WenuyOS-programmens) och hela hemmappen/USB-minnen (plocate)
    std::vector<Result> files;
    std::set<std::string> seen;
    if (recentEnabled()) {
        GList* items = gtk_recent_manager_get_items(gtk_recent_manager_get_default());
        for (GList* l = items; l; l = l->next) {
            auto* it = static_cast<GtkRecentInfo*>(l->data);
            const std::string name = gtk_recent_info_get_display_name(it);
            if (int s = matchScore(lower(name), name, lq)) {
                const std::string uri = gtk_recent_info_get_uri(it);
                gchar* path = g_filename_from_uri(uri.c_str(), nullptr, nullptr);
                if (path && !g_file_test(path, G_FILE_TEST_EXISTS)) { g_free(path); continue; }
                std::string sub = path ? path : uri;
                if (path) seen.insert(path);
                g_free(path);
                const char* home = g_get_home_dir();
                if (sub.rfind(home, 0) == 0) sub = "~" + sub.substr(strlen(home));
                Result x{name, sub, "text-x-generic", [uri] { H.spawn({"gio", "open", uri}); }, SecFiles};
                x.file = uri;
                if (GIcon* gi = gtk_recent_info_get_gicon(it)) { gchar* s2 = g_icon_to_string(gi); if (s2) x.icon = s2; g_free(s2); g_object_unref(gi); }
                x.rank = s + 10;   // nyss öppnad = lite före
                files.push_back(x);
            }
        }
        g_list_free_full(items, (GDestroyNotify)gtk_recent_info_unref);
    }
    if (lq.size() >= 3 && !uninstallWord) {
        static const bool haveLocate = g_file_test("/usr/bin/plocate", G_FILE_TEST_IS_EXECUTABLE);
        gchar* out = nullptr;
        if (haveLocate) {
            const gchar* argv[] = {"plocate", "-i", "-b", "-l", "200", "--", lq.c_str(), nullptr};
            g_spawn_sync(nullptr, (gchar**)argv, nullptr, (GSpawnFlags)(G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL), nullptr, nullptr, &out, nullptr, nullptr, nullptr);
        }
        const std::string home = g_get_home_dir();
        if (out) {
            gchar** lines = g_strsplit(out, "\n", -1);
            int n = 0;
            for (gchar** l = lines; *l && n < 40; ++l) {
                const std::string path = *l;
                if (path.empty() || seen.count(path) || !g_file_test(path.c_str(), G_FILE_TEST_EXISTS)) continue;
                const bool inHome = path.rfind(home + "/", 0) == 0;
                const bool onUsb = path.rfind("/media/", 0) == 0 || path.rfind("/run/media/", 0) == 0;
                if (!inHome && !onUsb) continue;
                const std::string rel = inHome ? path.substr(home.size()) : path;
                if (rel.find("/.") != std::string::npos || rel.find("/__pycache__") != std::string::npos ||
                    rel.find("/node_modules/") != std::string::npos) continue;   // dolda mappar, cache
                const std::string base = path.substr(path.rfind('/') + 1);
                int sc = matchScore(lower(base), base, lq);
                if (!sc) continue;
                const std::string shown = inHome ? "~" + rel : path;
                const bool dir = g_file_test(path.c_str(), G_FILE_TEST_IS_DIR);
                std::string icon = dir ? "folder" : "text-x-generic";
                if (!dir) {
                    gchar* ct = g_content_type_guess(base.c_str(), nullptr, 0, nullptr);
                    if (GIcon* gi = g_content_type_get_icon(ct)) { gchar* s2 = g_icon_to_string(gi); if (s2) icon = s2; g_free(s2); g_object_unref(gi); }
                    g_free(ct);
                }
                sc -= std::min(20, (int)std::count(rel.begin(), rel.end(), '/') * 3);   // djupt nedgrävt = lite längre ner
                Result x{base, shown, icon, [path] { H.spawn({"gio", "open", path}); }, SecFiles};
                x.rank = sc;
                if (gchar* u = g_filename_to_uri(path.c_str(), nullptr, nullptr)) { x.file = u; g_free(u); }
                files.push_back(x);
                ++n;
            }
            g_strfreev(lines);
            g_free(out);
        }
    }
    // Snabbsök (wenuyos-index): nya filer som plocate inte hunnit se, och ord inne i dokument ("kvitto clas ohlson")
    if (lq.size() >= 3 && !uninstallWord) {
        static const std::string db = std::string(g_get_user_cache_dir()) + "/wenuyos/index.db";
        if (g_file_test(db.c_str(), G_FILE_TEST_EXISTS)) {
            std::vector<std::string> argv{"wenuyos-index", "query", "--content", "--limit", "12"};
            gchar** words = g_strsplit_set(q.c_str(), " \t", -1);
            for (gchar** wd = words; *wd; ++wd) if (**wd) argv.push_back(*wd);
            g_strfreev(words);
            std::vector<const gchar*> av;
            for (auto& a : argv) av.push_back(a.c_str());
            av.push_back(nullptr);
            gchar* out = nullptr;
            g_spawn_sync(nullptr, (gchar**)av.data(), nullptr, (GSpawnFlags)(G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL), nullptr, nullptr, &out, nullptr, nullptr, nullptr);
            const std::string home = g_get_home_dir();
            if (out) {
                for (auto& l : wenuy::lines(out)) {
                    const size_t tab = l.find('\t');
                    if (tab == std::string::npos) continue;
                    const std::string path = l.substr(0, tab), snip = l.substr(tab + 1);
                    if (seen.count(path) || !g_file_test(path.c_str(), G_FILE_TEST_EXISTS)) continue;
                    bool dup = false;
                    for (auto& f2 : files) if (gchar* u = g_filename_to_uri(path.c_str(), nullptr, nullptr)) { if (f2.file == u) dup = true; g_free(u); }
                    if (dup) continue;
                    seen.insert(path);
                    const std::string base = path.substr(path.rfind('/') + 1);
                    std::string shown = path.rfind(home + "/", 0) == 0 ? "~" + path.substr(home.size()) : path;
                    shown = shown.substr(0, shown.rfind('/'));
                    if (!snip.empty()) shown += "  ·  ”" + snip + "”";
                    const bool dir = g_file_test(path.c_str(), G_FILE_TEST_IS_DIR);
                    std::string icon = dir ? "folder" : "text-x-generic";
                    if (!dir) {
                        gchar* ct = g_content_type_guess(base.c_str(), nullptr, 0, nullptr);
                        if (GIcon* gi = g_content_type_get_icon(ct)) { gchar* s2 = g_icon_to_string(gi); if (s2) icon = s2; g_free(s2); g_object_unref(gi); }
                        g_free(ct);
                    }
                    int sc = matchScore(lower(base), base, lq);
                    if (!sc) sc = snip.empty() ? 0 : 25;   // träff inne i texten: under namnträffarna
                    if (!sc) continue;
                    Result x{base, shown, icon, [path] { H.spawn({"gio", "open", path}); }, SecFiles};
                    x.rank = sc;
                    if (gchar* u = g_filename_to_uri(path.c_str(), nullptr, nullptr)) { x.file = u; g_free(u); }
                    files.push_back(x);
                }
                g_free(out);
            }
        }
    }
    std::stable_sort(files.begin(), files.end(), [](const Result& a, const Result& b) { return a.rank > b.rank; });
    if (files.size() > 5) files.resize(5);
    // webbsidor man besökt eller bokmärkt i Firefox ("aft" → aftonbladet.se)
    std::vector<Result> pages;
    if (lq.size() >= 2 && q.find(' ') == std::string::npos)
        for (auto& p : matchPages(firefoxPages(), lq, 3)) {
            const std::string url = p.url, host = hostOf(p.url);
            Result x{p.title.empty() ? host : p.title, host + (p.bookmark ? T("  ·  bookmark", "  ·  bokmärke") : ""), "w:globe",
                     [url] { openWeb(url); }, SecPages};
            x.key = "web:" + host;
            if (auto it = choices.find(lq); it != choices.end() && it->second == x.key) x.likely = true;
            else if (host.rfind(lq, 0) == 0 && lq.size() >= 3 && (apps.empty() || apps.front().rank < 80)) x.likely = true;
            pages.push_back(x);
        }
    for (auto* v : {&apps, &sets, &files, &pages}) r.insert(r.end(), v->begin(), v->end());
    // räknare och webbadress
    std::string calc;
    if (calculate(q, calc)) {
        Result x{"= " + calc, T("Press Enter to copy", "Tryck Enter för att kopiera"), "w:cpu", [calc] {
                     gdk_clipboard_set_text(gdk_display_get_clipboard(gdk_display_get_default()), calc.c_str());
                 }, SecCalc};
        x.likely = true;   // ett räkneuttryck är vad man menade
        r.push_back(x);
    }
    if (!web.empty()) {
        // "aftonbladet.se": sidan direkt i snabbwebben (Firefox, nere till vänster ovanför fältet) – överst
        Result x{std::string(T("Open ", "Öppna ")) + q, T("Enter opens the page – ↗ moves it to Firefox", "Enter öppnar sidan – ↗ flyttar den till Firefox"), "w:globe",
                 [web] { openWeb(web); }, SecTop};
        x.likely = apps.empty() || apps.front().rank < 80;   // om inget program heter så
        r.insert(r.begin(), x);
        r.push_back({std::string(T("Open ", "Öppna ")) + q + T(" in Firefox", " i Firefox"), T("A normal browser window", "Ett vanligt webbläsarfönster"), "w:globe",
                     [web] { H.spawn({"wenuyos-app-run", "xdg-open", web}); }, SecCalc});
    }
    gchar* esc = g_uri_escape_string(q.c_str(), nullptr, FALSE);
    const std::string url = "https://duckduckgo.com/?q=" + std::string(esc);
    g_free(esc);
    r.push_back({std::string(T("Search the web for “", "Sök på webben efter ”")) + q + "”", "DuckDuckGo", "w:globe",
                 [url] { openWeb(url); }, SecWeb});
    // Fråga AI: sist bland förslagen – överst om frågan slutar med ? (inget skickas förrän raden väljs)
    const bool question = q.back() == '?';
    if (AiInfo ai = aiInfo(); ai.ready) {
        Result x{std::string(T("Ask AI: ", "Fråga AI: ")) + q,
                 std::string(T("Sent to ", "Skickas till ")) + ai.name + " (" + ai.company + ")", "w:help", [q] { askAI(q); }, SecWeb};
        x.keep = true;   // svaret visas i Start
        if (question) { x.likely = true; x.section = SecTop; r.insert(r.begin(), x); }
        else r.push_back(x);
    } else if (question) {
        r.push_back({T("Ask an AI – set it up first", "Fråga en AI – ställ in den först"),
                     T("Claude, ChatGPT, Gemini or a local model (Ollama)", "Claude, ChatGPT, Gemini eller en lokal modell (Ollama)"), "w:help",
                     [] { H.spawn({"wenuyos-app-run", "wenuyos-terminal", "-H", "wenuyos", "ai", "setup"}); }, SecWeb});
    }
    return r;
}

GtkWidget* iconFor(const std::string& icon, int size) {
    if (icon.rfind("w:", 0) == 0) return wenuy_color_icon_new(icon.c_str() + 2, size);
    GIcon* gi = g_icon_new_for_string(icon.c_str(), nullptr);
    GtkWidget* img = gi ? gtk_image_new_from_gicon(gi) : gtk_image_new_from_icon_name(icon.c_str());
    if (gi) g_object_unref(gi);
    gtk_image_set_pixel_size(GTK_IMAGE(img), size);
    return img;
}

// En rad i menyer och träfflistor: ikon, titel, (undertext), ev. ▸
GtkWidget* menuRow(const std::string& icon, const std::string& title, const std::string& sub, bool arrow, int iconSize = 24) {
    auto* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(row, "ws-row");
    gtk_box_append(GTK_BOX(row), iconFor(icon, iconSize));
    auto* texts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(texts, GTK_ALIGN_CENTER);
    auto* l = gtk_label_new(title.c_str());
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(l), 34);
    gtk_box_append(GTK_BOX(texts), l);
    if (!sub.empty()) {
        auto* s = gtk_label_new(sub.c_str());
        gtk_label_set_xalign(GTK_LABEL(s), 0);
        gtk_label_set_ellipsize(GTK_LABEL(s), PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars(GTK_LABEL(s), 40);
        gtk_widget_add_css_class(s, "ws-sub");
        gtk_box_append(GTK_BOX(texts), s);
    }
    gtk_widget_set_hexpand(texts, TRUE);
    gtk_box_append(GTK_BOX(row), texts);
    if (arrow) {
        auto* a = gtk_label_new("▸");
        gtk_widget_add_css_class(a, "ws-arrow");
        gtk_box_append(GTK_BOX(row), a);
    }
    return row;
}

// Lista där varje rad har en åtgärd (std::function) – Enter/klick kör den, och Start stängs
GtkWidget* actionList() {
    auto* list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_BROWSE);
    gtk_widget_add_css_class(list, "ws-list");
    g_signal_connect(list, "row-activated", G_CALLBACK(+[](GtkListBox*, GtkListBoxRow* r, gpointer) {
        auto* fn = static_cast<std::function<void()>*>(g_object_get_data(G_OBJECT(r), "run"));
        if (fn && *fn) {
            auto copy = *fn;   // raden kan försvinna när Start stängs
            const bool keep = g_object_get_data(G_OBJECT(r), "keep") != nullptr;
            if (auto* k = static_cast<const char*>(g_object_get_data(G_OBJECT(r), "key"))) noteChoice(k);
            if (!keep) shellDone();
            copy();
        }
    }), nullptr);
    return list;
}
// Markerad rad: egen klass (syns alltid, även utan fokus i listan) + listans markering
void setCurrent(GtkWidget* list, GtkListBoxRow* row) {
    if (auto* old = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(list), "current")))
        if (old != GTK_WIDGET(row) && gtk_widget_get_parent(old) == list) gtk_widget_remove_css_class(old, "ws-current");
    g_object_set_data(G_OBJECT(list), "current", row);
    if (row) {
        gtk_widget_add_css_class(GTK_WIDGET(row), "ws-current");
        gtk_list_box_select_row(GTK_LIST_BOX(list), row);
    }
}
GtkListBoxRow* currentRow(GtkWidget* list) {
    auto* r = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(list), "current"));
    return r && gtk_widget_get_parent(r) == list ? GTK_LIST_BOX_ROW(r) : nullptr;
}
void clearOtherCurrents(GtkWidget* keep, GtkListBoxRow* row);
GtkListBoxRow* addAction(GtkWidget* list, GtkWidget* content, std::function<void()> run) {
    auto* row = gtk_list_box_row_new();
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), content);
    const bool action = static_cast<bool>(run);   // före std::move – en flyttad std::function är tom
    if (action) g_object_set_data_full(G_OBJECT(row), "run", new std::function<void()>(std::move(run)),
                                       +[](gpointer p) { delete static_cast<std::function<void()>*>(p); });
    gtk_list_box_append(GTK_LIST_BOX(list), row);
    if (!action) {   // rubrik: går inte att markera eller välja
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_widget_add_css_class(row, "ws-head-row");
        return GTK_LIST_BOX_ROW(row);
    }
    // musen markerar raden (samma markering som piltangenterna) – aldrig två markerade rader samtidigt
    auto* m = gtk_event_controller_motion_new();
    g_signal_connect(m, "enter", G_CALLBACK(+[](GtkEventControllerMotion* m, double, double, gpointer) {
        auto* r = GTK_LIST_BOX_ROW(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(m)));
        if (auto* l = gtk_widget_get_parent(GTK_WIDGET(r)); l && GTK_IS_LIST_BOX(l)) { clearOtherCurrents(l, r); setCurrent(l, r); }
    }), nullptr);
    gtk_widget_add_controller(row, m);
    return GTK_LIST_BOX_ROW(row);
}

constexpr int kBottomIcon = 26;   // bottenradens ikoner (Program, Internet, Inställningar, fästa, På/Av)
void refreshPins();

// Högerklick eller långt tryck på ett program (träff eller bricka): Fäst i dockan / Ta bort från dockan,
// Avinstallera…  (Avinstallera finns också i Program – högerklick är aldrig enda vägen)
void attachAppMenu(GtkWidget* w, const std::string& desktopFile) {
    if (desktopFile.empty()) return;
    g_object_set_data_full(G_OBJECT(w), "desktop-file", g_strdup(desktopFile.c_str()), g_free);
    {   // dra programmet till bottenraden = fäst det där (eller flytta det bland de fästa)
        auto* ds = gtk_drag_source_new();
        gtk_drag_source_set_actions(ds, GDK_ACTION_COPY);
        g_signal_connect(ds, "prepare", G_CALLBACK(+[](GtkDragSource* src, double, double, gpointer) -> GdkContentProvider* {
            GtkWidget* wid = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(src));
            const char* f = static_cast<const char*>(g_object_get_data(G_OBJECT(wid), "desktop-file"));
            if (!f) return nullptr;
            gchar* base = g_path_get_basename(f);
            const std::string v = std::string("app:") + base;
            g_free(base);
            GdkPaintable* pt = gtk_widget_paintable_new(wid);
            gtk_drag_source_set_icon(src, pt, 0, 0);
            g_object_unref(pt);
            return gdk_content_provider_new_typed(G_TYPE_STRING, v.c_str());
        }), nullptr);
        gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(ds));
    }
    auto handler = +[](GtkGesture* g, int, double x, double y, gpointer) {
        auto* wid = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g));
        gtk_gesture_set_state(g, GTK_EVENT_SEQUENCE_CLAIMED);
        const char* f = static_cast<const char*>(g_object_get_data(G_OBJECT(wid), "desktop-file"));
        if (!f) return;
        gchar* base = g_path_get_basename(f);
        const bool pinned = isPinned(base);
        g_free(base);
        auto* menu = g_menu_new();
        g_menu_append(menu, pinned ? T("Remove from Start", "Ta bort från Start") : T("Pin to Start", "Fäst i Start"), "wsapp.pin");
        g_menu_append(menu, T("Uninstall…", "Avinstallera…"), "wsapp.uninstall");
        auto* pop = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
        g_object_unref(menu);
        auto* grp = g_simple_action_group_new();
        auto* pin = g_simple_action_new("pin", nullptr);
        g_signal_connect(pin, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer r) {
            const char* f = static_cast<const char*>(g_object_get_data(G_OBJECT(r), "desktop-file"));
            if (!f) return;
            gchar* b = g_path_get_basename(f);
            setPinned(b, !isPinned(b));
            g_free(b);
            refreshPins();   // syns direkt i bottenraden
        }), wid);
        auto* un = g_simple_action_new("uninstall", nullptr);
        g_signal_connect(un, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer r) {
            const char* f = static_cast<const char*>(g_object_get_data(G_OBJECT(r), "desktop-file"));
            if (!f) return;
            const std::string file = f;
            shellDone();   // Start stängs, frågan om avinstallation visas
            H.spawn({"/usr/lib/wenuyos/wenuyos-uninstall", file});
        }), wid);
        g_action_map_add_action(G_ACTION_MAP(grp), G_ACTION(pin));
        g_action_map_add_action(G_ACTION_MAP(grp), G_ACTION(un));
        g_object_unref(pin);
        g_object_unref(un);
        gtk_widget_insert_action_group(wid, "wsapp", G_ACTION_GROUP(grp));
        g_object_unref(grp);
        gtk_widget_set_parent(pop, wid);
        GdkRectangle rc = {(int)x, (int)y, 1, 1};
        gtk_popover_set_pointing_to(GTK_POPOVER(pop), &rc);
        g_signal_connect(pop, "closed", G_CALLBACK(+[](GtkPopover* p, gpointer) {
            g_idle_add(+[](gpointer p) -> gboolean { gtk_widget_unparent(GTK_WIDGET(p)); return G_SOURCE_REMOVE; }, p);
        }), nullptr);
        gtk_popover_popup(GTK_POPOVER(pop));
    };
    auto* rc = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rc), GDK_BUTTON_SECONDARY);
    g_signal_connect(rc, "pressed", G_CALLBACK(handler), nullptr);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(rc));
    auto* lp = gtk_gesture_long_press_new();
    g_signal_connect(lp, "pressed", G_CALLBACK(+[](GtkGestureLongPress* g, double x, double y, gpointer h) {
        reinterpret_cast<void (*)(GtkGesture*, int, double, double, gpointer)>(h)(GTK_GESTURE(g), 1, x, y, nullptr);
    }), reinterpret_cast<gpointer>(handler));
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(lp));
}

// Högerklick eller långt tryck på en fil (senaste filer, sökträffar, fästa filer): Fäst i Start / Ta bort från
// Start, Öppna mappen. Filen går också att dra – till mini-terminalen (sökvägen), Filer eller ett program.
void fillFiles();
void attachFileMenu(GtkWidget* w, const std::string& uri) {
    if (uri.empty()) return;
    g_object_set_data_full(G_OBJECT(w), "file-uri", g_strdup(uri.c_str()), g_free);
    {
        auto* ds = gtk_drag_source_new();
        gtk_drag_source_set_actions(ds, GDK_ACTION_COPY);
        g_signal_connect(ds, "prepare", G_CALLBACK(+[](GtkDragSource* src, double, double, gpointer) -> GdkContentProvider* {
            GtkWidget* wid = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(src));
            const char* u = static_cast<const char*>(g_object_get_data(G_OBJECT(wid), "file-uri"));
            if (!u) return nullptr;
            GFile* f = g_file_new_for_uri(u);
            GSList* l = g_slist_append(nullptr, f);
            GdkFileList* fl = gdk_file_list_new_from_list(l);
            g_slist_free(l);
            g_object_unref(f);
            GdkContentProvider* p = gdk_content_provider_new_typed(GDK_TYPE_FILE_LIST, fl);
            g_boxed_free(GDK_TYPE_FILE_LIST, fl);
            GdkPaintable* pt = gtk_widget_paintable_new(wid);
            gtk_drag_source_set_icon(src, pt, 0, 0);
            g_object_unref(pt);
            return p;
        }), nullptr);
        gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(ds));
    }
    auto handler = +[](GtkGesture* g, int, double x, double y, gpointer) {
        auto* wid = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g));
        gtk_gesture_set_state(g, GTK_EVENT_SEQUENCE_CLAIMED);
        const char* u = static_cast<const char*>(g_object_get_data(G_OBJECT(wid), "file-uri"));
        if (!u) return;
        auto* menu = g_menu_new();
        g_menu_append(menu, isStartFile(u) ? T("Remove from Start", "Ta bort från Start") : T("Pin to Start", "Fäst i Start"), "wsfile.pin");
        g_menu_append(menu, T("Open the folder", "Öppna mappen"), "wsfile.folder");
        auto* pop = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
        g_object_unref(menu);
        auto* grp = g_simple_action_group_new();
        auto* pin = g_simple_action_new("pin", nullptr);
        g_signal_connect(pin, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer r) {
            const char* u = static_cast<const char*>(g_object_get_data(G_OBJECT(r), "file-uri"));
            if (!u) return;
            const std::string uri = u;   // raden kan försvinna när listan fylls om
            writeStartFiles(togglePinned(startFiles(), uri, !isStartFile(uri)));
            g_idle_add_once(+[](gpointer) { fillFiles(); }, nullptr);
        }), wid);
        auto* dir = g_simple_action_new("folder", nullptr);
        g_signal_connect(dir, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer r) {
            const char* u = static_cast<const char*>(g_object_get_data(G_OBJECT(r), "file-uri"));
            if (!u) return;
            GFile* f = g_file_new_for_uri(u);
            GFile* parent = g_file_get_parent(f);
            if (parent) {
                gchar* pu = g_file_get_uri(parent);
                shellDone();
                H.spawn({"gio", "open", pu});
                g_free(pu);
                g_object_unref(parent);
            }
            g_object_unref(f);
        }), wid);
        g_action_map_add_action(G_ACTION_MAP(grp), G_ACTION(pin));
        g_action_map_add_action(G_ACTION_MAP(grp), G_ACTION(dir));
        g_object_unref(pin);
        g_object_unref(dir);
        gtk_widget_insert_action_group(wid, "wsfile", G_ACTION_GROUP(grp));
        g_object_unref(grp);
        gtk_widget_set_parent(pop, wid);
        GdkRectangle rc = {(int)x, (int)y, 1, 1};
        gtk_popover_set_pointing_to(GTK_POPOVER(pop), &rc);
        g_signal_connect(pop, "closed", G_CALLBACK(+[](GtkPopover* p, gpointer) {
            g_idle_add(+[](gpointer p) -> gboolean { gtk_widget_unparent(GTK_WIDGET(p)); return G_SOURCE_REMOVE; }, p);
        }), nullptr);
        gtk_popover_popup(GTK_POPOVER(pop));
    };
    auto* rc = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rc), GDK_BUTTON_SECONDARY);
    g_signal_connect(rc, "pressed", G_CALLBACK(handler), nullptr);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(rc));
    auto* lp = gtk_gesture_long_press_new();
    g_signal_connect(lp, "pressed", G_CALLBACK(+[](GtkGestureLongPress* g, double x, double y, gpointer h) {
        reinterpret_cast<void (*)(GtkGesture*, int, double, double, gpointer)>(h)(GTK_GESTURE(g), 1, x, y, nullptr);
    }), reinterpret_cast<gpointer>(handler));
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(lp));
}

void clearList(GtkWidget* list) {
    g_object_set_data(G_OBJECT(list), "current", nullptr);
    while (GtkWidget* c = gtk_widget_get_first_child(list)) gtk_list_box_remove(GTK_LIST_BOX(list), c);
}
GtkWidget* sectionHead(const char* text) {
    auto* l = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_widget_add_css_class(l, "ws-head");
    return l;
}
void fillResults(GtkWidget* list, const std::string& q) {
    clearList(list);
    int last = -1;
    GtkListBoxRow *likely = nullptr, *first = nullptr;
    for (auto& r : search(q)) {
        if (r.section != last && (r.section == SecWindows || r.section == SecApps || r.section == SecSettings || r.section == SecFiles || r.section == SecPages)) {
            const char* h = r.section == SecWindows ? T("Open windows", "Öppna fönster") : r.section == SecApps ? T("Programs", "Program") : r.section == SecSettings ? T("Settings", "Inställningar")
                          : r.section == SecPages ? T("Web pages", "Webbsidor") : T("Files and folders", "Filer och mappar");
            addAction(list, sectionHead(h), nullptr);
        }
        if (r.section == SecCalc && last != SecCalc && last > 0) addAction(list, sectionHead(""), nullptr);   // lite luft
        last = r.section;
        auto* row = addAction(list, menuRow(r.icon, r.title, r.subtitle, false, 28), r.run);
        if (r.keep) g_object_set_data(G_OBJECT(row), "keep", GINT_TO_POINTER(1));
        if (!r.key.empty()) g_object_set_data_full(G_OBJECT(row), "key", g_strdup(r.key.c_str()), g_free);
        attachAppMenu(GTK_WIDGET(row), r.desktop);
        attachFileMenu(GTK_WIDGET(row), r.file);
        if (!first) first = row;
        if (r.likely && !likely) likely = row;
    }
    if (GtkListBoxRow* sel = likely ? likely : first) setCurrent(list, sel);
}

// Piltangenter i sökrutan flyttar markeringen i listan, Enter kör den markerade
void moveSelection(GtkListBox* list, int dir) {
    GtkListBoxRow* sel = currentRow(GTK_WIDGET(list));
    int i = sel ? gtk_list_box_row_get_index(sel) : -1;
    for (int n = i + dir; n >= 0; n += dir) {
        GtkListBoxRow* r = gtk_list_box_get_row_at_index(list, n);
        if (!r) return;
        if (gtk_list_box_row_get_selectable(r)) {
            setCurrent(GTK_WIDGET(list), r);
            // rulla så att raden syns (fokus stannar i sökfältet)
            if (GtkWidget* sc = gtk_widget_get_ancestor(GTK_WIDGET(list), GTK_TYPE_SCROLLED_WINDOW)) {
                graphene_rect_t b;
                if (gtk_widget_compute_bounds(GTK_WIDGET(r), GTK_WIDGET(list), &b)) {
                    GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sc));
                    const double top = gtk_adjustment_get_value(adj), page = gtk_adjustment_get_page_size(adj);
                    if (b.origin.y < top) gtk_adjustment_set_value(adj, b.origin.y);
                    else if (b.origin.y + b.size.height > top + page) gtk_adjustment_set_value(adj, b.origin.y + b.size.height - page);
                }
            }
            return;
        }
    }
}

// ------------------------------------------------------------------ Start (0.3.18): en enda yta
// Sökfältet överst (markören står där), under det appnätet i kategorier och senaste filer, längst ned
// Filer · Inställningar · Lås · Logga ut · Stäng av. Skriver man byts nätet mot förslag. I surfplatteläget
// samma yta i helskärm med större brickor. Panelen lägger kortet i ett fönster ovanför dockan.
struct StartUi {
    GtkWidget *card = nullptr, *entry = nullptr, *stack = nullptr, *grid = nullptr, *results = nullptr;
    GtkWidget *aiQuestion = nullptr, *aiText = nullptr, *aiCmds = nullptr, *aiStop = nullptr, *aiStatus = nullptr;
    GSubprocess* aiProc = nullptr;
    GCancellable* aiCancel = nullptr;
    std::string aiBuf, aiPending;
    bool aiConversation = false;   // nästa fråga fortsätter samtalet (tills "Ny fråga" eller Start stängs länge)
    GtkWidget *recent = nullptr, *recentBox = nullptr, *files = nullptr, *filesBox = nullptr;
    GtkWidget *side = nullptr, *sideSlot = nullptr;   // mini-terminalen (från skalet), till vänster
    GtkWidget *windows = nullptr, *windowsBox = nullptr, *pins = nullptr, *pinsBox = nullptr, *appsBtn = nullptr;
    GtkListBoxRow* homeCur = nullptr;   // markerad rad i den tomma vyn (piltangenterna)
    GtkWidget* bottom = nullptr;        // bottenraden (döljs när terminalen fyller Start)
    GtkWidget *often = nullptr, *oftenBox = nullptr;   // ofta använda program
    int width = -1;                     // kortets bredd från skalet (lika brett som fältets sökfält), -1 = förval
    bool expanded = false;              // mini-terminalen fyller hela Start
    bool tablet = false, dirty = true;
} M;

// Programkategorier, i den här ordningen. WenuyOS egna poster styrs med X-WenuyOS-*: Command = bara i
// sökningen (Lås, Logga ut … finns på bottenraden), Install = Lägg till program (flyttar till Program),
// Android, SystemTool = Systemverktyg.
struct Cat { const char* key; const char* en; const char* sv; };
const Cat kCats[] = {
    {"Accessories", "Accessories", "Tillbehör"},
    {"Network", "Internet", "Internet"},
    {"Office", "Office", "Kontor"},
    {"Graphics", "Graphics", "Grafik"},
    {"AudioVideo", "Sound and video", "Ljud och video"},
    {"Game", "Games", "Spel"},
    {"Education", "Education", "Utbildning"},
    {"Development", "Development", "Utveckling"},
    {"Android", "Android", "Android"},
    {"System", "System tools", "Systemverktyg"},
    {"Install", "Add programs", "Lägg till program"},
};
std::string categoryOf(GAppInfo* a) {
    const char* cats = G_IS_DESKTOP_APP_INFO(a) ? g_desktop_app_info_get_categories(G_DESKTOP_APP_INFO(a)) : nullptr;
    const std::string c = cats ? std::string(";") + cats : ";";
    auto has = [&](const char* k) { return c.find(std::string(";") + k + ";") != std::string::npos; };
    if (has("X-WenuyOS-Command")) return "";            // bara i sökningen
    if (has("X-WenuyOS-Install")) return "Install";
    if (has("X-WenuyOS-Android") || has("X-WayDroid-App")) return "Android";
    if (has("X-WenuyOS-SystemTool")) return "System";
    if (has("TerminalEmulator") || has("FileManager")) return "Accessories";   // Konsol och Filer under Tillbehör
    if (has("Office")) return "Office";
    if (has("Network") || has("WebBrowser") || has("Email")) return "Network";
    if (has("Graphics")) return "Graphics";
    if (has("AudioVideo") || has("Audio") || has("Video")) return "AudioVideo";
    if (has("Game")) return "Game";
    if (has("Development")) return "Development";
    if (has("Utility") || has("Accessories")) return "Accessories";
    if (has("Education") || has("Science")) return "Education";
    if (has("System") || has("Settings")) return "System";
    return "Accessories";
}

GtkWidget* appTile(GAppInfo* a) {
    const int size = M.tablet ? 56 : 36;
    auto* b = gtk_button_new();
    gtk_widget_add_css_class(b, "ws-tile");
    auto* v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* img = g_app_info_get_icon(a) ? gtk_image_new_from_gicon(g_app_info_get_icon(a)) : gtk_image_new_from_icon_name("application-x-executable");
    gtk_image_set_pixel_size(GTK_IMAGE(img), size);
    auto* l = gtk_label_new(g_app_info_get_name(a));
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(l), M.tablet ? 12 : 11);
    gtk_label_set_width_chars(GTK_LABEL(l), M.tablet ? 12 : 11);
    gtk_label_set_justify(GTK_LABEL(l), GTK_JUSTIFY_CENTER);
    gtk_box_append(GTK_BOX(v), img);
    gtk_box_append(GTK_BOX(v), l);
    gtk_button_set_child(GTK_BUTTON(b), v);
    gtk_widget_set_tooltip_text(b, g_app_info_get_description(a) ? g_app_info_get_description(a) : g_app_info_get_name(a));
    // X-WenuyOS-MinRAM=4096: avancerade program som behöver mer än datorn har får ett märke
    static const int ramMb = [] {
        gchar* m = nullptr;
        int mb = 0;
        if (g_file_get_contents("/proc/meminfo", &m, nullptr, nullptr)) { if (const char* p = strstr(m, "MemTotal:")) mb = atoi(p + 9) / 1024; g_free(m); }
        return mb;
    }();
    if (G_IS_DESKTOP_APP_INFO(a))
        if (gchar* mr = g_desktop_app_info_get_string(G_DESKTOP_APP_INFO(a), "X-WenuyOS-MinRAM")) {
            const int need = atoi(mr);
            g_free(mr);
            if (need > 0 && ramMb > 0 && ramMb < need - 200) {
                const std::string txt = std::string(T("Needs ", "Kräver ")) + std::to_string((need + 512) / 1024) + " GB";
                auto* badge = gtk_label_new(txt.c_str());
                gtk_widget_add_css_class(badge, "ws-ram");
                gtk_box_append(GTK_BOX(v), badge);
            }
        }
    g_object_set_data_full(G_OBJECT(b), "app", g_object_ref(a), g_object_unref);
    g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
        GAppInfo* info = G_APP_INFO(g_object_ref(g_object_get_data(G_OBJECT(btn), "app")));
        shellDone();
        H.launch(info);
        g_object_unref(info);
    }), nullptr);
    if (G_IS_DESKTOP_APP_INFO(a))
        if (const char* df = g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(a))) attachAppMenu(b, df);
    return b;
}

// Appnätet byggs bara om när program installerats/tagits bort eller läget ändrats
void buildGrid() {
    if (!M.dirty) return;
    M.dirty = false;
    while (GtkWidget* c = gtk_widget_get_first_child(M.grid)) gtk_box_remove(GTK_BOX(M.grid), c);
    const bool sdk = sdkInstalled();
    std::map<std::string, std::vector<GAppInfo*>> byCat;
    for (GAppInfo* a : H.apps()) {
        if (!sdk && needsSdk(a)) continue;
        const std::string c = categoryOf(a);
        if (!c.empty()) byCat[c].push_back(a);
    }
    for (auto& c : kCats) {
        auto it = byCat.find(c.key);
        if (it == byCat.end() || it->second.empty()) continue;
        auto* h = sectionHead(T(c.en, c.sv));
        gtk_widget_add_css_class(h, "ws-cat");
        gtk_box_append(GTK_BOX(M.grid), h);
        auto* flow = gtk_flow_box_new();
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
        gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), M.tablet ? 4 : M.side ? 2 : 5);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), M.tablet ? 10 : 5);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 2);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 2);
        for (GAppInfo* a : it->second) gtk_flow_box_append(GTK_FLOW_BOX(flow), appTile(a));
        gtk_box_append(GTK_BOX(M.grid), flow);
    }
}

// Senaste filer: bara ikon och namn, bara sådant som öppnats i WenuyOS egna program
void fillRecent() {
    while (GtkWidget* c = gtk_widget_get_first_child(M.recent)) gtk_list_box_remove(GTK_LIST_BOX(M.recent), c);
    int n = 0;
    if (recentEnabled()) {
        GList* items = gtk_recent_manager_get_items(gtk_recent_manager_get_default());
        items = g_list_sort(items, +[](gconstpointer a, gconstpointer b) -> int {
            GDateTime* ta = gtk_recent_info_get_modified((GtkRecentInfo*)a);
            GDateTime* tb = gtk_recent_info_get_modified((GtkRecentInfo*)b);
            return ta && tb ? g_date_time_compare(tb, ta) : 0;
        });
        for (GList* l = items; l && n < (M.tablet ? 12 : 9); l = l->next) {
            auto* it = static_cast<GtkRecentInfo*>(l->data);
            if (!gtk_recent_info_has_application(it, "wenuyos")) continue;
            const std::string uri = gtk_recent_info_get_uri(it);
            if (gtk_recent_info_is_local(it) && !gtk_recent_info_exists(it)) continue;
            gchar* icon = nullptr;
            if (GIcon* gi = gtk_recent_info_get_gicon(it)) { icon = g_icon_to_string(gi); g_object_unref(gi); }
            auto* row = addAction(M.recent, menuRow(icon ? icon : "text-x-generic", gtk_recent_info_get_display_name(it), "", false, 20),
                                  [uri] { H.spawn({"gio", "open", uri}); });
            attachFileMenu(GTK_WIDGET(row), uri);
            g_free(icon);
            ++n;
        }
        g_list_free_full(items, (GDestroyNotify)gtk_recent_info_unref);
    }
    gtk_widget_set_visible(M.recentBox, n > 0);
}

// Fästa filer överst i Start (högerklick på en fil → Fäst i Start, eller dra filen hit). En fil som inte finns
// just nu (t.ex. på ett minneskort som inte sitter i) står kvar, nedtonad.
void fillFiles() {
    if (!M.files) return;
    clearList(M.files);
    int n = 0;
    const std::string home = g_get_home_dir();
    for (const std::string& uri : startFiles()) {
        gchar* path = g_filename_from_uri(uri.c_str(), nullptr, nullptr);
        const bool local = path != nullptr;
        const bool exists = !local || g_file_test(path, G_FILE_TEST_EXISTS);
        const bool dir = local && g_file_test(path, G_FILE_TEST_IS_DIR);
        GFile* f = g_file_new_for_uri(uri.c_str());
        gchar* base = g_file_get_basename(f);
        std::string where;
        if (GFile* parent = g_file_get_parent(f)) {
            gchar* pp = g_file_get_parse_name(parent);
            where = pp ? pp : "";
            g_free(pp);
            g_object_unref(parent);
        }
        if (where.rfind(home, 0) == 0) where = "~" + where.substr(home.size());
        if (!exists) where = T("not available right now", "finns inte just nu");
        std::string icon = dir ? "folder" : "text-x-generic";
        if (!dir && base) {
            gchar* ct = g_content_type_guess(base, nullptr, 0, nullptr);
            if (GIcon* gi = g_content_type_get_icon(ct)) { gchar* s2 = g_icon_to_string(gi); if (s2) icon = s2; g_free(s2); g_object_unref(gi); }
            g_free(ct);
        }
        auto* row = addAction(M.files, menuRow(icon, base ? base : uri, where, false, 20), [uri] { H.spawn({"gio", "open", uri}); });
        if (!exists) gtk_widget_add_css_class(gtk_list_box_row_get_child(row), "ws-missing");   // inte raden: menyn är dess barn
        attachFileMenu(GTK_WIDGET(row), uri);
        g_free(base);
        g_object_unref(f);
        g_free(path);
        ++n;
    }
    gtk_widget_set_visible(M.filesBox, n > 0);
}

// Öppna fönster överst i Start (det finns ingen aktivitetsrad – Alt+Tab, sökfältet eller här)
void fillWindows() {
    clearList(M.windows);
    int n = 0;
    for (auto& w : openWindows()) {
        GIcon* gi = wenuy_app_icon_for_id(w.appId.c_str());
        gchar* icon = gi ? g_icon_to_string(gi) : nullptr;
        if (gi) g_object_unref(gi);
        const std::string id = w.id;
        addAction(M.windows, menuRow(icon ? icon : "application-x-executable", w.title, "", false, 20),
                  [id] { if (gActivate) gActivate(id.c_str(), gWindowsData); });
        g_free(icon);
        if (++n == (M.tablet ? 8 : 6)) break;
    }
    gtk_widget_set_visible(M.windowsBox, n > 0);
}
// Ofta använda: högst 6 program (antal gånger × hur nyligen), som ikoner med namnet som tips
void fillOften() {
    while (GtkWidget* c = gtk_widget_get_first_child(M.often)) gtk_box_remove(GTK_BOX(M.often), c);
    int n = 0;
    if (recentEnabled())
        for (const std::string& key : topUsed(loadUsage(), nowSec(), 12)) {
            if (key.rfind("app:", 0) != 0) continue;
            GDesktopAppInfo* d = g_desktop_app_info_new(key.c_str() + 4);
            if (!d) continue;
            if (!g_app_info_should_show(G_APP_INFO(d))) { g_object_unref(d); continue; }
            auto* b = gtk_button_new();
            GtkWidget* img = g_app_info_get_icon(G_APP_INFO(d)) ? gtk_image_new_from_gicon(g_app_info_get_icon(G_APP_INFO(d)))
                                                                : gtk_image_new_from_icon_name("application-x-executable");
            gtk_image_set_pixel_size(GTK_IMAGE(img), 32);
            gtk_button_set_child(GTK_BUTTON(b), img);
            gtk_widget_add_css_class(b, "ws-often");
            gtk_widget_set_tooltip_text(b, g_app_info_get_name(G_APP_INFO(d)));
            g_object_set_data_full(G_OBJECT(b), "app", d, g_object_unref);
            g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
                GAppInfo* info = G_APP_INFO(g_object_ref(g_object_get_data(G_OBJECT(btn), "app")));
                shellDone();
                H.launch(info);
                g_object_unref(info);
            }), nullptr);
            if (const char* df = g_desktop_app_info_get_filename(d)) attachAppMenu(b, df);
            gtk_box_append(GTK_BOX(M.often), b);
            if (++n == (M.tablet ? 8 : 6)) break;
        }
    gtk_widget_set_visible(M.oftenBox, n > 0);
}

// Fästa program (högerklick på ett program → Fäst i Start) som stora ikoner i bottenraden
void fillPins();
void refreshPins() { fillPins(); }
void fillPins() {
    while (GtkWidget* c = gtk_widget_get_first_child(M.pins)) gtk_box_remove(GTK_BOX(M.pins), c);
    int n = 0;
    std::vector<GAppInfo*> pinned;   // i den ordning de fästes
    for (const std::string& id : dockPins())
        for (GAppInfo* a : H.apps())
            if (G_IS_DESKTOP_APP_INFO(a) && g_app_info_get_id(a) && id == g_app_info_get_id(a)) { pinned.push_back(a); break; }
    for (GAppInfo* a : pinned) {
        auto* b = gtk_button_new();
        GtkWidget* img = g_app_info_get_icon(a) ? gtk_image_new_from_gicon(g_app_info_get_icon(a)) : gtk_image_new_from_icon_name("application-x-executable");
        gtk_image_set_pixel_size(GTK_IMAGE(img), kBottomIcon);
        gtk_button_set_child(GTK_BUTTON(b), img);
        gtk_widget_add_css_class(b, "ws-bottom-btn");
        // namnet och kortkommandot (Super+1 … Super+9) som tips
        const std::string tip = std::string(g_app_info_get_name(a)) + (n < 9 ? "  (Super+" + std::to_string(n + 1) + ")" : "");
        gtk_widget_set_tooltip_text(b, tip.c_str());
        g_object_set_data_full(G_OBJECT(b), "app", g_object_ref(a), g_object_unref);
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
            GAppInfo* info = G_APP_INFO(g_object_ref(g_object_get_data(G_OBJECT(btn), "app")));
            shellDone();
            H.launch(info);
            g_object_unref(info);
        }), nullptr);
        if (const char* df = g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(a))) attachAppMenu(b, df);
        gtk_box_append(GTK_BOX(M.pins), b);
        ++n;
    }
    if (!n) {
        auto* l = gtk_label_new(T("Drag a program here, or right click → Pin to Start", "Dra hit ett program, eller högerklicka → Fäst i Start"));
        gtk_widget_add_css_class(l, "ws-pin-hint");
        gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_widget_set_margin_start(l, 6);
        gtk_box_append(GTK_BOX(M.pins), l);
    }
}

GtkWidget* bottomButton(const char* icon, const char* label, std::function<void()> fn) {
    auto* b = gtk_button_new();   // stor ikon, texten som tips
    gtk_button_set_child(GTK_BUTTON(b), iconFor(std::string("w:") + icon, kBottomIcon));
    if (label) gtk_widget_set_tooltip_text(b, label);
    gtk_widget_add_css_class(b, "ws-bottom-btn");
    auto* f = new std::function<void()>(std::move(fn));
    g_signal_connect_data(b, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto copy = *static_cast<std::function<void()>*>(d);
        shellDone();
        copy();
    }), f, +[](gpointer d, GClosure*) { delete static_cast<std::function<void()>*>(d); }, (GConnectFlags)0);
    return b;
}

void showQuery() {
    const std::string q = gtk_editable_get_text(GTK_EDITABLE(M.entry));
    const bool searching = !q.empty() && q.find_first_not_of(' ') != std::string::npos;
    gLastQuery = q;
    const char* cur = gtk_stack_get_visible_child_name(GTK_STACK(M.stack));
    if (searching && M.expanded) wenuy_start_menu_set_side_expanded(FALSE);   // sökning: förslagen syns igen
    if (searching) gtk_stack_set_visible_child_name(GTK_STACK(M.stack), "results");
    else if (!g_strcmp0(cur, "results")) {
        const bool homeEmpty = !gtk_widget_get_visible(M.oftenBox) && !gtk_widget_get_visible(M.windowsBox) && !gtk_widget_get_visible(M.recentBox) && !gtk_widget_get_visible(M.filesBox);
        gtk_stack_set_visible_child_name(GTK_STACK(M.stack), homeEmpty ? "apps" : "home");
    }
    if (searching) fillResults(M.results, q);   // efter bytet: markeringen ska synas
    if (M.appsBtn) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(M.appsBtn), !g_strcmp0(gtk_stack_get_visible_child_name(GTK_STACK(M.stack)), "apps"));
}
// En rad ikoner som aldrig gör Start bredare: det som inte får plats klipps bort (Start är alltid lika bred som
// fältets sökfält – förut växte kortet när många program var fästa eller ofta använda).
// ---- AI-svaret i Start: frågan, svaret medan det kommer, kommandon ur svaret som knappar ----
void aiSetRunning(bool on) {   // medan svaret kommer: Stoppa i stället för Kopiera (raden får plats i halva Start)
    gtk_widget_set_visible(M.aiStop, on);
    gtk_widget_set_visible(M.aiStatus, !on);
}
// Svaret som text: kodblock i fast teckenbredd (utan ```), **fetstil**, rubriker (#) fetstilta, "- " som •
void aiRender() {
    GtkTextBuffer* b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(M.aiText));
    GtkTextTagTable* tt = gtk_text_buffer_get_tag_table(b);
    if (!gtk_text_tag_table_lookup(tt, "code")) {
        gtk_text_buffer_create_tag(b, "code", "family", "monospace", "paragraph-background", "#1d1d1d", "foreground", "#e6e6e6",
                                   "left-margin", 8, "right-margin", 4, nullptr);
        gtk_text_buffer_create_tag(b, "bold", "weight", PANGO_WEIGHT_BOLD, nullptr);
    }
    gtk_text_buffer_set_text(b, "", 0);
    GtkTextIter end;
    bool code = false;
    size_t a = 0;
    const std::string& t = M.aiBuf;
    while (a <= t.size()) {
        size_t nl = t.find('\n', a);
        const bool last = nl == std::string::npos;
        std::string line = t.substr(a, last ? std::string::npos : nl - a);
        const size_t ind = line.find_first_not_of(" \t");
        if (ind != std::string::npos && line.compare(ind, 3, "```") == 0) { code = !code; if (last) break; a = nl + 1; continue; }
        gtk_text_buffer_get_end_iter(b, &end);
        if (code) gtk_text_buffer_insert_with_tags_by_name(b, &end, line.c_str(), -1, "code", nullptr);
        else {
            bool head = false;
            if (ind != std::string::npos && line[ind] == '#') { line = line.substr(line.find_first_not_of("# ", ind)); head = true; }
            else if (ind != std::string::npos && (line.compare(ind, 2, "- ") == 0 || line.compare(ind, 2, "* ") == 0)) line = line.substr(0, ind) + "• " + line.substr(ind + 2);
            size_t p = 0;   // **fetstil**
            bool bold = head;
            while (p <= line.size()) {
                size_t q = line.find("**", p);
                const std::string part = line.substr(p, q == std::string::npos ? std::string::npos : q - p);
                gtk_text_buffer_get_end_iter(b, &end);
                if (bold) gtk_text_buffer_insert_with_tags_by_name(b, &end, part.c_str(), -1, "bold", nullptr);
                else gtk_text_buffer_insert(b, &end, part.c_str(), -1);
                if (q == std::string::npos) break;
                if (!head) bold = !bold;
                p = q + 2;
            }
        }
        if (last) break;
        gtk_text_buffer_get_end_iter(b, &end);
        gtk_text_buffer_insert(b, &end, "\n", 1);
        a = nl + 1;
    }
}
void aiAppend(const std::string& t) {
    M.aiBuf += t;
    aiRender();
    GtkTextBuffer* b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(M.aiText));   // följ med nedåt medan svaret kommer
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(b, &end);
    gtk_text_buffer_place_cursor(b, &end);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(M.aiText), gtk_text_buffer_get_insert(b));
}
void aiFinished() {
    if (M.aiProc) { g_object_unref(M.aiProc); M.aiProc = nullptr; }
    aiSetRunning(false);
    while (GtkWidget* c = gtk_widget_get_first_child(M.aiCmds)) gtk_box_remove(GTK_BOX(M.aiCmds), c);
    for (const std::string& cmd : aiCommands(M.aiBuf)) {   // "Kör i terminalen" – aldrig av sig själv
        auto* b = gtk_button_new();
        auto* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_append(GTK_BOX(row), iconFor("w:terminal", 18));
        auto* l = gtk_label_new(cmd.c_str());
        gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_widget_add_css_class(l, "ws-ai-cmd");
        gtk_box_append(GTK_BOX(row), l);
        gtk_button_set_child(GTK_BUTTON(b), row);
        gtk_widget_set_tooltip_text(b, T("Run in the terminal (you see it before Enter)", "Skriv i terminalen (du ser det innan Enter)"));
        g_object_set_data_full(G_OBJECT(b), "cmd", g_strdup(cmd.c_str()), g_free);
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer) {
            const char* c = static_cast<const char*>(g_object_get_data(G_OBJECT(b), "cmd"));
            if (c && gTypeFunc) gTypeFunc(c, gCommandData);
        }), nullptr);
        gtk_box_append(GTK_BOX(M.aiCmds), b);
    }
    gtk_widget_set_visible(M.aiCmds, gtk_widget_get_first_child(M.aiCmds) != nullptr && gTypeFunc != nullptr);
}
void aiRead(GObject* src, GAsyncResult* res, gpointer) {
    GError* err = nullptr;
    GBytes* bytes = g_input_stream_read_bytes_finish(G_INPUT_STREAM(src), res, &err);
    if (!bytes) {   // avbrutet (Stoppa) eller fel
        if (err && !g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) aiAppend(std::string("\n⚠ ") + err->message);
        if (err) g_error_free(err);
        aiFinished();
        return;
    }
    gsize n = 0;
    const char* d = static_cast<const char*>(g_bytes_get_data(bytes, &n));
    if (n == 0) {   // klart
        g_bytes_unref(bytes);
        if (!M.aiPending.empty()) { aiAppend(M.aiPending); M.aiPending.clear(); }
        aiFinished();
        return;
    }
    M.aiPending.append(d, n);
    g_bytes_unref(bytes);
    const size_t ok = utf8Complete(M.aiPending);   // bara hela tecken (å, ä, ö kan delas mellan bitarna)
    if (ok) { aiAppend(M.aiPending.substr(0, ok)); M.aiPending.erase(0, ok); }
    g_input_stream_read_bytes_async(G_INPUT_STREAM(src), 4096, G_PRIORITY_DEFAULT, M.aiCancel, aiRead, nullptr);
}
void aiStop() {
    if (M.aiCancel) g_cancellable_cancel(M.aiCancel);
    if (M.aiProc) g_subprocess_force_exit(M.aiProc);
}
void askAI(const std::string& q) {
    aiStop();
    if (M.aiCancel) g_object_unref(M.aiCancel);
    M.aiCancel = g_cancellable_new();
    M.aiBuf.clear();
    M.aiPending.clear();
    gtk_label_set_text(GTK_LABEL(M.aiQuestion), q.c_str());
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(M.aiText)), "", 0);
    while (GtkWidget* c = gtk_widget_get_first_child(M.aiCmds)) gtk_box_remove(GTK_BOX(M.aiCmds), c);
    gtk_widget_set_visible(M.aiCmds, FALSE);
    gtk_stack_set_visible_child_name(GTK_STACK(M.stack), "ai");
    gtk_editable_set_text(GTK_EDITABLE(M.entry), "");   // fältet är fritt för en följdfråga
    GError* err = nullptr;
    M.aiProc = g_subprocess_new(GSubprocessFlags(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE), &err,
                                "/usr/lib/wenuyos/wenuyos-ai-chat", "ask", M.aiConversation ? "--continue" : "--new", q.c_str(), nullptr);
    if (!M.aiProc) {
        aiAppend(std::string("⚠ ") + (err ? err->message : "wenuyos-ai-chat"));
        if (err) g_error_free(err);
        return;
    }
    M.aiConversation = true;
    aiSetRunning(true);
    g_input_stream_read_bytes_async(g_subprocess_get_stdout_pipe(M.aiProc), 4096, G_PRIORITY_DEFAULT, M.aiCancel, aiRead, nullptr);
}
GtkWidget* buildAiPage() {
    auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(box, "ws-ai");
    auto* head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    M.aiQuestion = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(M.aiQuestion), TRUE);
    gtk_label_set_xalign(GTK_LABEL(M.aiQuestion), 0);
    gtk_label_set_selectable(GTK_LABEL(M.aiQuestion), TRUE);
    gtk_widget_add_css_class(M.aiQuestion, "ws-ai-q");
    gtk_widget_set_hexpand(M.aiQuestion, TRUE);
    gtk_box_append(GTK_BOX(head), M.aiQuestion);
    gtk_box_append(GTK_BOX(box), head);
    M.aiText = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(M.aiText), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(M.aiText), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(M.aiText), GTK_WRAP_WORD_CHAR);
    gtk_widget_add_css_class(M.aiText, "ws-ai-text");
    auto* sc = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sc), M.aiText);
    gtk_widget_set_vexpand(sc, TRUE);
    gtk_box_append(GTK_BOX(box), sc);
    M.aiCmds = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_visible(M.aiCmds, FALSE);
    gtk_box_append(GTK_BOX(box), M.aiCmds);
    auto* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(row, GTK_ALIGN_END);
    M.aiStop = gtk_button_new_with_label(T("Stop", "Stoppa"));
    gtk_widget_add_css_class(M.aiStop, "ws-ai-btn");
    g_signal_connect(M.aiStop, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) { aiStop(); }), nullptr);
    gtk_widget_set_visible(M.aiStop, FALSE);
    gtk_box_append(GTK_BOX(row), M.aiStop);
    auto* copy = M.aiStatus = gtk_button_new_with_label(T("Copy", "Kopiera"));
    gtk_widget_add_css_class(copy, "ws-ai-btn");
    g_signal_connect(copy, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        gdk_clipboard_set_text(gdk_display_get_clipboard(gdk_display_get_default()), M.aiBuf.c_str());
    }), nullptr);
    gtk_box_append(GTK_BOX(row), copy);
    auto* fresh = gtk_button_new_with_label(T("New question", "Ny fråga"));
    gtk_widget_add_css_class(fresh, "ws-ai-btn");
    gtk_widget_set_tooltip_text(fresh, T("Starts over – the AI forgets this conversation", "Börjar om – AI:n glömmer det här samtalet"));
    g_signal_connect(fresh, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        aiStop();
        M.aiConversation = false;
        H.spawn({"/usr/lib/wenuyos/wenuyos-ai-chat", "forget"});
        gtk_stack_set_visible_child_name(GTK_STACK(M.stack), "home");
        gtk_widget_grab_focus(M.entry);
    }), nullptr);
    gtk_box_append(GTK_BOX(row), fresh);
    gtk_box_append(GTK_BOX(box), row);
    return box;
}
GtkWidget* clipRow(GtkWidget* child) {
    auto* sc = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_EXTERNAL, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(sc), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sc), child);
    gtk_widget_set_hexpand(sc, TRUE);
    return sc;
}
GtkWidget* scrolled(GtkWidget* child) {
    auto* sc = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sc), child);
    gtk_widget_set_vexpand(sc, TRUE);
    return sc;
}

// Bara en markerad rad i hela Start: musen över en rad i en lista tar bort markeringen i de andra (förut
// kunde både ett öppet fönster och en senaste fil vara markerade samtidigt)
void clearOtherCurrents(GtkWidget* keep, GtkListBoxRow* row) {
    for (GtkWidget* list : {M.files, M.windows, M.recent, M.results}) {
        if (!list || list == keep) continue;
        if (auto* cur = currentRow(list)) gtk_widget_remove_css_class(GTK_WIDGET(cur), "ws-current");
        g_object_set_data(G_OBJECT(list), "current", nullptr);
        gtk_list_box_unselect_all(GTK_LIST_BOX(list));
    }
    if (keep != M.results) M.homeCur = row;   // Enter öppnar raden musen står på
}
// Piltangenterna i den tomma vyn: öppna fönster, sedan senaste filer, som en lista
void moveHome(int dir) {
    std::vector<GtkListBoxRow*> rows;
    for (GtkWidget* list : {M.files, M.windows, M.recent}) {
        if (!gtk_widget_get_visible(gtk_widget_get_parent(list))) continue;
        for (int i = 0;; ++i) {
            GtkListBoxRow* r = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list), i);
            if (!r) break;
            if (gtk_list_box_row_get_selectable(r)) rows.push_back(r);
        }
    }
    if (rows.empty()) return;
    int i = -1;
    for (size_t k = 0; k < rows.size(); ++k) if (rows[k] == M.homeCur) i = (int)k;
    i = i < 0 ? (dir > 0 ? 0 : (int)rows.size() - 1) : std::clamp(i + dir, 0, (int)rows.size() - 1);
    for (GtkWidget* list : {M.files, M.windows, M.recent})
        if (auto* cur = currentRow(list); cur && cur != rows[i]) { gtk_widget_remove_css_class(GTK_WIDGET(cur), "ws-current"); g_object_set_data(G_OBJECT(list), "current", nullptr); }
    M.homeCur = rows[i];
    setCurrent(gtk_widget_get_parent(GTK_WIDGET(rows[i])), rows[i]);
    if (GtkWidget* sc = gtk_widget_get_ancestor(GTK_WIDGET(rows[i]), GTK_TYPE_SCROLLED_WINDOW)) {
        graphene_rect_t b;
        GtkWidget* child = gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(sc));
        if (gtk_widget_compute_bounds(GTK_WIDGET(rows[i]), child, &b)) {
            GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sc));
            const double top = gtk_adjustment_get_value(adj), page = gtk_adjustment_get_page_size(adj);
            if (b.origin.y < top) gtk_adjustment_set_value(adj, b.origin.y);
            else if (b.origin.y + b.size.height > top + page) gtk_adjustment_set_value(adj, b.origin.y + b.size.height - page);
        }
    }
}

// Start (0.4.0): kortet ovanför sökfältet i fältet. Till vänster mini-terminalen (om skalet har en), till
// höger öppna fönster, fästa program och senaste filer – eller förslagen när man skriver. Under det
// Program/Filer · Internet · Inställningar · På/Av, och sist sökfältet, där markören står.
void buildStart() {
    M.card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(M.card, "ws-start");
    gtk_widget_set_halign(M.card, GTK_ALIGN_START);
    gtk_widget_set_valign(M.card, GTK_ALIGN_END);
    // allt utom sökfältet ligger i en inre ruta med luft runt; sökfältet längst ned går kant i kant med kortet
    // (i datorläget ligger det exakt över fältets sökfält och ser likadant ut)
    auto* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(inner, "ws-inner");
    gtk_widget_set_vexpand(inner, TRUE);
    gtk_box_append(GTK_BOX(M.card), inner);
    auto* body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(body, TRUE);
    // förslagen och mini-terminalen får hälften var (0.4.0, Daniels önskemål); förstorad terminal: bara den
    gtk_box_set_homogeneous(GTK_BOX(body), TRUE);
    M.sideSlot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(M.sideSlot, "ws-side");
    gtk_widget_set_visible(M.sideSlot, FALSE);
    M.stack = gtk_stack_new();
    gtk_widget_set_hexpand(M.stack, TRUE);
    gtk_widget_set_vexpand(M.stack, TRUE);
    // tom vy: öppna fönster, fästa program, senaste filer
    auto* home = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    // fästa filer (dokument, bilder …) överst
    M.filesBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(M.filesBox), sectionHead(T("Pinned files", "Fästa filer")));
    M.files = actionList();
    gtk_box_append(GTK_BOX(M.filesBox), M.files);
    gtk_widget_set_visible(M.filesBox, FALSE);
    gtk_box_append(GTK_BOX(home), M.filesBox);
    {   // släpp filer här (från Filer, skrivbordet, senaste filer): de fästs i Start
        auto* dt = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GdkDragAction(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
        g_signal_connect(dt, "drop", G_CALLBACK(+[](GtkDropTarget*, const GValue* v, double, double, gpointer) -> gboolean {
            auto list = startFiles();
            int n = 0;
            for (GSList* l = static_cast<GSList*>(g_value_get_boxed(v)); l; l = l->next, ++n) {
                gchar* u = g_file_get_uri(G_FILE(l->data));
                if (u) list = togglePinned(list, u, true);
                g_free(u);
            }
            if (!n) return FALSE;
            writeStartFiles(list);
            fillFiles();
            return TRUE;
        }), nullptr);
        gtk_widget_add_controller(home, GTK_EVENT_CONTROLLER(dt));
    }
    // ofta använda: de program man öppnar mest, som stora ikoner överst
    M.oftenBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(M.oftenBox), sectionHead(T("Often used", "Ofta använda")));
    M.often = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_margin_start(M.often, 6);
    gtk_box_append(GTK_BOX(M.oftenBox), clipRow(M.often));
    gtk_box_append(GTK_BOX(home), M.oftenBox);
    M.windowsBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(M.windowsBox), sectionHead(T("Open windows", "Öppna fönster")));
    M.windows = actionList();
    gtk_box_append(GTK_BOX(M.windowsBox), M.windows);
    gtk_box_append(GTK_BOX(home), M.windowsBox);
    M.recentBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(M.recentBox, "ws-recent");
    gtk_box_append(GTK_BOX(M.recentBox), sectionHead(T("Recent files", "Senaste filer")));
    M.recent = actionList();
    gtk_box_append(GTK_BOX(M.recentBox), M.recent);
    gtk_box_append(GTK_BOX(home), M.recentBox);
    auto* hint = gtk_label_new(T("Type to search programs, settings and files – or type a command",
                                 "Skriv för att söka program, inställningar och filer – eller skriv ett kommando"));
    gtk_widget_add_css_class(hint, "ws-hint");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_xalign(GTK_LABEL(hint), 0);
    gtk_widget_set_vexpand(hint, TRUE);
    gtk_widget_set_valign(hint, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(home), hint);
    gtk_stack_add_named(GTK_STACK(M.stack), scrolled(home), "home");
    // Program/Filer: appnätet i kategorier
    M.grid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(M.grid, "ws-grid");
    gtk_stack_add_named(GTK_STACK(M.stack), scrolled(M.grid), "apps");
    // förslag när man skriver
    M.results = actionList();
    gtk_stack_add_named(GTK_STACK(M.stack), scrolled(M.results), "results");
    gtk_stack_add_named(GTK_STACK(M.stack), buildAiPage(), "ai");
    gtk_box_append(GTK_BOX(body), M.stack);
    gtk_box_append(GTK_BOX(body), M.sideSlot);   // mini-terminalen till höger (0.4.0, Daniels önskemål)
    gtk_box_append(GTK_BOX(inner), body);
    // bottenraden (0.4.0): stora ikoner utan text – Program/Filer · Internet · Inställningar, fästa program, På/Av
    auto* bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    M.bottom = bottom;
    gtk_widget_add_css_class(bottom, "ws-bottom");
    {   // Program/Filer växlar till appnätet (Start står kvar); högerklick = Filer-programmet
        M.appsBtn = gtk_toggle_button_new();
        gtk_button_set_child(GTK_BUTTON(M.appsBtn), iconFor("w:apps", kBottomIcon));
        gtk_widget_add_css_class(M.appsBtn, "ws-bottom-btn");
        g_signal_connect(M.appsBtn, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer) {
            const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b));
            if (on) { buildGrid(); gtk_editable_set_text(GTK_EDITABLE(M.entry), ""); }
            gtk_stack_set_visible_child_name(GTK_STACK(M.stack), on ? "apps" : "home");
            gtk_widget_grab_focus(M.entry);
        }), nullptr);
        auto* rc = gtk_gesture_click_new();
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rc), GDK_BUTTON_SECONDARY);
        g_signal_connect(rc, "pressed", G_CALLBACK(+[](GtkGestureClick*, int, double, double, gpointer) {
            shellDone();
            H.spawn({"wenuyos-app-run", "wenuy-launch", "wenuyos-files"});
        }), nullptr);
        gtk_widget_add_controller(M.appsBtn, GTK_EVENT_CONTROLLER(rc));
        gtk_widget_set_tooltip_text(M.appsBtn, T("Programs and files (right click: Files)", "Program och filer (högerklick: Filer)"));
        gtk_box_append(GTK_BOX(bottom), M.appsBtn);
    }
    gtk_box_append(GTK_BOX(bottom), bottomButton("globe", T("Internet", "Internet"), [] {
        GAppInfo* b = g_app_info_get_default_for_uri_scheme("https");
        if (b) { H.launch(b); g_object_unref(b); }
        else H.spawn({"wenuyos-app-run", "xdg-open", "https://"});
    }));
    gtk_box_append(GTK_BOX(bottom), bottomButton("settings", T("Settings", "Inställningar"), [] { H.spawn({"wenuyos-app-run", "wenuy-launch", "wenuyos-settings"}); }));
    auto* sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_add_css_class(sep, "ws-bottom-sep");
    gtk_box_append(GTK_BOX(bottom), sep);
    // fästa program (högerklick på ett program → Fäst i Start); tomt: en kort förklaring
    M.pins = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_hexpand(M.pins, TRUE);
    {   // släpp ett program här: fäst det på den platsen (de fästa går att dra om i ny ordning)
        auto* dt = gtk_drop_target_new(G_TYPE_STRING, GDK_ACTION_COPY);
        g_signal_connect(dt, "drop", G_CALLBACK(+[](GtkDropTarget*, const GValue* v, double x, double, gpointer) -> gboolean {
            const char* s = g_value_get_string(v);
            if (!s || !g_str_has_prefix(s, "app:")) return FALSE;
            size_t idx = 0;   // antal fästa ikoner vars mitt ligger till vänster om släppunkten
            for (GtkWidget* c = gtk_widget_get_first_child(M.pins); c; c = gtk_widget_get_next_sibling(c)) {
                if (!GTK_IS_BUTTON(c)) continue;
                graphene_rect_t b;
                if (gtk_widget_compute_bounds(c, M.pins, &b) && b.origin.x + b.size.width / 2 < x) ++idx;
            }
            setPinOrder(movePin(dockPins(), s + 4, idx));
            fillPins();
            return TRUE;
        }), nullptr);
        gtk_widget_add_controller(M.pins, GTK_EVENT_CONTROLLER(dt));
    }
    gtk_box_append(GTK_BOX(bottom), clipRow(M.pins));
    {   // På/Av ▴: lås, logga ut, viloläge, starta om, stäng av
        auto* mb = gtk_menu_button_new();
        gtk_menu_button_set_child(GTK_MENU_BUTTON(mb), iconFor("w:power", kBottomIcon));
        gtk_widget_add_css_class(mb, "ws-bottom-btn");
        gtk_widget_set_tooltip_text(mb, T("Lock, log out, sleep, restart, shut down", "Lås, logga ut, viloläge, starta om, stäng av"));
        gtk_menu_button_set_direction(GTK_MENU_BUTTON(mb), GTK_ARROW_UP);
        auto* pop = gtk_popover_new();
        auto* list = actionList();
        addAction(list, menuRow("w:lock", T("Lock", "Lås"), "", false, 20), [] { H.lockScreen(); });
        addAction(list, menuRow("w:logout", T("Log out", "Logga ut"), "", false, 20), [] { H.logout(); });
        addAction(list, menuRow("w:moon", T("Sleep", "Viloläge"), "", false, 20), [] { H.spawn({"systemctl", "suspend"}); });
        addAction(list, menuRow("w:restart", T("Restart", "Starta om"), "", false, 20), [] { H.spawn({"systemctl", "reboot"}); });
        addAction(list, menuRow("w:power", T("Shut down", "Stäng av"), "", false, 20), [] { H.spawn({"systemctl", "poweroff"}); });
        gtk_popover_set_child(GTK_POPOVER(pop), list);
        gtk_menu_button_set_popover(GTK_MENU_BUTTON(mb), pop);
        gtk_box_append(GTK_BOX(bottom), mb);
    }
    gtk_box_append(GTK_BOX(inner), bottom);
    // sökfältet längst ned – ligger över sökfältet i fältet när Start är öppen
    M.entry = gtk_search_entry_new();
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(M.entry), T("Search or type a command…", "Sök eller skriv ett kommando…"));
    gtk_widget_add_css_class(M.entry, "ws-entry");
    g_signal_connect(M.entry, "search-changed", G_CALLBACK(+[](GtkSearchEntry*, gpointer) { showQuery(); }), nullptr);
    g_signal_connect(M.entry, "changed", G_CALLBACK(+[](GtkEditable*, gpointer) { showQuery(); }), nullptr);   // utan väntetid
    g_signal_connect(M.entry, "stop-search", G_CALLBACK(+[](GtkSearchEntry*, gpointer) { shellDone(); }), nullptr);   // Esc
    gtk_box_append(GTK_BOX(M.card), M.entry);
    // tangenter: pilar/Enter i sökfältet styr förslagen; Ner i tom vy går till nätet; att skriva var som
    // helst i Start hamnar i sökfältet
    auto* key = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(key, GTK_PHASE_CAPTURE);
    g_signal_connect(key, "key-pressed", G_CALLBACK(+[](GtkEventControllerKey*, guint kv, guint, GdkModifierType st, gpointer) -> gboolean {
        const bool inEntry = gtk_widget_has_focus(M.entry) || gtk_widget_is_ancestor(gtk_root_get_focus(gtk_widget_get_root(M.entry)), M.entry);
        const bool searching = !g_strcmp0(gtk_stack_get_visible_child_name(GTK_STACK(M.stack)), "results");
        // Tab i kommandoläget: komplettera kommandot eller sökvägen (som i en terminal)
        if (inEntry && kv == GDK_KEY_Tab && !(st & (GDK_CONTROL_MASK | GDK_ALT_MASK))) {
            const std::string line = gtk_editable_get_text(GTK_EDITABLE(M.entry));
            if (!line.empty() && line.find_first_not_of(' ') != std::string::npos) {
                auto c = tabCandidates(line);
                if (!c.empty()) {
                    const std::string done = completeLine(line, c);
                    if (done != line) {
                        gtk_editable_set_text(GTK_EDITABLE(M.entry), done.c_str());
                        gtk_editable_set_position(GTK_EDITABLE(M.entry), -1);
                    } else if (c.size() > 1 && c.size() <= 40 && gCommand) {   // flera: visa dem i mini-terminalen
                        std::string list;
                        for (auto& x : c) list += (list.empty() ? "" : "  ") + x;
                        gTabList = list;
                        showTabList();
                    }
                }
                return TRUE;
            }
        }
        if (inEntry && searching) {
            if (kv == GDK_KEY_Down || kv == GDK_KEY_Up) { moveSelection(GTK_LIST_BOX(M.results), kv == GDK_KEY_Down ? 1 : -1); gtk_widget_grab_focus(M.entry); return TRUE; }
            if (kv == GDK_KEY_Return || kv == GDK_KEY_KP_Enter) {
                if (GtkListBoxRow* sel = currentRow(M.results)) g_signal_emit_by_name(M.results, "row-activated", sel);
                return TRUE;
            }
        }
        // tom vy: Upp/Ner går genom öppna fönster och senaste filer (markeringen syns, fokus stannar i sökfältet),
        // Enter öppnar; i appnätet flyttar Upp fokus in i nätet
        const bool home = !g_strcmp0(gtk_stack_get_visible_child_name(GTK_STACK(M.stack)), "home");
        if (inEntry && home && !(st & (GDK_CONTROL_MASK | GDK_ALT_MASK))) {
            if (kv == GDK_KEY_Down || kv == GDK_KEY_Up) { moveHome(kv == GDK_KEY_Down ? 1 : -1); return TRUE; }
            if ((kv == GDK_KEY_Return || kv == GDK_KEY_KP_Enter) && M.homeCur) {
                GtkWidget* list = gtk_widget_get_parent(GTK_WIDGET(M.homeCur));
                g_signal_emit_by_name(list, "row-activated", M.homeCur);
                return TRUE;
            }
        }
        if (inEntry && !searching && !home && kv == GDK_KEY_Up) { gtk_widget_child_focus(M.stack, GTK_DIR_TAB_FORWARD); return TRUE; }
        // i mini-terminalen går tangenterna till terminalen
        GtkWidget* focus = gtk_root_get_focus(gtk_widget_get_root(M.entry));
        if (focus && M.side && (focus == M.side || gtk_widget_is_ancestor(focus, M.side))) return FALSE;
        if (!inEntry && !(st & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK))) {
            const gunichar c = gdk_keyval_to_unicode(kv);
            if (c && g_unichar_isprint(c) && c != ' ') {
                char buf[8] = {0};
                g_unichar_to_utf8(c, buf);
                const std::string t = std::string(gtk_editable_get_text(GTK_EDITABLE(M.entry))) + buf;
                gtk_editable_set_text(GTK_EDITABLE(M.entry), t.c_str());
                gtk_widget_grab_focus(M.entry);
                gtk_editable_set_position(GTK_EDITABLE(M.entry), -1);
                return TRUE;
            }
            if (kv == GDK_KEY_BackSpace) { gtk_widget_grab_focus(M.entry); return FALSE; }
        }
        return FALSE;
    }), nullptr);
    gtk_widget_add_controller(M.card, key);
    // nya/borttagna program: bygg om nätet nästa gång Start visas
    g_signal_connect(g_app_info_monitor_get(), "changed", G_CALLBACK(+[](GAppInfoMonitor*, gpointer) { M.dirty = true; }), nullptr);
}

void sizeCard() {
    if (M.tablet) {
        gtk_widget_set_size_request(M.card, -1, -1);
        gtk_widget_set_halign(M.card, GTK_ALIGN_FILL);
        gtk_widget_set_valign(M.card, GTK_ALIGN_FILL);
        gtk_widget_add_css_class(M.card, "ws-tablet");
        if (M.sideSlot) gtk_widget_set_visible(M.sideSlot, FALSE);   // surfplattan: ingen terminal
    } else {
        if (M.sideSlot) gtk_widget_set_visible(M.sideSlot, M.side != nullptr);
        gtk_widget_set_size_request(M.card, M.width > 0 ? M.width : M.side ? 900 : 640, 540);
        gtk_widget_set_halign(M.card, GTK_ALIGN_START);
        gtk_widget_set_valign(M.card, GTK_ALIGN_END);
        gtk_widget_remove_css_class(M.card, "ws-tablet");
    }
}

const char* kCss = R"CSS(
.ws-start { margin: 0 8px 8px 8px; background: @window_bg_color; border: 1px solid @borders; border-radius: 12px;
  box-shadow: 0 6px 20px alpha(black, 0.20); padding: 0; }
.ws-inner { padding: 12px 12px 8px 12px; }
.ws-start.ws-anchored { margin: 0; border-radius: 10px 10px 6px 6px; border-color: alpha(black, 0.25); }
.ws-start.ws-tablet { margin: 48px 64px; border-radius: 16px; box-shadow: none; }
.ws-tablet .ws-inner { padding: 20px 24px 12px 24px; }
/* sökfältet: samma som fältets (helt vitt i ljust tema, helt svart i mörkt, 40 px, 15 px text, ikon 18 px + 10 px) */
.ws-missing label { color: alpha(@window_fg_color, 0.5); }
.ws-ai { padding: 6px 4px 4px 6px; }
.ws-ai-q { font-weight: 700; margin: 4px 4px 2px 4px; }
.ws-ai-text { background: none; font-size: 0.95em; padding: 2px 4px; }
.ws-ai-text text { background: none; }
.ws-ai-cmd { font-family: monospace; font-size: 0.9em; }
.ws-ai-btn { min-height: 26px; padding: 0 10px; font-size: 0.9em; }
.ws-entry { min-height: 40px; font-size: 15px; margin: 0 12px 12px 12px; padding: 0 14px; border-radius: 6px;
  background: #ffffff; color: #000000; border: 1px solid alpha(black, 0.25); box-shadow: none; outline: none; }
.ws-entry > image { -gtk-icon-size: 18px; color: alpha(black, 0.75); opacity: 1; }
.ws-entry > image:first-child { margin-right: 5px; }
.ws-entry > text > placeholder { color: alpha(black, 0.6); opacity: 1; }
.ws-entry:focus-within { border-color: alpha(black, 0.25); box-shadow: none; outline: none; }
.ws-start.ws-anchored .ws-entry { margin: 0; border: none; border-top: 1px solid alpha(black, 0.25); border-radius: 0 0 5px 5px; }
.ws-start.ws-dark.ws-anchored { border-color: alpha(white, 0.28); }
.ws-dark .ws-entry { background: #000000; color: #ffffff; border-color: alpha(white, 0.28); }
.ws-dark .ws-entry > image, .ws-dark .ws-entry > text > placeholder { color: alpha(white, 0.7); }
.ws-start.ws-dark.ws-anchored .ws-entry { border-top-color: alpha(white, 0.28); }
.ws-side { min-width: 0; margin-left: 10px; padding-left: 10px; border-left: 1px solid alpha(@borders, 0.7); }
.ws-side.ws-side-full { min-width: 0; margin-left: 0; padding-left: 0; border-left: none; }
.ws-hint { margin: 16px 10px 6px 10px; font-size: 0.86em; color: alpha(@window_fg_color, 0.5); }
.ws-tablet .ws-entry { min-height: 52px; font-size: 18px; margin: 0 24px 16px 24px; }
.ws-grid { padding: 0 4px 8px 0; }
.ws-head { margin: 10px 10px 4px 10px; font-size: 0.82em; font-weight: 700; color: alpha(@window_fg_color, 0.55); }
.ws-cat { margin-top: 12px; }
.ws-tile { padding: 8px 2px 6px 2px; border-radius: 8px; background: none; box-shadow: none; }
.ws-tile:hover, .ws-tile:focus-visible { background: alpha(@accent_bg_color, 0.14); }
.ws-tile label { font-size: 0.86em; }
.ws-tile label.ws-ram { font-size: 0.72em; color: @warning_color; font-weight: 600; }
.ws-tablet .ws-tile { padding: 14px 6px 10px 6px; }
.ws-tablet .ws-tile label { font-size: 1em; }
.ws-bottom { border-top: 1px solid alpha(@borders, 0.7); padding-top: 6px; margin-top: 4px; }
.ws-bottom .ws-bottom-btn, .ws-bottom .ws-bottom-btn > button { padding: 6px; min-width: 40px; min-height: 40px; }
.ws-bottom-sep { margin: 8px 4px; }
.ws-often { padding: 6px; min-width: 44px; min-height: 44px; border-radius: 8px; background: none; box-shadow: none; }
.ws-often:hover { background: alpha(@accent_bg_color, 0.14); }
.ws-pin-hint { font-size: 0.82em; color: alpha(@window_fg_color, 0.5); }
.ws-bottom-btn:checked { background: alpha(@accent_bg_color, 0.16); color: @accent_bg_color; }
.ws-bottom-btn, .ws-bottom-btn > button { background: none; box-shadow: none; padding: 4px 10px; min-height: 30px; border-radius: 7px; }
.ws-bottom-btn:hover, .ws-bottom-btn > button:hover { background: alpha(@window_fg_color, 0.07); }
.ws-list { background: none; }
.ws-list > row { padding: 0; margin: 0 4px; border-radius: 6px; }
list.ws-list > row:selected { background-color: transparent; }
.ws-start list.ws-list > row.ws-current, popover list.ws-list > row.ws-current { background-color: @accent_bg_color; background-image: none; color: @accent_fg_color; }
.ws-start list.ws-list > row.ws-current .ws-sub, popover list.ws-list > row.ws-current .ws-sub { color: alpha(@accent_fg_color, 0.85); }
.ws-start list.ws-list > row.ws-current label { color: @accent_fg_color; }
.ws-list > row.ws-head-row, .ws-list > row.ws-head-row:hover { background: none; min-height: 0; }
.ws-row { padding: 5px 10px; min-height: 30px; }
.ws-sub { font-size: 0.85em; color: alpha(@window_fg_color, 0.6); }
.wenuy-dock-button, .wenuy-task-button { border-radius: 7px; background: none; box-shadow: none; padding: 0 7px; min-height: 0; }
.wenuy-task-dot-space { min-height: 2px; }
.wenuy-dock-button:hover, .wenuy-task-button:hover { background: alpha(@window_fg_color, 0.07); }
.wenuy-task-dot { min-width: 4px; min-height: 2px; border-radius: 2px; background: transparent; transition: all 200ms ease; }
.wenuy-task-button.active .wenuy-task-dot { min-width: 16px; background: @accent_bg_color; }
.wenuy-task-button.minimized .wenuy-task-dot { background: alpha(@window_fg_color, 0.35); }
.wenuy-task-button.minimized image { opacity: 0.6; }
)CSS";

void ensureBuilt() {
    static bool done = false;
    if (done) return;
    done = true;
    auto* css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css, kCss);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
    g_object_unref(css);
    buildStart();
    sizeCard();
    {   // sökfältet helt vitt/helt svart som fältets – följer ljust/mörkt
        auto apply = +[](GObject*, GParamSpec*, gpointer) {
            if (adw_style_manager_get_dark(adw_style_manager_get_default())) gtk_widget_add_css_class(M.card, "ws-dark");
            else gtk_widget_remove_css_class(M.card, "ws-dark");
        };
        g_signal_connect(adw_style_manager_get_default(), "notify::dark", G_CALLBACK(apply), nullptr);
        apply(nullptr, nullptr, nullptr);
    }
    g_object_ref_sink(M.card);   // kortet lever så länge programmet kör
}

}  // namespace
}  // namespace wenuy

using namespace wenuy;

void wenuy_shell_set_done_func(WenuyShellDoneFunc func, gpointer user_data) { gDone = func; gDoneData = user_data; }
void wenuy_shell_set_launch_func(WenuyShellLaunchFunc func, gpointer user_data) { gLaunched = func; gLaunchedData = user_data; }
void wenuy_shell_set_switch_funcs(WenuyShellSwitchFunc open_search, WenuyShellSwitchFunc open_start, gpointer user_data) {
    gOpenSearch = open_search; gOpenStart = open_start; gSwitchData = user_data;   // finns kvar för äldre skal; Start är en yta
}

void wenuy_shell_set_windows_func(WenuyShellWindowsFunc list, WenuyShellActivateFunc activate, gpointer user_data) {
    gWindows = list; gActivate = activate; gWindowsData = user_data;
}
void wenuy_shell_set_command_func(WenuyShellCommandFunc func, gpointer user_data) { gCommand = func; gCommandData = user_data; }
void wenuy_shell_set_tab_list_func(WenuyShellTabListFunc func) { gTabListFunc = func; }
void wenuy_shell_set_type_func(WenuyShellTabListFunc func) { gTypeFunc = func; }
void wenuy_start_menu_set_side(GtkWidget* side) {
    ensureBuilt();
    if (M.side) gtk_box_remove(GTK_BOX(M.sideSlot), M.side);
    M.side = side;
    if (side) { gtk_widget_set_vexpand(side, TRUE); gtk_box_append(GTK_BOX(M.sideSlot), side); }
    sizeCard();
}
gboolean wenuy_start_menu_launch_pin(int n) {
    auto pins = dockPins();
    if (n < 1 || n > (int)pins.size()) return FALSE;
    GDesktopAppInfo* d = g_desktop_app_info_new(pins[n - 1].c_str());
    if (!d) return FALSE;
    H.launch(G_APP_INFO(d));
    g_object_unref(d);
    return TRUE;
}

void wenuy_start_menu_set_width(int width) {
    ensureBuilt();
    if (M.width == width) return;
    M.width = width;
    M.dirty = true;   // appnätet efter den nya bredden
    sizeCard();
}
void wenuy_start_menu_set_side_expanded(gboolean expanded) {
    ensureBuilt();
    if (!M.side || M.expanded == (bool)expanded) return;
    M.expanded = expanded;
    gtk_widget_set_visible(M.stack, !expanded);
    gtk_widget_set_visible(M.bottom, !expanded);
    if (expanded) gtk_widget_add_css_class(M.sideSlot, "ws-side-full"); else gtk_widget_remove_css_class(M.sideSlot, "ws-side-full");
    gtk_widget_set_hexpand(M.sideSlot, expanded);
}
gboolean wenuy_start_menu_get_side_expanded(void) { return M.expanded; }

void wenuy_start_menu_set_anchor(int x, int bottom) {
    ensureBuilt();
    if (x < 0) {
        gtk_widget_remove_css_class(M.card, "ws-anchored");
        gtk_widget_set_margin_start(M.card, 0);
        gtk_widget_set_margin_bottom(M.card, 0);
        return;
    }
    gtk_widget_add_css_class(M.card, "ws-anchored");
    gtk_widget_set_margin_start(M.card, x);
    gtk_widget_set_margin_bottom(M.card, bottom);
}

GtkWidget* wenuy_start_menu_get(void) { wenuy::Measure m("start-menu"); ensureBuilt(); return M.card; }

void wenuy_start_menu_reset(void) { wenuy_search_box_reset(nullptr); }

GtkWidget* wenuy_search_box_get(void) { ensureBuilt(); return M.card; }

void wenuy_search_box_reset(const char* text) {
    ensureBuilt();
    buildGrid();
    fillRecent();
    fillFiles();
    fillWindows();
    fillPins();
    fillOften();
    {   // Firefox historik till förslagen (högst en gång i minuten; wenuyos-webb gör inget om inget ändrats)
        static gint64 last = 0;
        const gint64 t = g_get_monotonic_time();
        if (recentEnabled() && t - last > 60 * G_USEC_PER_SEC) {
            last = t;
            if (gchar* w = g_find_program_in_path("wenuyos-webb")) { g_free(w); H.spawn({"wenuyos-webb", "--export-history"}); }
        }
    }
    M.homeCur = nullptr;
    // inget att visa i den tomma vyn (inga fönster, fästa eller senaste filer): appnätet direkt
    const bool homeEmpty = !gtk_widget_get_visible(M.oftenBox) && !gtk_widget_get_visible(M.windowsBox) && !gtk_widget_get_visible(M.recentBox);
    gtk_stack_set_visible_child_name(GTK_STACK(M.stack), homeEmpty ? "apps" : "home");
    gtk_editable_set_text(GTK_EDITABLE(M.entry), text ? text : "");
    showQuery();
    gtk_widget_grab_focus(M.entry);
    gtk_editable_set_position(GTK_EDITABLE(M.entry), -1);
    // nätet börjar överst
    if (GtkWidget* sc = gtk_widget_get_ancestor(M.grid, GTK_TYPE_SCROLLED_WINDOW))
        gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sc)), 0);
}

void wenuy_start_menu_set_tablet(gboolean tablet) {
    ensureBuilt();
    if (M.tablet == (bool)tablet) return;
    M.tablet = tablet;
    M.dirty = true;   // större brickor
    sizeCard();
}

// ---------------------------------------------------------------- aktivitetsfältet
GtkWidget* wenuy_dock_button_new(const char* icon_name, const char* tooltip, int size) {
    ensureBuilt();
    auto* b = gtk_button_new();
    auto* img = gtk_image_new_from_icon_name(icon_name);
    gtk_image_set_pixel_size(GTK_IMAGE(img), size > 0 ? size : 30);
    gtk_button_set_child(GTK_BUTTON(b), img);
    gtk_widget_add_css_class(b, "wenuy-dock-button");
    if (tooltip) gtk_widget_set_tooltip_text(b, tooltip);
    return b;
}

GtkWidget* wenuy_task_button_new(GIcon* icon) {
    ensureBuilt();
    auto* b = gtk_button_new();
    auto* v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    auto* img = icon ? gtk_image_new_from_gicon(icon) : gtk_image_new_from_icon_name("application-x-executable");
    gtk_image_set_pixel_size(GTK_IMAGE(img), 26);
    auto* dot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);   // streck under ikonen: aktivt / minimerat
    gtk_widget_add_css_class(dot, "wenuy-task-dot");
    gtk_widget_set_halign(dot, GTK_ALIGN_CENTER);
    // lika stort tomrum ovanför ikonen som strecket under – ikonen hamnar mitt i dockan, i linje med hemknappen
    auto* space = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(space, "wenuy-task-dot-space");
    gtk_box_append(GTK_BOX(v), space);
    gtk_box_append(GTK_BOX(v), img);
    gtk_box_append(GTK_BOX(v), dot);
    gtk_button_set_child(GTK_BUTTON(b), v);
    gtk_widget_add_css_class(b, "wenuy-task-button");
    return b;
}

void wenuy_task_button_set_state(GtkWidget* button, gboolean active, gboolean minimized) {
    if (active && !minimized) gtk_widget_add_css_class(button, "active"); else gtk_widget_remove_css_class(button, "active");
    if (minimized) gtk_widget_add_css_class(button, "minimized"); else gtk_widget_remove_css_class(button, "minimized");
}

GIcon* wenuy_app_icon_for_id(const char* app_id) {
    if (!app_id || !*app_id) return g_themed_icon_new("application-x-executable");
    const std::string appId = app_id;
    // WenuyOS egna program: app-id se.wenuyos.Preview → se.wenuyos.Preview.desktop eller wenuyos-preview.desktop
    std::string own = appId.rfind("se.wenuyos.", 0) == 0 ? "wenuyos-" + lower(appId.substr(11)) : appId;
    for (std::string id : {appId, lower(appId), "org." + appId, own}) {
        if (GDesktopAppInfo* d = g_desktop_app_info_new((id + ".desktop").c_str())) {
            GIcon* i = g_app_info_get_icon(G_APP_INFO(d));
            if (i) g_object_ref(i);
            g_object_unref(d);
            if (i) return i;
        }
    }
    for (GAppInfo* a : H.apps())   // StartupWMClass
        if (G_IS_DESKTOP_APP_INFO(a)) {
            const char* wm = g_desktop_app_info_get_startup_wm_class(G_DESKTOP_APP_INFO(a));
            if (wm && g_ascii_strcasecmp(wm, app_id) == 0) { GIcon* i = g_app_info_get_icon(a); return i ? G_ICON(g_object_ref(i)) : g_themed_icon_new("application-x-executable"); }
        }
    return g_themed_icon_new(app_id);
}
