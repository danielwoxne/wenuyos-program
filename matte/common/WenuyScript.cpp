// libwenuy – Lua-tillägg i en sandlåda. Se WenuyScript.h.
#include "WenuyScript.h"
#ifndef WENUY_NO_LUA

extern "C" {   // Lua är skrivet i C
#include <lua5.4/lauxlib.h>
#include <lua5.4/lua.h>
#include <lua5.4/lualib.h>
}

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
constexpr size_t kMemLimit = 16u << 20;          // 16 MB för alla tillägg tillsammans
constexpr gint64 kTimeLimit = 500 * 1000;        // 0,5 s per anrop
GQuark scriptQuark() { return g_quark_from_static_string("wenuy-script-error"); }

struct Func { WenuyScriptFunc fn; gpointer data; GDestroyNotify destroy; };
struct Plugin { std::string file, name, description; };
}  // namespace

struct _WenuyScript {
    GObject parent;
    lua_State* L;
    std::string* appId;
    size_t used;
    gint64 deadline;
    std::map<std::string, Func>* funcs;
    std::vector<Plugin>* plugins;
    std::string* current;   // tillägget som laddas just nu
};
G_DEFINE_FINAL_TYPE(WenuyScript, wenuy_script, G_TYPE_OBJECT)

enum { SIG_ERROR, SIG_LOG, SIG_ACTIONS, N_SIGS };
static guint scriptSignals[N_SIGS];

static WenuyScript* self_of(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "wenuy.self");
    auto* s = static_cast<WenuyScript*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return s;
}

// Minnestak: allokeringen nekas när taket nås (Lua ger då ett "not enough memory"-fel)
static void* limitedAlloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    auto* s = static_cast<WenuyScript*>(ud);
    const size_t old = ptr ? osize : 0;
    if (nsize == 0) { s->used -= old; g_free(ptr); return nullptr; }
    if (s->used - old + nsize > kMemLimit) return nullptr;
    void* p = g_try_realloc(ptr, nsize);
    if (p) s->used = s->used - old + nsize;
    return p;
}

// Tidstak: kontrolleras var 10 000:e instruktion
static void timeHook(lua_State* L, lua_Debug*) {
    WenuyScript* s = self_of(L);
    if (s && s->deadline && g_get_monotonic_time() > s->deadline) luaL_error(L, "tidsgränsen (0,5 s) överskreds");
}

static int pcall(WenuyScript* s, int nargs, int nres) {
    s->deadline = g_get_monotonic_time() + kTimeLimit;
    const int rc = lua_pcall(s->L, nargs, nres, 0);
    s->deadline = 0;
    return rc;
}

static void reportError(WenuyScript* s, const std::string& plugin, const char* msg) {
    g_warning("wenuy-script %s: %s", plugin.c_str(), msg);
    g_signal_emit(s, scriptSignals[SIG_ERROR], 0, plugin.c_str(), msg);
}

/* ---- funktioner som tilläggen ser (tabellen wenuy) */
static int l_on(lua_State* L) {   // wenuy.on(hook, fn)
    const char* hook = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_getfield(L, LUA_REGISTRYINDEX, "wenuy.hooks");
    lua_getfield(L, -1, hook);
    if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_newtable(L); lua_pushvalue(L, -1); lua_setfield(L, -3, hook); }
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, luaL_len(L, -2) + 1);
    return 0;
}

static int l_action(lua_State* L) {   // wenuy.action(namn, fn)
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_getfield(L, LUA_REGISTRYINDEX, "wenuy.actions");
    lua_pushvalue(L, 2);
    lua_setfield(L, -2, name);
    g_signal_emit(self_of(L), scriptSignals[SIG_ACTIONS], 0);
    return 0;
}

static int l_log(lua_State* L) {   // print/wenuy.log
    std::string msg;
    for (int i = 1; i <= lua_gettop(L); ++i) {
        if (i > 1) msg += "\t";
        msg += luaL_tolstring(L, i, nullptr);
        lua_pop(L, 1);
    }
    g_message("wenuy-script: %s", msg.c_str());
    g_signal_emit(self_of(L), scriptSignals[SIG_LOG], 0, msg.c_str());
    return 0;
}

