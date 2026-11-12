// libwenuy – inställningar (WenuySettings) och databas (WenuyStore). Se WenuyData.h.
#include "WenuyData.h"
#include "Wenuy.h"

#include <sqlite3.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {
constexpr const char* kGroup = "settings";
constexpr const char* kLocked = "locked";
GQuark storeQuark() { return g_quark_from_static_string("wenuy-store-error"); }
}  // namespace

/* =================================================================== WenuySettings */
struct _WenuySettings {
    GObject parent;
    std::string* appId;
    std::string* path;
    GKeyFile* defaults;   // /usr/share + /etc/xdg (sammanslagna)
    GKeyFile* user;       // ~/.config/<id>/settings.ini
    GFileMonitor* monitor;
    guint saveId;
    guint reloadId;
};
G_DEFINE_FINAL_TYPE(WenuySettings, wenuy_settings, G_TYPE_OBJECT)

enum { SET_CHANGED, SET_N };
static guint setSignals[SET_N];

static void mergeInto(GKeyFile* dst, const std::string& file) {
    GKeyFile* k = g_key_file_new();
    if (g_key_file_load_from_file(k, file.c_str(), G_KEY_FILE_NONE, nullptr)) {
        for (const char* group : {kGroup, kLocked}) {
            gsize n = 0;
            gchar** keys = g_key_file_get_keys(k, group, &n, nullptr);
            for (gsize i = 0; keys && i < n; ++i) {
                gchar* v = g_key_file_get_value(k, group, keys[i], nullptr);
                if (v) g_key_file_set_value(dst, group, keys[i], v);
                g_free(v);
            }
            g_strfreev(keys);
        }
    }
    g_key_file_free(k);
}

// Effektivt värde (råtext): låst > användare > standard
static gchar* rawValue(WenuySettings* s, const char* key) {
    if (gchar* v = g_key_file_get_value(s->defaults, kLocked, key, nullptr)) return v;
    if (gchar* v = g_key_file_get_value(s->user, kGroup, key, nullptr)) return v;
    return g_key_file_get_value(s->defaults, kGroup, key, nullptr);
}

static gboolean saveNow(gpointer data) {
    auto* s = WENUY_SETTINGS(data);
    s->saveId = 0;
    wenuy_settings_sync(s);
    return G_SOURCE_REMOVE;
}

static void scheduleSave(WenuySettings* s) {
    if (!s->saveId) s->saveId = g_timeout_add(300, saveNow, s);
}

static std::vector<std::string> allKeys(WenuySettings* s) {
    std::set<std::string> keys;
    for (auto [kf, group] : {std::pair{s->defaults, kGroup}, {s->defaults, kLocked}, {s->user, kGroup}}) {
        gsize n = 0;
        gchar** k = g_key_file_get_keys(kf, group, &n, nullptr);
        for (gsize i = 0; k && i < n; ++i) keys.insert(k[i]);
        g_strfreev(k);
    }
    return {keys.begin(), keys.end()};
}

static void emitChanged(WenuySettings* s, const char* key) {
    g_signal_emit(s, setSignals[SET_CHANGED], g_quark_from_string(key), key);
}

// Filen ändrades utifrån (annan instans, `wenuyos`, en texteditor): läs om och meddela skillnader
static gboolean reloadNow(gpointer data) {
    auto* s = WENUY_SETTINGS(data);
    s->reloadId = 0;
    std::vector<std::pair<std::string, std::string>> before;
    for (auto& k : allKeys(s)) {
        gchar* v = rawValue(s, k.c_str());
        before.emplace_back(k, v ? v : "\x01");
        g_free(v);
    }
    GKeyFile* fresh = g_key_file_new();
    g_key_file_load_from_file(fresh, s->path->c_str(), G_KEY_FILE_KEEP_COMMENTS, nullptr);
    g_key_file_free(s->user);
    s->user = fresh;
    std::set<std::string> seen;
    for (auto& [k, old] : before) {
        seen.insert(k);
        gchar* v = rawValue(s, k.c_str());
        if (old != (v ? v : "\x01")) emitChanged(s, k.c_str());
        g_free(v);
    }
    for (auto& k : allKeys(s))
        if (!seen.count(k)) emitChanged(s, k.c_str());
    return G_SOURCE_REMOVE;
}

