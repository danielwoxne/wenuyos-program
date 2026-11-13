// libwenuy – bakgrundsjobb (WenuyJob) och aviseringar (wenuy_notify). Se WenuyJobs.h.
// Jobben skrivs som statusfiler i $XDG_RUNTIME_DIR/wenuyos/transfers – samma mapp som
// överföringarna, så panelen visar båda i samma lista (Kind=job).
#include "WenuyJobs.h"

#include <gio/gunixfdlist.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>

namespace {
bool sv() {
    const char* l = g_getenv("LANG");
    return l && g_str_has_prefix(l, "sv");
}
const char* tr(const char* en, const char* s) { return sv() ? s : en; }
std::string statusDir() {
    const std::string d = std::string(g_get_user_runtime_dir()) + "/wenuyos/transfers";
    g_mkdir_with_parents(d.c_str(), 0700);
    return d;
}
}  // namespace

struct _WenuyJob {
    GObject parent;
    std::string* id;
    std::string* title;
    std::string* text;
    std::mutex* lock;
    double fraction;
    int state;            // 0 pågår, 1 klar, 2 misslyckades, 3 avbruten
    gint64 started, lastWrite;
    std::atomic<bool>* cancelled;
    GCancellable* cancellable;
    guint pollId;
    int inhibitFd;
    bool held, notifyAlways;
};
G_DEFINE_FINAL_TYPE(WenuyJob, wenuy_job, G_TYPE_OBJECT)

enum { SIG_CANCELLED, N_SIGS };
static guint jobSignals[N_SIGS];

static void writeStatus(WenuyJob* j) {
    GKeyFile* k = g_key_file_new();
    g_key_file_set_string(k, "Transfer", "Kind", "job");
    g_key_file_set_string(k, "Transfer", "Name", j->title->c_str());
    g_key_file_set_string(k, "Transfer", "Text", j->text->c_str());
    // panelen räknar procent av Done/Total; okänt förlopp = Total 0
    g_key_file_set_uint64(k, "Transfer", "Done", j->fraction >= 0 ? guint64(j->fraction * 1000) : 0);
    g_key_file_set_uint64(k, "Transfer", "Total", j->fraction >= 0 ? 1000 : 0);
    g_key_file_set_integer(k, "Transfer", "State", j->state);
    g_key_file_set_integer(k, "Transfer", "Pid", int(getpid()));
    gsize len = 0;
    gchar* data = g_key_file_to_data(k, &len, nullptr);
    const std::string p = statusDir() + "/" + *j->id;
    g_file_set_contents((p + ".tmp").c_str(), data, gssize(len), nullptr);
    g_rename((p + ".tmp").c_str(), p.c_str());
    g_free(data);
    g_key_file_free(k);
}

static void markCancelled(WenuyJob* j) {
    if (j->cancelled->exchange(true)) return;
    g_cancellable_cancel(j->cancellable);
    // signalen alltid i huvudtråden
    g_main_context_invoke(nullptr, +[](gpointer p) -> gboolean {
        auto* jj = WENUY_JOB(p);
        g_signal_emit(jj, jobSignals[SIG_CANCELLED], 0);
        g_object_unref(jj);
        return G_SOURCE_REMOVE;
    }, g_object_ref(j));
}

static bool cancelFileExists(WenuyJob* j) {
    return g_file_test((statusDir() + "/" + *j->id + ".cancel").c_str(), G_FILE_TEST_EXISTS);
}

static void wenuy_job_dispose(GObject* o) {
    auto* j = WENUY_JOB(o);
    if (j->state == 0) { j->cancelled->store(true); wenuy_job_finish(j, FALSE, nullptr); }  // släppt utan finish: städa tyst
    G_OBJECT_CLASS(wenuy_job_parent_class)->dispose(o);
}

static void wenuy_job_finalize(GObject* o) {
    auto* j = WENUY_JOB(o);
    if (j->pollId) g_source_remove(j->pollId);
    g_clear_object(&j->cancellable);
    delete j->id;
    delete j->title;
    delete j->text;
    delete j->lock;
    delete j->cancelled;
    G_OBJECT_CLASS(wenuy_job_parent_class)->finalize(o);
}

