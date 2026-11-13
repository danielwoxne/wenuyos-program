// libwenuy – Arbetsminne (se WenuyMemory.h).
#include "WenuyMemory.h"

#include <fcntl.h>
#include <glib-unix.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <set>

namespace wenuy {
namespace memory {

bool enabled() {
    gchar* c = nullptr;
    bool on = true;
    if (g_file_get_contents((std::string(g_get_user_config_dir()) + "/wenuyos/arbetsminne").c_str(), &c, nullptr, nullptr)) {
        on = !g_str_has_prefix(g_strstrip(c), "off");
        g_free(c);
    }
    return on;
}
std::string root() { return std::string(g_get_user_state_dir()) + "/wenuyos/arbetsminne"; }
std::string pathFor(const std::string& program, const std::string& id) { return root() + "/" + program + "/" + id + ".toml"; }

std::string newId() {
    GDateTime* now = g_date_time_new_now_local();
    gchar* t = g_date_time_format(now, "%Y%m%d-%H%M%S");
    g_date_time_unref(now);
    char r[8];
    g_snprintf(r, sizeof r, "%04x", g_random_int_range(0, 0x10000));
    std::string id = std::string(t) + "-" + r;
    g_free(t);
    return id;
}

namespace {
std::string quote(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\t': o += "\\t"; break;
            case '\r': o += "\\r"; break;
            default:
                if (c < 0x20) { char b[8]; g_snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    return o + "\"";
}
// läser en citerad sträng från s[i] (som ska vara '"'); i flyttas förbi slutcitattecknet
bool unquote(const std::string& s, size_t& i, std::string& out) {
    if (i >= s.size() || s[i] != '"') return false;
    out.clear();
    for (++i; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') { ++i; return true; }
        if (c != '\\') { out += c; continue; }
        if (++i >= s.size()) return false;
        switch (s[i]) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case 'u': {
                if (i + 4 >= s.size()) return false;
                const gunichar u = (gunichar)strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16);
                char b[8] = {0};
                g_unichar_to_utf8(u, b);
                out += b;
                i += 4;
                break;
            }
            default: return false;
        }
    }
    return false;
}
void skipSpace(const std::string& s, size_t& i) { while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i; }
}  // namespace

std::string toToml(const Entry& e) {
    GDateTime* now = g_date_time_new_now_local();
    gchar* t = g_date_time_format_iso8601(now);
    g_date_time_unref(now);
    std::string o = "# WenuyOS arbetsminne – skrivs av programmet, tas bort när du stänger fönstret\n";
    o += "program = " + quote(e.program) + "\n";
    o += "fönster = " + quote(e.id) + "\n";
    o += "exec = " + quote(e.exec) + "\n";
    o += std::string("sparad = ") + quote(t ? t : "") + "\n";
    g_free(t);
    o += "\n[minne]\n";
    for (auto& [k, v] : e.values) o += quote(k) + " = " + quote(v) + "\n";
    return o;
}

bool fromToml(const std::string& text, Entry& e) {
    e = Entry();
    bool inMemory = false, any = false;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        size_t i = 0;
        skipSpace(line, i);
        if (i >= line.size() || line[i] == '#') continue;
        if (line[i] == '[') { inMemory = line.compare(i, 7, "[minne]") == 0; continue; }
        std::string key;
        if (line[i] == '"') { if (!unquote(line, i, key)) return false; }
        else { while (i < line.size() && line[i] != '=' && line[i] != ' ' && line[i] != '\t') key += line[i++]; }
        skipSpace(line, i);
        if (i >= line.size() || line[i] != '=') return false;
        ++i;
        skipSpace(line, i);
        std::string val;
        if (!unquote(line, i, val)) return false;
        any = true;
        if (inMemory) e.values[key] = val;
        else if (key == "program") e.program = val;
        else if (key == "fönster") e.id = val;
        else if (key == "exec") e.exec = val;
    }
    return any;
}

bool save(const std::string& path, const Entry& e) {
    gchar* dir = g_path_get_dirname(path.c_str());
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
    const std::string tmp = path + ".tmp";
    const std::string data = toToml(e);
    const int fd = g_open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    bool ok = write(fd, data.data(), data.size()) == (ssize_t)data.size();
    ok = fsync(fd) == 0 && ok;   // finns kvar även om strömmen går
    close(fd);
    if (!ok || g_rename(tmp.c_str(), path.c_str()) != 0) { g_unlink(tmp.c_str()); return false; }
    return true;
}

bool load(const std::string& path, Entry& e) {
    gchar* c = nullptr;
    gsize n = 0;
    if (!g_file_get_contents(path.c_str(), &c, &n, nullptr)) return false;
    const bool ok = fromToml(std::string(c, n), e);
    g_free(c);
    return ok;
}

}  // namespace memory
}  // namespace wenuy