static void onFileChanged(GFileMonitor*, GFile*, GFile*, GFileMonitorEvent ev, gpointer data) {
    auto* s = WENUY_SETTINGS(data);
    if (ev == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED) return;
    if (s->saveId) return;  // egen osparad ändring vinner
    if (s->reloadId) g_source_remove(s->reloadId);
    s->reloadId = g_timeout_add(120, reloadNow, s);
}

static void wenuy_settings_finalize(GObject* o) {
    auto* s = WENUY_SETTINGS(o);
    if (s->saveId) { g_source_remove(s->saveId); s->saveId = 0; wenuy_settings_sync(s); }
    if (s->reloadId) g_source_remove(s->reloadId);
    if (s->monitor) { g_file_monitor_cancel(s->monitor); g_object_unref(s->monitor); }
    g_key_file_free(s->defaults);
    g_key_file_free(s->user);
    delete s->appId;
    delete s->path;
    G_OBJECT_CLASS(wenuy_settings_parent_class)->finalize(o);
}

static void wenuy_settings_class_init(WenuySettingsClass* c) {
    G_OBJECT_CLASS(c)->finalize = wenuy_settings_finalize;
    setSignals[SET_CHANGED] = g_signal_new("changed", G_TYPE_FROM_CLASS(c),
                                           GSignalFlags(G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED), 0, nullptr, nullptr,
                                           nullptr, G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void wenuy_settings_init(WenuySettings* s) {
    s->appId = new std::string;
    s->path = new std::string;
    s->defaults = g_key_file_new();
    s->user = g_key_file_new();
}

WenuySettings* wenuy_settings_new(const char* app_id) {
    auto* s = WENUY_SETTINGS(g_object_new(WENUY_TYPE_SETTINGS, nullptr));
    *s->appId = app_id ? app_id : "wenuyos";
    mergeInto(s->defaults, "/usr/share/" + *s->appId + "/settings.ini");
    mergeInto(s->defaults, std::string(g_get_user_data_dir()) + "/" + *s->appId + "/settings.ini");  // installerat i ~/.local
    mergeInto(s->defaults, "/etc/xdg/" + *s->appId + "/settings.ini");
    const std::string dir = std::string(g_get_user_config_dir()) + "/" + *s->appId;
    g_mkdir_with_parents(dir.c_str(), 0700);
    *s->path = dir + "/settings.ini";
    g_key_file_load_from_file(s->user, s->path->c_str(), G_KEY_FILE_KEEP_COMMENTS, nullptr);
    GFile* f = g_file_new_for_path(s->path->c_str());
    s->monitor = g_file_monitor_file(f, G_FILE_MONITOR_WATCH_MOVES, nullptr, nullptr);
    g_object_unref(f);
    if (s->monitor) g_signal_connect(s->monitor, "changed", G_CALLBACK(onFileChanged), s);
    return s;
}

char* wenuy_settings_get_string(WenuySettings* s, const char* key, const char* fallback) {
    if (gchar* raw = rawValue(s, key)) {
        // samma tolkning som GKeyFile (escape-sekvenser)
        GKeyFile* k = g_key_file_new();
        g_key_file_set_value(k, kGroup, "v", raw);
        gchar* v = g_key_file_get_string(k, kGroup, "v", nullptr);
        g_key_file_free(k);
        g_free(raw);
        return v;
    }
    return g_strdup(fallback);
}

int wenuy_settings_get_int(WenuySettings* s, const char* key, int fallback) {
    gchar* raw = rawValue(s, key);
    if (!raw) return fallback;
    gchar* end = nullptr;
    const long long v = g_ascii_strtoll(g_strstrip(raw), &end, 10);
    const bool ok = end && *end == 0 && end != raw;
    g_free(raw);
    return ok ? int(v) : fallback;
}

double wenuy_settings_get_double(WenuySettings* s, const char* key, double fallback) {
    gchar* raw = rawValue(s, key);
    if (!raw) return fallback;
    gchar* end = nullptr;
    const double v = g_ascii_strtod(g_strstrip(raw), &end);
    const bool ok = end && *end == 0 && end != raw;
    g_free(raw);
    return ok ? v : fallback;
}

gboolean wenuy_settings_get_boolean(WenuySettings* s, const char* key, gboolean fallback) {
    gchar* raw = rawValue(s, key);
    if (!raw) return fallback;
    g_strstrip(raw);
    gboolean v = fallback;
    if (!g_ascii_strcasecmp(raw, "true") || !strcmp(raw, "1") || !g_ascii_strcasecmp(raw, "yes")) v = TRUE;
    else if (!g_ascii_strcasecmp(raw, "false") || !strcmp(raw, "0") || !g_ascii_strcasecmp(raw, "no")) v = FALSE;
    g_free(raw);
    return v;
}

char** wenuy_settings_get_strv(WenuySettings* s, const char* key) {
    gchar* raw = rawValue(s, key);
    if (!raw) return g_new0(char*, 1);
    GKeyFile* k = g_key_file_new();
    g_key_file_set_value(k, kGroup, "v", raw);
    gchar** v = g_key_file_get_string_list(k, kGroup, "v", nullptr, nullptr);
    g_key_file_free(k);
    g_free(raw);
    return v ? v : g_new0(char*, 1);
}

gboolean wenuy_settings_is_locked(WenuySettings* s, const char* key) {
    return g_key_file_has_key(s->defaults, kLocked, key, nullptr);
}

gboolean wenuy_settings_has_key(WenuySettings* s, const char* key) {
    gchar* v = rawValue(s, key);
    const bool has = v != nullptr;
    g_free(v);
    return has;
}

// Gemensamt för alla set_*: skriv råtext, meddela bara vid verklig ändring
template <typename F>
static gboolean setWith(WenuySettings* s, const char* key, F&& write) {
    if (!key || !*key || wenuy_settings_is_locked(s, key)) return FALSE;
    gchar* before = rawValue(s, key);
    write(s->user);
    gchar* after = rawValue(s, key);
    const bool changed = g_strcmp0(before, after) != 0;
    g_free(before);
    g_free(after);
    if (changed) {
        scheduleSave(s);
        emitChanged(s, key);
    }
    return TRUE;
}

gboolean wenuy_settings_set_string(WenuySettings* s, const char* key, const char* value) {
    return setWith(s, key, [&](GKeyFile* k) { g_key_file_set_string(k, kGroup, key, value ? value : ""); });
}
gboolean wenuy_settings_set_int(WenuySettings* s, const char* key, int value) {
    return setWith(s, key, [&](GKeyFile* k) { g_key_file_set_integer(k, kGroup, key, value); });
}
gboolean wenuy_settings_set_double(WenuySettings* s, const char* key, double value) {
    return setWith(s, key, [&](GKeyFile* k) {
        char buf[G_ASCII_DTOSTR_BUF_SIZE];
        g_key_file_set_value(k, kGroup, key, g_ascii_dtostr(buf, sizeof buf, value));
    });
}
gboolean wenuy_settings_set_boolean(WenuySettings* s, const char* key, gboolean value) {
    return setWith(s, key, [&](GKeyFile* k) { g_key_file_set_boolean(k, kGroup, key, value); });
}
gboolean wenuy_settings_set_strv(WenuySettings* s, const char* key, const char* const* value) {
    return setWith(s, key, [&](GKeyFile* k) {
        g_key_file_set_string_list(k, kGroup, key, value, value ? g_strv_length((gchar**)value) : 0);
    });
}

void wenuy_settings_reset(WenuySettings* s, const char* key) {
    if (!g_key_file_has_key(s->user, kGroup, key, nullptr)) return;
    setWith(s, key, [&](GKeyFile* k) { g_key_file_remove_key(k, kGroup, key, nullptr); });
}

char** wenuy_settings_list_keys(WenuySettings* s) {
    auto keys = allKeys(s);
    char** out = g_new0(char*, keys.size() + 1);
    for (size_t i = 0; i < keys.size(); ++i) out[i] = g_strdup(keys[i].c_str());
    return out;
}

gboolean wenuy_settings_sync(WenuySettings* s) {
    if (s->saveId) { g_source_remove(s->saveId); s->saveId = 0; }
    gsize len = 0;
    gchar* data = g_key_file_to_data(s->user, &len, nullptr);
    GError* err = nullptr;
    const gboolean ok = g_file_set_contents_full(s->path->c_str(), data, len,
                                                 G_FILE_SET_CONTENTS_CONSISTENT, 0600, &err);
    if (!ok) {
        g_warning("wenuy: kunde inte spara %s: %s", s->path->c_str(), err->message);
        g_error_free(err);
    }
    g_free(data);
    return ok;
}

const char* wenuy_settings_get_path(WenuySettings* s) { return s->path->c_str(); }

/* ---- bind: nyckel ⇄ egenskap */
struct SettingsBinding {
    WenuySettings* settings;   // stark referens
    GObject* object;           // svag
    std::string key, property;
    GType type;
    gulong settingsHandler = 0, objectHandler = 0;
    bool busy = false;
};

static void bindingToObject(SettingsBinding* b) {
    if (b->busy || !b->object) return;
    b->busy = true;
    GValue v = G_VALUE_INIT;
    g_value_init(&v, b->type);
    bool ok = true;
    if (!wenuy_settings_has_key(b->settings, b->key.c_str())) ok = false;  // låt widgetens startvärde gälla
    else if (b->type == G_TYPE_BOOLEAN) g_value_set_boolean(&v, wenuy_settings_get_boolean(b->settings, b->key.c_str(), FALSE));
    else if (b->type == G_TYPE_INT) g_value_set_int(&v, wenuy_settings_get_int(b->settings, b->key.c_str(), 0));
    else if (b->type == G_TYPE_UINT) g_value_set_uint(&v, guint(MAX(0, wenuy_settings_get_int(b->settings, b->key.c_str(), 0))));
    else if (b->type == G_TYPE_DOUBLE) g_value_set_double(&v, wenuy_settings_get_double(b->settings, b->key.c_str(), 0));
    else if (b->type == G_TYPE_FLOAT) g_value_set_float(&v, float(wenuy_settings_get_double(b->settings, b->key.c_str(), 0)));
    else if (b->type == G_TYPE_STRING) g_value_take_string(&v, wenuy_settings_get_string(b->settings, b->key.c_str(), ""));
    else ok = false;
    if (ok) g_object_set_property(b->object, b->property.c_str(), &v);
    g_value_unset(&v);
    b->busy = false;
}

static void bindingToSettings(SettingsBinding* b) {
    if (b->busy || !b->object) return;
    b->busy = true;
    GValue v = G_VALUE_INIT;
    g_value_init(&v, b->type);
    g_object_get_property(b->object, b->property.c_str(), &v);
    const char* k = b->key.c_str();
    if (b->type == G_TYPE_BOOLEAN) wenuy_settings_set_boolean(b->settings, k, g_value_get_boolean(&v));
    else if (b->type == G_TYPE_INT) wenuy_settings_set_int(b->settings, k, g_value_get_int(&v));
    else if (b->type == G_TYPE_UINT) wenuy_settings_set_int(b->settings, k, int(g_value_get_uint(&v)));
    else if (b->type == G_TYPE_DOUBLE) wenuy_settings_set_double(b->settings, k, g_value_get_double(&v));
    else if (b->type == G_TYPE_FLOAT) wenuy_settings_set_double(b->settings, k, g_value_get_float(&v));
    else if (b->type == G_TYPE_STRING) wenuy_settings_set_string(b->settings, k, g_value_get_string(&v));
    g_value_unset(&v);
    b->busy = false;
}

static void bindingObjectGone(gpointer data, GObject*) {
    auto* b = static_cast<SettingsBinding*>(data);
    b->object = nullptr;
    if (b->settingsHandler) g_signal_handler_disconnect(b->settings, b->settingsHandler);
    g_object_unref(b->settings);
    delete b;
}

void wenuy_settings_bind(WenuySettings* s, const char* key, gpointer object, const char* property) {
    g_return_if_fail(G_IS_OBJECT(object));
    GParamSpec* ps = g_object_class_find_property(G_OBJECT_GET_CLASS(object), property);
    if (!ps) { g_warning("wenuy_settings_bind: %s saknar egenskapen %s", G_OBJECT_TYPE_NAME(object), property); return; }
    auto* b = new SettingsBinding{WENUY_SETTINGS(g_object_ref(s)), G_OBJECT(object), key, property, ps->value_type};
    if (b->type != G_TYPE_BOOLEAN && b->type != G_TYPE_INT && b->type != G_TYPE_UINT && b->type != G_TYPE_DOUBLE &&
        b->type != G_TYPE_FLOAT && b->type != G_TYPE_STRING) {
        g_warning("wenuy_settings_bind: typen %s stöds inte", g_type_name(b->type));
        g_object_unref(b->settings);
        delete b;
        return;
    }
    bindingToObject(b);
    const std::string detail = "changed::" + b->key;
    b->settingsHandler = g_signal_connect_swapped(s, detail.c_str(), G_CALLBACK(+[](SettingsBinding* bb) { bindingToObject(bb); }), b);
    const std::string notify = "notify::" + b->property;
    b->objectHandler = g_signal_connect_swapped(object, notify.c_str(), G_CALLBACK(+[](SettingsBinding* bb) { bindingToSettings(bb); }), b);
    g_object_weak_ref(G_OBJECT(object), bindingObjectGone, b);
    if (wenuy_settings_is_locked(s, key) && GTK_IS_WIDGET(object)) gtk_widget_set_sensitive(GTK_WIDGET(object), FALSE);
}

/* =================================================================== WenuyRow */
struct _WenuyRow {
    GObject parent;
    std::shared_ptr<std::vector<std::string>>* names;
    std::vector<std::optional<std::string>>* values;
};
G_DEFINE_FINAL_TYPE(WenuyRow, wenuy_row, G_TYPE_OBJECT)

static void wenuy_row_finalize(GObject* o) {
    auto* r = WENUY_ROW(o);
    delete r->names;
    delete r->values;
    G_OBJECT_CLASS(wenuy_row_parent_class)->finalize(o);
}
static void wenuy_row_class_init(WenuyRowClass* c) { G_OBJECT_CLASS(c)->finalize = wenuy_row_finalize; }
static void wenuy_row_init(WenuyRow* r) {
    r->names = new std::shared_ptr<std::vector<std::string>>;
    r->values = new std::vector<std::optional<std::string>>;
}

int wenuy_row_get_n_columns(WenuyRow* r) { return int(r->values->size()); }
const char* wenuy_row_get_column_name(WenuyRow* r, int c) {
    return c >= 0 && *r->names && c < int((*r->names)->size()) ? (**r->names)[c].c_str() : nullptr;
}
const char* wenuy_row_get_string(WenuyRow* r, int c) {
    return c >= 0 && c < int(r->values->size()) && (*r->values)[c] ? (*r->values)[c]->c_str() : nullptr;
}
const char* wenuy_row_get(WenuyRow* r, const char* name) {
    if (!*r->names) return nullptr;
    for (size_t i = 0; i < (*r->names)->size(); ++i)
        if ((**r->names)[i] == name) return wenuy_row_get_string(r, int(i));
    return nullptr;
}
gint64 wenuy_row_get_int(WenuyRow* r, int c) {
    const char* s = wenuy_row_get_string(r, c);
    return s ? g_ascii_strtoll(s, nullptr, 10) : 0;
}
double wenuy_row_get_double(WenuyRow* r, int c) {
    const char* s = wenuy_row_get_string(r, c);
    return s ? g_ascii_strtod(s, nullptr) : 0;
}
gboolean wenuy_row_is_null(WenuyRow* r, int c) { return wenuy_row_get_string(r, c) == nullptr; }

/* =================================================================== WenuyStore */
struct _WenuyStore {
    GObject parent;
    sqlite3* db;
    std::set<std::string>* touched;
    int depth;
};
G_DEFINE_FINAL_TYPE(WenuyStore, wenuy_store, G_TYPE_OBJECT)

enum { STORE_CHANGED, STORE_N };
static guint storeSignals[STORE_N];

static void wenuy_store_finalize(GObject* o) {
    auto* s = WENUY_STORE(o);
    if (s->db) sqlite3_close_v2(s->db);
    delete s->touched;
    G_OBJECT_CLASS(wenuy_store_parent_class)->finalize(o);
}
static void wenuy_store_class_init(WenuyStoreClass* c) {
    G_OBJECT_CLASS(c)->finalize = wenuy_store_finalize;
    storeSignals[STORE_CHANGED] = g_signal_new("changed", G_TYPE_FROM_CLASS(c), G_SIGNAL_RUN_LAST, 0, nullptr,
                                               nullptr, nullptr, G_TYPE_NONE, 1, G_TYPE_STRING);
}
static void wenuy_store_init(WenuyStore* s) { s->touched = new std::set<std::string>; }

static bool fail(WenuyStore* s, GError** error, const char* what) {
    g_set_error(error, storeQuark(), sqlite3_errcode(s->db), "%s: %s", what, sqlite3_errmsg(s->db));
    return false;
}

static void flushChanged(WenuyStore* s) {
    if (s->depth > 0 || s->touched->empty()) return;
    auto tables = *s->touched;
    s->touched->clear();
    for (auto& t : tables)
        if (t != "_kv") g_signal_emit(s, storeSignals[STORE_CHANGED], 0, t.c_str());
}

WenuyStore* wenuy_store_open_path(const char* path, GError** error) {
    auto* s = WENUY_STORE(g_object_new(WENUY_TYPE_STORE, nullptr));
    if (sqlite3_open_v2(path, &s->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
        fail(s, error, path);
        g_object_unref(s);
        return nullptr;
    }
    sqlite3_busy_timeout(s->db, 3000);
    // WAL: läsning medan någon skriver, och säkert vid strömavbrott; NORMAL räcker med WAL
    sqlite3_exec(s->db, "PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    sqlite3_update_hook(s->db, +[](void* d, int, const char*, const char* table, sqlite3_int64) {
        static_cast<WenuyStore*>(d)->touched->insert(table);
    }, s);
    return s;
}

WenuyStore* wenuy_store_open(const char* app_id, const char* name, GError** error) {
    const std::string dir = std::string(g_get_user_data_dir()) + "/" + (app_id ? app_id : "wenuyos");
    g_mkdir_with_parents(dir.c_str(), 0700);
    return wenuy_store_open_path((dir + "/" + (name ? name : "data") + ".db").c_str(), error);
}

// Kör alla satser i sql; ? fylls i ordning från params (även över flera satser)
static int run(WenuyStore* s, const char* sql, const char* const* params, GError** error, GListStore* rows) {
    const char* tail = sql;
    int p = 0, changes = 0;
    while (tail && *tail) {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(s->db, tail, -1, &st, &tail) != SQLITE_OK) { fail(s, error, "SQL"); return -1; }
        if (!st) continue;  // tom sats/kommentar
        const int n = sqlite3_bind_parameter_count(st);
        for (int i = 1; i <= n; ++i) {
            const char* v = params ? params[p] : nullptr;
            if (v) { sqlite3_bind_text(st, i, v, -1, SQLITE_TRANSIENT); ++p; }
            else sqlite3_bind_null(st, i);
        }
        std::shared_ptr<std::vector<std::string>> names;
        int rc;
        while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
            if (!rows) continue;
            const int cols = sqlite3_column_count(st);
            if (!names) {
                names = std::make_shared<std::vector<std::string>>();
                for (int c = 0; c < cols; ++c) names->push_back(sqlite3_column_name(st, c));
            }
            auto* r = WENUY_ROW(g_object_new(WENUY_TYPE_ROW, nullptr));
            *r->names = names;
            for (int c = 0; c < cols; ++c) {
                const unsigned char* t = sqlite3_column_text(st, c);
                if (sqlite3_column_type(st, c) == SQLITE_NULL) r->values->push_back(std::nullopt);
                else r->values->push_back(std::string(reinterpret_cast<const char*>(t), sqlite3_column_bytes(st, c)));
            }
            g_list_store_append(rows, r);
            g_object_unref(r);
        }
        if (rc != SQLITE_DONE) { fail(s, error, "SQL"); sqlite3_finalize(st); return -1; }
        if (!sqlite3_stmt_readonly(st)) changes += sqlite3_changes(s->db);
        sqlite3_finalize(st);
    }
    return changes;
}