static int l_call(lua_State* L) {   // anrop till en funktion som programmet registrerat
    WenuyScript* s = self_of(L);
    const char* name = lua_tostring(L, lua_upvalueindex(1));
    auto it = s->funcs->find(name);
    if (it == s->funcs->end()) return luaL_error(L, "okänd funktion %s", name);
    const int n = lua_gettop(L);
    std::vector<std::string> store;
    for (int i = 1; i <= n; ++i) { store.push_back(luaL_tolstring(L, i, nullptr)); lua_pop(L, 1); }
    std::vector<const char*> args;
    for (auto& a : store) args.push_back(a.c_str());
    args.push_back(nullptr);
    char* r = it->second.fn(args.data(), it->second.data);
    if (r) { lua_pushstring(L, r); g_free(r); } else lua_pushnil(L);
    return 1;
}

static void exposeFunc(WenuyScript* s, const char* name) {
    lua_getglobal(s->L, "wenuy");
    lua_pushstring(s->L, name);
    lua_pushcclosure(s->L, l_call, 1);
    lua_setfield(s->L, -2, name);
    lua_pop(s->L, 1);
}

static void wenuy_script_finalize(GObject* o) {
    auto* s = WENUY_SCRIPT(o);
    if (s->L) lua_close(s->L);
    for (auto& [_, f] : *s->funcs)
        if (f.destroy) f.destroy(f.data);
    delete s->funcs;
    delete s->plugins;
    delete s->appId;
    delete s->current;
    G_OBJECT_CLASS(wenuy_script_parent_class)->finalize(o);
}