static void wenuy_job_class_init(WenuyJobClass* c) {
    G_OBJECT_CLASS(c)->dispose = wenuy_job_dispose;
    G_OBJECT_CLASS(c)->finalize = wenuy_job_finalize;
    jobSignals[SIG_CANCELLED] = g_signal_new("cancelled", G_TYPE_FROM_CLASS(c), G_SIGNAL_RUN_LAST, 0, nullptr,
                                             nullptr, nullptr, G_TYPE_NONE, 0);
}

static void wenuy_job_init(WenuyJob* j) {
    j->id = new std::string;
    j->title = new std::string;
    j->text = new std::string;
    j->lock = new std::mutex;
    j->cancelled = new std::atomic<bool>(false);
    j->cancellable = g_cancellable_new();
    j->fraction = -1;
    j->inhibitFd = -1;
}

// Hindra viloläge medan jobbet pågår (logind; tyst om det inte går)
static void inhibitSleep(WenuyJob* j) {
    GDBusConnection* sys = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, nullptr);
    if (!sys) return;
    const char* who = g_get_application_name() ? g_get_application_name() : g_get_prgname();
    g_dbus_connection_call_with_unix_fd_list(
        sys, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Inhibit",
        g_variant_new("(ssss)", "sleep:idle", who ? who : "WenuyOS", j->title->c_str(), "block"),
        G_VARIANT_TYPE("(h)"), G_DBUS_CALL_FLAGS_NONE, 2000, nullptr, nullptr,
        +[](GObject* src, GAsyncResult* res, gpointer p) {
            auto* jj = WENUY_JOB(p);
            GUnixFDList* fds = nullptr;
            GVariant* r = g_dbus_connection_call_with_unix_fd_list_finish(G_DBUS_CONNECTION(src), &fds, res, nullptr);
            if (r && fds) {
                gint32 h = 0;
                g_variant_get(r, "(h)", &h);
                const int fd = g_unix_fd_list_get(fds, h, nullptr);
                if (jj->state == 0) jj->inhibitFd = fd;
                else if (fd >= 0) close(fd);   // jobbet hann bli klart
            }
            if (r) g_variant_unref(r);
            g_clear_object(&fds);
            g_object_unref(jj);
        },
        g_object_ref(j));
    g_object_unref(sys);
}

WenuyJob* wenuy_job_new(const char* title) {
    auto* j = WENUY_JOB(g_object_new(WENUY_TYPE_JOB, nullptr));
    static std::atomic<int> counter{0};
    *j->id = "job-" + std::to_string(getpid()) + "-" + std::to_string(++counter);
    *j->title = title ? title : "";
    j->started = g_get_monotonic_time();
    writeStatus(j);
    // Avbryt från panelen märks även om programmet inte anropar set_progress
    j->pollId = g_timeout_add_seconds(1, +[](gpointer p) -> gboolean {
        auto* jj = WENUY_JOB(p);
        if (jj->state != 0) { jj->pollId = 0; return G_SOURCE_REMOVE; }
        if (cancelFileExists(jj)) markCancelled(jj);
        return G_SOURCE_CONTINUE;
    }, j);
    if (GApplication* app = g_application_get_default()) {   // programmet lever vidare om fönstret stängs
        g_application_hold(app);
        j->held = true;
    }
    inhibitSleep(j);
    return j;
}

void wenuy_job_set_progress(WenuyJob* j, double fraction, const char* text) {
    std::lock_guard<std::mutex> g(*j->lock);
    if (j->state != 0) return;
    j->fraction = fraction < 0 ? -1 : MIN(fraction, 1.0);
    if (text) *j->text = text;
    const gint64 now = g_get_monotonic_time();
    if (now - j->lastWrite < 200000) return;
    j->lastWrite = now;
    writeStatus(j);
    if (cancelFileExists(j)) markCancelled(j);
}

