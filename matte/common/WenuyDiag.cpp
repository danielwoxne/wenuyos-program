// libwenuy – mätning och buggkontroll. Se WenuyDiag.h.
#include "WenuyDiag.h"

#include <glib-unix.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "WenuyCrash.h"

namespace {

struct Check { std::string name; WenuyDiagCheckFunc fn; gpointer data; GDestroyNotify destroy; std::string result; bool ran = false; bool ok = true; };
struct Mark { std::string name; gint64 ms; guint64 rss; };
// Mätning av funktioner (wenuy_diag_measure_*): antal, misslyckade, tid
struct Func { guint64 count = 0, fail = 0; double totalMs = 0, maxMs = 0, lastMs = 0; };

struct State {
    std::mutex lock;
    std::string appId, path;
    gint64 startMono = 0;          // monoton klocka vid processstart
    gint64 startupMs = -1;         // till första fönstret
    int warnings = 0, criticals = 0;
    std::deque<std::string> recent;   // senaste meddelandena
    std::vector<Mark> marks;
    std::vector<Check> checks;
    std::map<std::string, Func> funcs;
    std::map<guint64, std::pair<std::string, gint64>> open;   // pågående mätningar
    guint64 nextToken = 1;
    guint writeLater = 0;
    bool previousCrashed = false;
    bool exiting = false;
    bool autoNamed = false;
};
State* S = nullptr;

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += char(c); }
        else if (c == '\n') o += "\\n";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += char(c);
    }
    return o;
}