static void wenuy_script_class_init(WenuyScriptClass* c) {
    G_OBJECT_CLASS(c)->finalize = wenuy_script_finalize;
    scriptSignals[SIG_ERROR] = g_signal_new("error", G_TYPE_FROM_CLASS(c), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr,
                                            nullptr, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
    scriptSignals[SIG_LOG] = g_signal_new("log", G_TYPE_FROM_CLASS(c), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr,
                                          nullptr, G_TYPE_NONE, 1, G_TYPE_STRING);
    scriptSignals[SIG_ACTIONS] = g_signal_new("actions-changed", G_TYPE_FROM_CLASS(c), G_SIGNAL_RUN_LAST, 0, nullptr,
                                              nullptr, nullptr, G_TYPE_NONE, 0);
}

static void wenuy_script_init(WenuyScript* s) {
    s->appId = new std::string;
    s->funcs = new std::map<std::string, Func>;
    s->plugins = new std::vector<Plugin>;
    s->current = new std::string;
}

WenuyScript* wenuy_script_new(const char* app_id) {
    auto* s = WENUY_SCRIPT(g_object_new(WENUY_TYPE_SCRIPT, nullptr));
    *s->appId = app_id ? app_id : "wenuyos";
    s->L = lua_newstate(limitedAlloc, s);
    lua_State* L = s->L;
    // bara säkra bibliotek: inga filer (io), inget os, ingen require/dofile/loadfile
    const luaL_Reg safe[] = {{LUA_GNAME, luaopen_base}, {LUA_STRLIBNAME, luaopen_string}, {LUA_TABLIBNAME, luaopen_table},
                             {LUA_MATHLIBNAME, luaopen_math}, {LUA_UTF8LIBNAME, luaopen_utf8}, {LUA_COLIBNAME, luaopen_coroutine}};
    for (auto& r : safe) { luaL_requiref(L, r.name, r.func, 1); lua_pop(L, 1); }
    for (const char* bad : {"dofile", "loadfile", "load", "require", "collectgarbage", "rawset", "rawget"}) {
        lua_pushnil(L);
        lua_setglobal(L, bad);
    }
    lua_pushlightuserdata(L, s);
    lua_setfield(L, LUA_REGISTRYINDEX, "wenuy.self");
    lua_newtable(L); lua_setfield(L, LUA_REGISTRYINDEX, "wenuy.hooks");
    lua_newtable(L); lua_setfield(L, LUA_REGISTRYINDEX, "wenuy.actions");
    lua_newtable(L);
    lua_pushcfunction(L, l_on); lua_setfield(L, -2, "on");
    lua_pushcfunction(L, l_action); lua_setfield(L, -2, "action");
    lua_pushcfunction(L, l_log); lua_setfield(L, -2, "log");
    lua_setglobal(L, "wenuy");
    lua_pushcfunction(L, l_log); lua_setglobal(L, "print");
    lua_sethook(L, timeHook, LUA_MASKCOUNT, 10000);
    return s;
}

void wenuy_script_register(WenuyScript* s, const char* name, WenuyScriptFunc func, gpointer data, GDestroyNotify destroy) {
    auto it = s->funcs->find(name);
    if (it != s->funcs->end() && it->second.destroy) it->second.destroy(it->second.data);
    (*s->funcs)[name] = {func, data, destroy};
    exposeFunc(s, name);
}

gboolean wenuy_script_load(WenuyScript* s, const char* path, GError** error) {
    gchar* code = nullptr;
    gsize len = 0;
    if (!g_file_get_contents(path, &code, &len, error)) return FALSE;
    gchar* base = g_path_get_basename(path);
    *s->current = base;
    lua_pushnil(s->L); lua_setglobal(s->L, "plugin");
    bool ok = luaL_loadbufferx(s->L, code, len, base, "t") == LUA_OK && pcall(s, 0, 0) == LUA_OK;   // "t" = bara text, ingen bytekod
    g_free(code);
    if (!ok) {
        const char* msg = lua_tostring(s->L, -1);
        g_set_error(error, scriptQuark(), 1, "%s", msg ? msg : "fel");
        reportError(s, base, msg ? msg : "fel");
        lua_pop(s->L, 1);
        g_free(base);
        return FALSE;
    }
    Plugin p{path, base, ""};
    lua_getglobal(s->L, "plugin");
    if (lua_istable(s->L, -1)) {
        lua_getfield(s->L, -1, "name");
        if (lua_isstring(s->L, -1)) p.name = lua_tostring(s->L, -1);
        lua_pop(s->L, 1);
        lua_getfield(s->L, -1, "description");
        if (lua_isstring(s->L, -1)) p.description = lua_tostring(s->L, -1);
        lua_pop(s->L, 1);
    }
    lua_pop(s->L, 1);
    s->plugins->push_back(p);
    g_free(base);
    return TRUE;
}

int wenuy_script_load_plugins(WenuyScript* s) {
    int n = 0;
    for (std::string dir : {"/usr/share/" + *s->appId + "/plugins",
                            std::string(g_get_user_data_dir()) + "/" + *s->appId + "/plugins"}) {
        GDir* d = g_dir_open(dir.c_str(), 0, nullptr);
        if (!d) continue;
        std::vector<std::string> files;
        while (const char* f = g_dir_read_name(d))
            if (g_str_has_suffix(f, ".lua")) files.push_back(dir + "/" + f);
        g_dir_close(d);
        std::sort(files.begin(), files.end());   // förutsägbar ordning (10-…, 20-…)
        for (auto& f : files)
            if (wenuy_script_load(s, f.c_str(), nullptr)) ++n;
    }
    return n;
}

char* wenuy_script_run(WenuyScript* s, const char* code, GError** error) {
    *s->current = "run";
    if (luaL_loadbufferx(s->L, code, strlen(code), "=run", "t") != LUA_OK || pcall(s, 0, 1) != LUA_OK) {
        g_set_error(error, scriptQuark(), 1, "%s", lua_tostring(s->L, -1));
        lua_pop(s->L, 1);
        return nullptr;
    }
    char* r = lua_isnil(s->L, -1) ? nullptr : g_strdup(luaL_tolstring(s->L, -1, nullptr));
    lua_settop(s->L, 0);
    return r;
}

char* wenuy_script_emit(WenuyScript* s, const char* hook, const char* value) {
    std::string cur = value ? value : "";
    bool has = value != nullptr;
    lua_getfield(s->L, LUA_REGISTRYINDEX, "wenuy.hooks");
    lua_getfield(s->L, -1, hook);
    if (lua_istable(s->L, -1)) {
        const lua_Integer n = luaL_len(s->L, -1);
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_rawgeti(s->L, -1, i);
            if (has) lua_pushlstring(s->L, cur.data(), cur.size()); else lua_pushnil(s->L);
            if (pcall(s, 1, 1) != LUA_OK) {
                reportError(s, std::string("hook ") + hook, lua_tostring(s->L, -1));
            } else if (lua_isstring(s->L, -1)) {   // filter: ny text till nästa tillägg
                size_t len = 0;
                const char* r = lua_tolstring(s->L, -1, &len);
                cur.assign(r, len);
                has = true;
            }
            lua_pop(s->L, 1);
        }
    }
    lua_pop(s->L, 2);
    return has ? g_strndup(cur.data(), cur.size()) : nullptr;
}