int wenuy_store_exec(WenuyStore* s, const char* sql, const char* const* params, GError** error) {
    const int n = run(s, sql, params, error, nullptr);
    flushChanged(s);
    return n;
}

GListModel* wenuy_store_query(WenuyStore* s, const char* sql, const char* const* params, GError** error) {
    GListStore* rows = g_list_store_new(WENUY_TYPE_ROW);
    if (run(s, sql, params, error, rows) < 0) { g_object_unref(rows); return nullptr; }
    flushChanged(s);
    return G_LIST_MODEL(rows);
}

char* wenuy_store_query_value(WenuyStore* s, const char* sql, const char* const* params, GError** error) {
    GListModel* m = wenuy_store_query(s, sql, params, error);
    if (!m) return nullptr;
    char* out = nullptr;
    if (g_list_model_get_n_items(m) > 0) {
        auto* r = WENUY_ROW(g_list_model_get_item(m, 0));
        out = g_strdup(wenuy_row_get_string(r, 0));
        g_object_unref(r);
    }
    g_object_unref(m);
    return out;
}

int wenuy_store_get_version(WenuyStore* s) {
    sqlite3_stmt* st = nullptr;
    int v = 0;
    if (sqlite3_prepare_v2(s->db, "PRAGMA user_version", -1, &st, nullptr) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW)
        v = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return v;
}