// Processens starttid (från /proc) i den monotona klockan – så räknas även laddning av bibliotek
gint64 processStartMono() {
    // Startad via wenuy-launch (ev. genom zygoten): räkna från när starten begärdes – samma klocka
    if (const char* t0 = g_getenv("WENUY_LAUNCH_T0")) {
        const gint64 v = g_ascii_strtoll(t0, nullptr, 10), now = g_get_monotonic_time();
        if (v > 0 && v <= now && now - v < 600 * G_USEC_PER_SEC) return v;
    }
    // /proc/self/stat: fält 22 (starttime, i klocktick sedan uppstart); allt efter ") " är fält 3 …
    gchar* stat = nullptr;
    if (!g_file_get_contents("/proc/self/stat", &stat, nullptr, nullptr)) return g_get_monotonic_time();
    unsigned long long startTicks = 0;
    if (const char* p = strrchr(stat, ')')) {
        gchar** tok = g_strsplit(p + 2, " ", -1);
        if (g_strv_length(tok) > 19) startTicks = g_ascii_strtoull(tok[19], nullptr, 10);
        g_strfreev(tok);
    }
    g_free(stat);
    const long hz = sysconf(_SC_CLK_TCK);
    struct timespec now;
    clock_gettime(CLOCK_BOOTTIME, &now);
    const gint64 bootUs = gint64(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
    gint64 elapsed = startTicks && hz > 0 ? bootUs - gint64(startTicks * 1000000ULL / hz) : 0;
    if (elapsed < 0 || elapsed > 600 * G_USEC_PER_SEC) elapsed = 0;   // osannolikt: räkna från nu
    return g_get_monotonic_time() - elapsed;
}

// Programmets namn när inget id angetts: för python3/gjs/sh används skriptets projektmapp
std::string guessAppId() {
    gchar* cmd = nullptr;
    gsize len = 0;
    std::string comm = g_get_prgname() ? g_get_prgname() : "";
    if (g_file_get_contents("/proc/self/cmdline", &cmd, &len, nullptr)) {
        std::vector<std::string> args;
        for (gsize i = 0; i < len;) { args.emplace_back(cmd + i); i += args.back().size() + 1; }
        g_free(cmd);
        if (!args.empty()) {
            gchar* exe = g_path_get_basename(args[0].c_str());
            comm = exe;
            g_free(exe);
            const bool interp = g_str_has_prefix(comm.c_str(), "python") || comm == "gjs" || comm == "sh" || comm == "bash";
            for (size_t i = 1; interp && i < args.size(); ++i) {
                if (args[i].empty() || args[i][0] == '-') continue;
                gchar* dir = g_path_get_dirname(args[i].c_str());
                gchar* base = g_path_get_basename(dir);
                comm = std::string(base) == "." ? args[i] : base;
                g_free(dir);
                g_free(base);
                break;
            }
        }
    }
    return comm.empty() ? "okänt" : comm;
}

guint64 statusKb(const char* key) {
    gchar* s = nullptr;
    if (!g_file_get_contents("/proc/self/status", &s, nullptr, nullptr)) return 0;
    guint64 v = 0;
    if (const char* p = strstr(s, key)) v = g_ascii_strtoull(p + strlen(key), nullptr, 10);
    g_free(s);
    return v;
}

GLogWriterOutput writer(GLogLevelFlags level, const GLogField* fields, gsize n, gpointer) {
    if (S && (level & (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_ERROR))) {
        std::string domain, msg;
        for (gsize i = 0; i < n; ++i) {
            if (g_str_equal(fields[i].key, "GLIB_DOMAIN") && fields[i].value) domain = static_cast<const char*>(fields[i].value);
            if (g_str_equal(fields[i].key, "MESSAGE") && fields[i].value) msg = static_cast<const char*>(fields[i].value);
        }
        // vanliga, ofarliga varningar i virtuella maskiner/utan skärmläsare räknas inte
        const bool noise = msg.find("accessibility bus") != std::string::npos || msg.find("a11y") != std::string::npos;
        if (!noise) {
            std::lock_guard<std::mutex> g(S->lock);
            if (level & G_LOG_LEVEL_WARNING) ++S->warnings; else ++S->criticals;
            S->recent.push_back((level & G_LOG_LEVEL_WARNING ? "W " : "C ") + (domain.empty() ? "" : domain + ": ") + msg.substr(0, 300));
            if (S->recent.size() > 20) S->recent.pop_front();
        }
    }
    return g_log_writer_default(level, fields, n, nullptr);
}

std::string reportPath(const std::string& appId) {
    const std::string dir = std::string(g_get_user_state_dir()) + "/wenuyos/diag";
    g_mkdir_with_parents(dir.c_str(), 0700);
    return dir + "/" + appId + ".json";
}

void writeReport(bool running) {
    if (!S) return;
    std::string j;
    {
        std::lock_guard<std::mutex> g(S->lock);
        GDateTime* now = g_date_time_new_now_local();
        gchar* when = g_date_time_format_iso8601(now);
        g_date_time_unref(now);
        j = "{\n  \"app\": \"" + jsonEscape(S->appId) + "\",\n  \"pid\": " + std::to_string(getpid()) +
            ",\n  \"running\": " + (running ? "true" : "false") + ",\n  \"updated\": \"" + when + "\"" +
            ",\n  \"startup_ms\": " + std::to_string(S->startupMs) +
            ",\n  \"rss_kb\": " + std::to_string(statusKb("VmRSS:")) + ",\n  \"peak_rss_kb\": " + std::to_string(statusKb("VmHWM:")) +
            ",\n  \"warnings\": " + std::to_string(S->warnings) + ",\n  \"criticals\": " + std::to_string(S->criticals) +
            ",\n  \"previous_run_crashed\": " + (S->previousCrashed ? "true" : "false") + ",\n  \"recent\": [";
        g_free(when);
        for (size_t i = 0; i < S->recent.size(); ++i) j += (i ? ", " : "") + std::string("\"") + jsonEscape(S->recent[i]) + "\"";
        j += "],\n  \"marks\": [";
        for (size_t i = 0; i < S->marks.size(); ++i)
            j += (i ? ", " : "") + std::string("{\"name\": \"") + jsonEscape(S->marks[i].name) + "\", \"ms\": " +
                 std::to_string(S->marks[i].ms) + ", \"rss_kb\": " + std::to_string(S->marks[i].rss) + "}";
        j += "],\n  \"checks\": [";
        bool first = true;
        for (auto& c : S->checks) {
            if (!c.ran) continue;
            j += (first ? "" : ", ") + std::string("{\"name\": \"") + jsonEscape(c.name) + "\", \"ok\": " + (c.ok ? "true" : "false") +
                 ", \"message\": \"" + jsonEscape(c.result) + "\"}";
            first = false;
        }
        j += "],\n  \"functions\": {";
        bool firstF = true;
        for (auto& [name, f] : S->funcs) {
            char buf[256];
            g_snprintf(buf, sizeof buf, "\"count\": %" G_GUINT64_FORMAT ", \"fail\": %" G_GUINT64_FORMAT ", \"avg_ms\": %.1f, \"max_ms\": %.1f, \"last_ms\": %.1f",
                       f.count, f.fail, f.count ? f.totalMs / f.count : 0.0, f.maxMs, f.lastMs);
            j += (firstF ? "\n    \"" : ",\n    \"") + jsonEscape(name) + "\": {" + buf + "}";
            firstF = false;
        }
        j += "\n  }\n}\n";
    }
    g_file_set_contents(S->path.c_str(), j.c_str(), -1, nullptr);
}

gboolean onWindowMapped(GSignalInvocationHint*, guint, const GValue* params, gpointer) {
    GObject* obj = G_OBJECT(g_value_get_object(&params[0]));
    if (!S || !GTK_IS_WINDOW(obj)) return TRUE;
    if (S->startupMs < 0) {
        S->startupMs = (g_get_monotonic_time() - S->startMono) / 1000;
        wenuy_diag_mark("first-window");
        writeReport(true);
        if (g_strcmp0(g_getenv("WENUY_SELFTEST"), "1") == 0) {
            // självtest: låt fönstret rita klart, kör kontrollerna och avsluta
            g_timeout_add(1500, +[](gpointer) -> gboolean {
                const int fails = wenuy_diag_run_checks();
                S->exiting = true;
                writeReport(false);
                _exit(fails > 0 ? 1 : 0);
                return G_SOURCE_REMOVE;
            }, nullptr);
        }
        return FALSE;   // bara första fönstret behövs
    }
    return TRUE;
}

void atExit() {
    if (S && !S->exiting) { S->exiting = true; writeReport(false); }
}

}  // namespace