char** wenuy_script_list_actions(WenuyScript* s) {
    std::vector<std::string> names;
    lua_getfield(s->L, LUA_REGISTRYINDEX, "wenuy.actions");
    lua_pushnil(s->L);
    while (lua_next(s->L, -2)) {
        if (lua_type(s->L, -2) == LUA_TSTRING) names.push_back(lua_tostring(s->L, -2));
        lua_pop(s->L, 1);
    }
    lua_pop(s->L, 1);
    std::sort(names.begin(), names.end());
    char** out = g_new0(char*, names.size() + 1);
    for (size_t i = 0; i < names.size(); ++i) out[i] = g_strdup(names[i].c_str());
    return out;
}

gboolean wenuy_script_run_action(WenuyScript* s, const char* name) {
    lua_getfield(s->L, LUA_REGISTRYINDEX, "wenuy.actions");
    lua_getfield(s->L, -1, name);
    if (!lua_isfunction(s->L, -1)) { lua_pop(s->L, 2); return FALSE; }
    const bool ok = pcall(s, 0, 0) == LUA_OK;
    if (!ok) { reportError(s, std::string("action ") + name, lua_tostring(s->L, -1)); lua_pop(s->L, 1); }
    lua_pop(s->L, 1);
    return ok;
}

char** wenuy_script_list_plugins(WenuyScript* s) {
    char** out = g_new0(char*, s->plugins->size() + 1);
    for (size_t i = 0; i < s->plugins->size(); ++i) {
        auto& p = (*s->plugins)[i];
        out[i] = g_strdup((p.name + (p.description.empty() ? "" : " – " + p.description)).c_str());
    }
    return out;
}

#else  // WENUY_NO_LUA – byggt utan Lua (t.ex. i en Flatpak): samma gränssnitt, tillägg är avstängda
struct _WenuyScript { GObject parent; };
G_DEFINE_FINAL_TYPE(WenuyScript, wenuy_script, G_TYPE_OBJECT)
static void wenuy_script_class_init(WenuyScriptClass* c) {
    g_signal_new("error", G_TYPE_FROM_CLASS(c), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
    g_signal_new("log", G_TYPE_FROM_CLASS(c), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 1, G_TYPE_STRING);
    g_signal_new("actions-changed", G_TYPE_FROM_CLASS(c), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
}
static void wenuy_script_init(WenuyScript*) {}
static GQuark q() { return g_quark_from_static_string("wenuy-script-error"); }
WenuyScript* wenuy_script_new(const char*) { return WENUY_SCRIPT(g_object_new(WENUY_TYPE_SCRIPT, nullptr)); }
void wenuy_script_register(WenuyScript*, const char*, WenuyScriptFunc, gpointer data, GDestroyNotify destroy) { if (destroy) destroy(data); }
int wenuy_script_load_plugins(WenuyScript*) { return 0; }
gboolean wenuy_script_load(WenuyScript*, const char*, GError** e) { g_set_error(e, q(), 1, "Lua saknas"); return FALSE; }
char* wenuy_script_run(WenuyScript*, const char*, GError** e) { g_set_error(e, q(), 1, "Lua saknas"); return nullptr; }
char* wenuy_script_emit(WenuyScript*, const char*, const char* value) { return g_strdup(value); }
char** wenuy_script_list_actions(WenuyScript*) { return g_new0(char*, 1); }
gboolean wenuy_script_run_action(WenuyScript*, const char*) { return FALSE; }
char** wenuy_script_list_plugins(WenuyScript*) { return g_new0(char*, 1); }
#endif