gboolean wenuy_store_migrate(WenuyStore* s, int version, const char* sql, GError** error) {
    if (wenuy_store_get_version(s) >= version) return TRUE;
    if (!wenuy_store_begin(s)) return fail(s, error, "BEGIN");
    if (run(s, sql, nullptr, error, nullptr) < 0) { wenuy_store_rollback(s); return FALSE; }
    const std::string pragma = "PRAGMA user_version=" + std::to_string(version);
    sqlite3_exec(s->db, pragma.c_str(), nullptr, nullptr, nullptr);
    return wenuy_store_commit(s);
}

gint64 wenuy_store_last_insert_id(WenuyStore* s) { return sqlite3_last_insert_rowid(s->db); }

gboolean wenuy_store_begin(WenuyStore* s) {
    if (s->depth++ > 0) return TRUE;  // nästlat: en yttre transaktion räcker
    return sqlite3_exec(s->db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK;
}
gboolean wenuy_store_commit(WenuyStore* s) {
    if (s->depth == 0) return FALSE;
    if (--s->depth > 0) return TRUE;
    const bool ok = sqlite3_exec(s->db, "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK;
    flushChanged(s);
    return ok;
}
void wenuy_store_rollback(WenuyStore* s) {
    if (s->depth == 0) return;
    s->depth = 0;
    sqlite3_exec(s->db, "ROLLBACK", nullptr, nullptr, nullptr);
    s->touched->clear();
}

static void ensureKv(WenuyStore* s) {
    sqlite3_exec(s->db, "CREATE TABLE IF NOT EXISTS _kv (k TEXT PRIMARY KEY, v TEXT)", nullptr, nullptr, nullptr);
}
void wenuy_store_set(WenuyStore* s, const char* key, const char* value) {
    ensureKv(s);
    const char* p[] = {key, value, nullptr};
    if (value) wenuy_store_exec(s, "INSERT INTO _kv(k,v) VALUES(?,?) ON CONFLICT(k) DO UPDATE SET v=excluded.v", p, nullptr);
    else wenuy_store_exec(s, "DELETE FROM _kv WHERE k=?", p, nullptr);
}
char* wenuy_store_get(WenuyStore* s, const char* key) {
    ensureKv(s);
    const char* p[] = {key, nullptr};
    return wenuy_store_query_value(s, "SELECT v FROM _kv WHERE k=?", p, nullptr);
}

/* =================================================================== språk */
void wenuy_i18n_init(const char* app_id) { wenuy::i18nInit(app_id); }
const char* wenuy_tr(const char* en, const char* sv) { return wenuy::tr(en, sv ? sv : en); }