void wenuy_diag_init(const char* app_id) {
    if (S) {
        // startades automatiskt (utan id) – byt till det riktiga namnet när programmet anger det
        if (app_id && *app_id && S->appId != app_id && S->autoNamed) {
            g_unlink(S->path.c_str());
            S->appId = app_id;
            S->path = reportPath(S->appId);
            S->autoNamed = false;
            writeReport(true);
        }
        return;
    }
    if (g_strcmp0(g_getenv("WENUY_DIAG"), "0") == 0) return;   // går att stänga av
    S = new State;
    S->startMono = processStartMono();
    S->autoNamed = !(app_id && *app_id);
    S->appId = app_id && *app_id ? app_id : guessAppId();
    for (auto& c : S->appId) if (c == '/') c = '_';
    S->path = reportPath(S->appId);
    // förra körningen avslutades aldrig och processen finns inte → den kraschade
    gchar* prev = nullptr;
    if (g_file_get_contents(S->path.c_str(), &prev, nullptr, nullptr)) {
        if (strstr(prev, "\"running\": true")) {
            const char* p = strstr(prev, "\"pid\": ");
            const int pid = p ? atoi(p + 7) : 0;
            if (pid > 0 && pid != getpid() && !g_file_test(("/proc/" + std::to_string(pid)).c_str(), G_FILE_TEST_EXISTS))
                S->previousCrashed = true;
        }
        g_free(prev);
    }
    // GLib tillåter bara en loggskrivare per process; GJS har en egen, så där räknas inga varningar
    static bool writerSet = false;
    gchar* comm = nullptr;
    g_file_get_contents("/proc/self/comm", &comm, nullptr, nullptr);
    const bool gjs = (comm && g_str_has_prefix(comm, "gjs")) || g_getenv("GJS_PATH");
    g_free(comm);
    if (!writerSet && !gjs && !g_getenv("WENUY_DIAG_NO_LOG")) { writerSet = true; g_log_set_writer_func(writer, nullptr, nullptr); }
    g_type_class_ref(GTK_TYPE_WIDGET);   // signalen "map" måste finnas innan kroken sätts
    g_signal_add_emission_hook(g_signal_lookup("map", GTK_TYPE_WIDGET), 0, onWindowMapped, nullptr, nullptr);
    atexit(atExit);
    // Avslutad av systemet (utloggning, avstängning, kill): skriv att körningen tog slut som den skulle – förut
    // räknades det som en krasch ("förra körningen avslutades aldrig") efter varje omstart
    for (int sig : {SIGTERM, SIGHUP})
        g_unix_signal_add(sig, +[](gpointer s) -> gboolean {
            atExit();
            signal(GPOINTER_TO_INT(s), SIG_DFL);
            raise(GPOINTER_TO_INT(s));
            return G_SOURCE_REMOVE;
        }, GINT_TO_POINTER(sig));
    // egen kraschlogg (~/.local/state/wenuyos/crash/) – systemets kraschspår räcker inte alltid
    wenuy::crash::install(S->appId.c_str());
    writeReport(true);
}