// ------------------------------------------------------------------ fönstren
namespace {
using wenuy::memory::Entry;

struct WinMem {
    GtkWidget* win = nullptr;
    Entry entry;
    std::string path;
    guint timer = 0;
    bool dirty = false;
};
std::set<WinMem*> gAll;

void writeNow(WinMem* m) {
    if (m->timer) { g_source_remove(m->timer); m->timer = 0; }
    if (!m->dirty || !wenuy::memory::enabled()) return;
    m->dirty = false;
    wenuy::memory::save(m->path, m->entry);
}

gboolean onSignal(gpointer) {
    wenuy_app_memory_flush();
    _exit(0);   // som standardåtgärden för SIGTERM – fönstren "stängs" inte, så minnet ligger kvar
    return G_SOURCE_REMOVE;
}

WinMem* memOf(GtkWidget* win, bool create) {
    if (!win) return nullptr;
    auto* m = static_cast<WinMem*>(g_object_get_data(G_OBJECT(win), "wenuy-memory"));
    if (m || !create) return m;
    m = new WinMem;
    m->win = win;
    m->entry.program = g_get_prgname() ? g_get_prgname() : "program";
    m->entry.exec = m->entry.program;
    m->entry.id = wenuy::memory::newId();
    m->path = wenuy::memory::pathFor(m->entry.program, m->entry.id);
    gAll.insert(m);
    g_object_set_data_full(G_OBJECT(win), "wenuy-memory", m, +[](gpointer p) {
        auto* m = static_cast<WinMem*>(p);
        if (m->timer) g_source_remove(m->timer);
        gAll.erase(m);
        delete m;
    });
    // användaren stänger fönstret (eller programmet avslutas på vanligt sätt) → glöm det
    g_signal_connect(win, "destroy", G_CALLBACK(+[](GtkWidget* w, gpointer) {
        if (auto* m = memOf(w, false)) {
            if (m->timer) { g_source_remove(m->timer); m->timer = 0; }
            m->dirty = false;
            g_unlink(m->path.c_str());
            gchar* dir = g_path_get_dirname(m->path.c_str());
            g_rmdir(dir);   // tom programmapp
            g_free(dir);
        }
    }), nullptr);
    static bool signals = false;
    if (!signals) {   // utloggning/avstängning: skriv det som väntar innan programmet försvinner
        signals = true;
        g_unix_signal_add(SIGTERM, onSignal, nullptr);
        g_unix_signal_add(SIGHUP, onSignal, nullptr);
    }
    return m;
}
}  // namespace

void wenuy_app_remember(GtkWidget* window, const char* key, const char* value) {
    if (!key || !*key) return;
    WinMem* m = memOf(window, true);
    if (!m) return;
    auto it = m->entry.values.find(key);
    if (!value) {
        if (it == m->entry.values.end()) return;
        m->entry.values.erase(it);
    } else {
        if (it != m->entry.values.end() && it->second == value) return;   // oförändrat
        m->entry.values[key] = value;
    }
    m->dirty = true;
    if (!m->timer)   // högst var 2:a sekund – även medan man rullar hela tiden
        m->timer = g_timeout_add_seconds(2, +[](gpointer p) -> gboolean {
            auto* m = static_cast<WinMem*>(p);
            m->timer = 0;
            writeNow(m);
            return G_SOURCE_REMOVE;
        }, m);
}

char* wenuy_app_recall(GtkWidget* window, const char* key) {
    WinMem* m = memOf(window, false);
    if (!m || !key) return nullptr;
    auto it = m->entry.values.find(key);
    return it == m->entry.values.end() ? nullptr : g_strdup(it->second.c_str());
}

gboolean wenuy_app_window_resume(GtkWidget* window, const char* id) {
    if (!window || !id || !*id || strchr(id, '/')) return FALSE;
    WinMem* m = memOf(window, true);
    const std::string path = wenuy::memory::pathFor(m->entry.program, id);
    Entry e;
    if (!wenuy::memory::load(path, e)) return FALSE;
    m->entry.id = id;
    m->entry.values = e.values;
    m->path = path;
    m->dirty = true;   // skrivs igen (ny tid i "sparad") – så syns det att fönstret lever
    if (!m->timer)
        m->timer = g_timeout_add_seconds(2, +[](gpointer p) -> gboolean {
            auto* m = static_cast<WinMem*>(p);
            m->timer = 0;
            writeNow(m);
            return G_SOURCE_REMOVE;
        }, m);
    return TRUE;
}

const char* wenuy_app_window_get_memory_id(GtkWidget* window) {
    WinMem* m = memOf(window, false);
    return m ? m->entry.id.c_str() : nullptr;
}

char* wenuy_app_take_resume_arg(int* argc, char** argv) {
    if (!argc || !argv) return nullptr;
    char* id = nullptr;
    int out = 0;
    for (int i = 0; i < *argc; ++i) {
        if (!id && !strcmp(argv[i], "--resume") && i + 1 < *argc) { id = g_strdup(argv[++i]); continue; }
        if (!id && g_str_has_prefix(argv[i], "--resume=")) { id = g_strdup(argv[i] + 9); continue; }
        argv[out++] = argv[i];
    }
    *argc = out;
    argv[out] = nullptr;
    return id;
}

void wenuy_app_memory_flush(void) {
    for (WinMem* m : gAll) writeNow(m);
}