gboolean wenuy_job_is_cancelled(WenuyJob* j) { return j->cancelled->load(); }
void wenuy_job_cancel(WenuyJob* j) { markCancelled(j); }
GCancellable* wenuy_job_get_cancellable(WenuyJob* j) { return j->cancellable; }
void wenuy_job_set_notify(WenuyJob* j, gboolean always) { j->notifyAlways = always; }

void wenuy_job_finish(WenuyJob* j, gboolean success, const char* message) {
    {
        std::lock_guard<std::mutex> g(*j->lock);
        if (j->state != 0) return;
        j->state = j->cancelled->load() ? 3 : success ? 1 : 2;
        if (j->state == 1) j->fraction = 1;
        if (message) *j->text = message;
        writeStatus(j);
    }
    if (j->inhibitFd >= 0) { close(j->inhibitFd); j->inhibitFd = -1; }
    const std::string id = *j->id;
    g_timeout_add_seconds(8, +[](gpointer p) -> gboolean {
        auto* s = static_cast<std::string*>(p);
        g_unlink((statusDir() + "/" + *s).c_str());
        g_unlink((statusDir() + "/" + *s + ".cancel").c_str());
        delete s;
        return G_SOURCE_REMOVE;
    }, new std::string(id));
    const bool longJob = g_get_monotonic_time() - j->started > 5 * G_USEC_PER_SEC;
    if (j->state == 2 || (j->state == 1 && (longJob || j->notifyAlways))) {
        const std::string title = std::string(j->state == 1 ? tr("Finished: ", "Klart: ") : tr("Failed: ", "Misslyckades: ")) + *j->title;
        wenuy_notify(title.c_str(), message, j->state == 1 ? "emblem-ok-symbolic" : "dialog-warning", FALSE);
    }
    if (j->held) {
        j->held = false;
        g_main_context_invoke(nullptr, +[](gpointer) -> gboolean {
            if (GApplication* app = g_application_get_default()) g_application_release(app);
            return G_SOURCE_REMOVE;
        }, nullptr);
    }
}

/* ------------------------------------------------------------------ aviseringar */
static std::mutex gNotifyLock;
static std::map<std::string, std::pair<guint, gint64>> gRecent;   // rubrik → (id, tid)

guint wenuy_notify(const char* title, const char* body, const char* icon, gboolean urgent) {
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!bus || !title) { g_clear_object(&bus); return 0; }
    const gint64 now = g_get_monotonic_time();
    guint replaces = 0;
    {
        std::lock_guard<std::mutex> g(gNotifyLock);
        auto it = gRecent.find(title);
        if (it != gRecent.end() && now - it->second.second < 10 * G_USEC_PER_SEC) replaces = it->second.first;
    }
    GVariantBuilder hints;
    g_variant_builder_init(&hints, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&hints, "{sv}", "urgency", g_variant_new_byte(urgent ? 2 : 1));
    if (const char* id = g_application_get_default() ? g_application_get_application_id(g_application_get_default()) : nullptr)
        g_variant_builder_add(&hints, "{sv}", "desktop-entry", g_variant_new_string(id));
    const char* app = g_get_application_name() ? g_get_application_name() : (g_get_prgname() ? g_get_prgname() : "WenuyOS");
    GVariant* r = g_dbus_connection_call_sync(
        bus, "org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify",
        g_variant_new("(susssasa{sv}i)", app, replaces, icon ? icon : "", title, body ? body : "", nullptr, &hints, -1),
        G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, 1500, nullptr, nullptr);
    guint id = 0;
    if (r) {
        g_variant_get(r, "(u)", &id);
        g_variant_unref(r);
        std::lock_guard<std::mutex> g(gNotifyLock);
        gRecent[title] = {id, now};
    }
    g_object_unref(bus);
    return id;
}

void wenuy_notify_close(guint id) {
    if (!id) return;
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!bus) return;
    g_dbus_connection_call(bus, "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                           "org.freedesktop.Notifications", "CloseNotification", g_variant_new("(u)", id), nullptr,
                           G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);
    g_object_unref(bus);
}