void wenuy_diag_mark(const char* name) {
    if (!S) return;
    std::lock_guard<std::mutex> g(S->lock);
    S->marks.push_back({name ? name : "", (g_get_monotonic_time() - S->startMono) / 1000, statusKb("VmRSS:")});
    if (S->marks.size() > 50) S->marks.erase(S->marks.begin());
}

gint64 wenuy_diag_elapsed_ms(void) { return S ? (g_get_monotonic_time() - S->startMono) / 1000 : 0; }
guint64 wenuy_diag_rss_kb(void) { return statusKb("VmRSS:"); }

void wenuy_diag_add_check(const char* name, WenuyDiagCheckFunc func, gpointer data, GDestroyNotify destroy) {
    if (!S) wenuy_diag_init(nullptr);
    if (!S) { if (destroy) destroy(data); return; }
    S->checks.push_back({name ? name : "?", func, data, destroy});
}

int wenuy_diag_run_checks(void) {
    if (!S) return 0;
    int fails = 0;
    for (auto& c : S->checks) {
        char* r = c.fn ? c.fn(c.data) : nullptr;
        c.ran = true;
        c.ok = r == nullptr;
        c.result = r ? r : "";
        g_free(r);
        if (!c.ok) ++fails;
    }
    // inbyggda: inga kritiska fel, rimlig start
    if (S->criticals > 0) ++fails;
    writeReport(!S->exiting);
    return fails;
}

void wenuy_diag_write(void) { writeReport(S && !S->exiting); }

// Alla program som använder libwenuy får mätning direkt när biblioteket laddas
// (även Python/JavaScript via GObject Introspection); wenuy_diag_init(app_id) ger sedan rätt namn.
__attribute__((constructor)) static void wenuy_diag_autostart() {
    if (!g_getenv("WENUY_DIAG_LATE")) wenuy_diag_init(nullptr);
}

// ---- mätning av funktioner
namespace {
void record(const std::string& name, double ms, bool ok) {
    if (!S) return;
    {
        std::lock_guard<std::mutex> g(S->lock);
        Func& f = S->funcs[name];
        f.count++;
        if (!ok) f.fail++;
        if (ms >= 0) { f.totalMs += ms; f.lastMs = ms; if (ms > f.maxMs) f.maxMs = ms; }
    }
    // rapporten skrivs högst var 10:e sekund (inte vid varje mätning)
    if (!S->writeLater)
        S->writeLater = g_timeout_add_seconds(10, +[](gpointer) -> gboolean { if (S) { S->writeLater = 0; writeReport(true); } return G_SOURCE_REMOVE; }, nullptr);
}
}  // namespace

guint64 wenuy_diag_measure_begin(const char* name) {
    if (!S) wenuy_diag_init(nullptr);
    if (!S || !name) return 0;
    std::lock_guard<std::mutex> g(S->lock);
    const guint64 t = S->nextToken++;
    S->open[t] = {name, g_get_monotonic_time()};
    if (S->open.size() > 256) S->open.erase(S->open.begin());   // glömda mätningar fyller inte minnet
    return t;
}

void wenuy_diag_measure_end(guint64 token, gboolean ok) {
    if (!S || !token) return;
    std::string name;
    gint64 start = 0;
    {
        std::lock_guard<std::mutex> g(S->lock);
        auto it = S->open.find(token);
        if (it == S->open.end()) return;
        name = it->second.first;
        start = it->second.second;
        S->open.erase(it);
    }
    record(name, (g_get_monotonic_time() - start) / 1000.0, ok);
}

void wenuy_diag_event(const char* name, gboolean ok) {
    if (!S) wenuy_diag_init(nullptr);
    if (name) record(name, -1, ok);
}
