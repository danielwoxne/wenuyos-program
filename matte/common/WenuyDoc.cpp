#include "WenuyDoc.h"

#include <adwaita.h>
#include <cairo-pdf.h>
#include <glib/gstdio.h>

#include <cstring>
#include <memory>

#include "Wenuy.h"

namespace wenuy {

// ================================================================ History
void History::perform(const std::string& label, std::function<void()> redo, std::function<void()> undo,
                      const std::string& mergeKey) {
    redo();
    record(label, std::move(redo), std::move(undo), mergeKey);
}

void History::record(const std::string& label, std::function<void()> redo, std::function<void()> undo,
                     const std::string& mergeKey) {
    const gint64 now = g_get_monotonic_time();
    if (pos_ < steps_.size()) {                      // ny gren: framtiden försvinner
        if (savedValid_ && saved_ > pos_) savedValid_ = false;
        steps_.resize(pos_);
    }
    // Slå ihop med föregående steg (samma nyckel, < 1 s, och inte precis efter sparning)
    if (!mergeKey.empty() && pos_ > 0 && steps_[pos_ - 1].key == mergeKey &&
        now - steps_[pos_ - 1].time < G_USEC_PER_SEC && !(savedValid_ && saved_ == pos_)) {
        auto& prev = steps_[pos_ - 1];
        auto firstUndo = std::move(prev.undo);
        auto secondUndo = std::move(undo);
        prev.undo = [firstUndo, secondUndo]() { secondUndo(); firstUndo(); };
        auto firstRedo = std::move(prev.redo);
        auto secondRedo = std::move(redo);
        prev.redo = [firstRedo, secondRedo]() { firstRedo(); secondRedo(); };
        prev.time = now;
        notify();
        return;
    }
    steps_.push_back({label, mergeKey, std::move(redo), std::move(undo), now});
    pos_ = steps_.size();
    if (steps_.size() > limit_) {                    // äldsta steget faller bort
        steps_.erase(steps_.begin());
        --pos_;
        if (saved_ > 0) --saved_; else savedValid_ = false;
    }
    notify();
}

void History::undo() {
    if (!canUndo()) return;
    steps_[--pos_].undo();
    notify();
}

void History::redo() {
    if (!canRedo()) return;
    steps_[pos_++].redo();
    notify();
}

void History::clear() {
    steps_.clear();
    pos_ = saved_ = 0;
    savedValid_ = true;
    notify();
}

// ================================================================ filer
bool atomicWrite(const std::string& path, const std::string& data, std::string* error) {
    GError* e = nullptr;
    // g_file_set_contents skriver till en temporär fil, kör fsync och byter namn
    if (!g_file_set_contents_full(path.c_str(), data.data(), (gssize)data.size(),
                                  G_FILE_SET_CONTENTS_CONSISTENT, 0644, &e)) {
        if (error) *error = e ? e->message : "write failed";
        g_clear_error(&e);
        return false;
    }
    return true;
}

static std::string recoveryDir(const std::string& appId) {
    return std::string(g_get_user_state_dir()) + "/wenuyos/recovery/" + appId;
}

Document::Document(std::string appId) : appId_(std::move(appId)) {
    gchar* u = g_uuid_string_random();
    id_ = u;
    g_free(u);
    history.onChanged([this]() { scheduleAutosave(); });
}

Document::~Document() {
    if (timer_) g_source_remove(timer_);
}

std::string Document::title() const {
    if (path_.empty()) return tr("Untitled", "Namnlös");
    gchar* b = g_path_get_basename(path_.c_str());
    std::string s = b;
    g_free(b);
    return s;
}

void Document::newDocument() {
    discardRecovery();
    path_.clear();
    gchar* u = g_uuid_string_random();
    id_ = u;
    g_free(u);
    history.clear();
}

bool Document::open(const std::string& path, std::string* error) {
    gchar* data = nullptr;
    gsize len = 0;
    GError* e = nullptr;
    if (!g_file_get_contents(path.c_str(), &data, &len, &e)) {
        if (error) *error = e->message;
        g_clear_error(&e);
        return false;
    }
    std::string s(data, len);
    g_free(data);
    if (!deserialize || !deserialize(s)) {
        if (error) *error = tr("The file could not be read", "Filen kunde inte läsas");
        return false;
    }
    discardRecovery();
    path_ = path;
    history.clear();
    return true;
}

bool Document::save(std::string* error) {
    if (path_.empty()) {
        if (error) *error = "no path";
        return false;
    }
    return saveAs(path_, error);
}

bool Document::saveAs(const std::string& path, std::string* error) {
    if (!serialize) return false;
    if (!atomicWrite(path, serialize(), error)) return false;
    path_ = path;
    discardRecovery();
    history.markSaved();
    return true;
}

void Document::setAutosave(guint seconds) {
    autosave_ = seconds;
    if (!seconds && timer_) {
        g_source_remove(timer_);
        timer_ = 0;
    }
}

void Document::scheduleAutosave() {
    if (!autosave_ || !dirty() || timer_) return;
    timer_ = g_timeout_add_seconds(autosave_, +[](gpointer p) -> gboolean {
        auto* d = static_cast<Document*>(p);
        d->timer_ = 0;
        if (d->dirty()) d->writeRecovery();
        return G_SOURCE_REMOVE;
    }, this);
}

std::string Document::recoveryFile() const { return recoveryDir(appId_) + "/" + id_; }

void Document::writeRecovery() {
    if (!serialize) return;
    g_mkdir_with_parents(recoveryDir(appId_).c_str(), 0700);
    if (!atomicWrite(recoveryFile() + ".data", serialize())) return;
    GKeyFile* k = g_key_file_new();
    g_key_file_set_string(k, "Recovery", "Path", path_.c_str());
    g_key_file_set_string(k, "Recovery", "Title", title().c_str());
    g_key_file_save_to_file(k, (recoveryFile() + ".meta").c_str(), nullptr);
    g_key_file_free(k);
}

void Document::discardRecovery() {
    g_unlink((recoveryFile() + ".data").c_str());
    g_unlink((recoveryFile() + ".meta").c_str());
}

std::vector<Recovery> Document::recoverable(const std::string& appId) {
    std::vector<Recovery> out;
    const std::string dir = recoveryDir(appId);
    GDir* d = g_dir_open(dir.c_str(), 0, nullptr);
    if (!d) return out;
    while (const char* n = g_dir_read_name(d)) {
        if (!g_str_has_suffix(n, ".meta")) continue;
        std::string base = dir + "/" + std::string(n, strlen(n) - 5);
        GKeyFile* k = g_key_file_new();
        if (g_key_file_load_from_file(k, (base + ".meta").c_str(), G_KEY_FILE_NONE, nullptr)) {
            gchar* p = g_key_file_get_string(k, "Recovery", "Path", nullptr);
            gchar* t = g_key_file_get_string(k, "Recovery", "Title", nullptr);
            GStatBuf st;
            gint64 m = g_stat((base + ".data").c_str(), &st) == 0 ? (gint64)st.st_mtime : 0;
            if (m) out.push_back({base + ".data", p ? p : "", t ? t : "", m});
            g_free(p);
            g_free(t);
        }
        g_key_file_free(k);
    }
    g_dir_close(d);
    return out;
}

bool Document::restore(const Recovery& r, std::string* error) {
    gchar* data = nullptr;
    gsize len = 0;
    if (!g_file_get_contents(r.file.c_str(), &data, &len, nullptr)) return false;
    std::string s(data, len);
    g_free(data);
    if (!deserialize || !deserialize(s)) {
        if (error) *error = tr("The recovered copy could not be read", "Den återställda kopian kunde inte läsas");
        return false;
    }
    discardRecovery();
    // Ta över återställningsfilen: den nya kopian skrivs med vårt id, den gamla tas bort
    g_unlink(r.file.c_str());
    std::string meta = r.file.substr(0, r.file.size() - 5) + ".meta";
    g_unlink(meta.c_str());
    path_ = r.originalPath;
    history.clear();
    history.record(tr("Recovered", "Återställd"), [] {}, [] {});   // = osparad
    return true;
}

// ================================================================ fönster
struct DocWin {
    Document* doc;
    std::string name;
    std::function<void()> onSave, onOpen, onPrint;
    bool closing = false;
};

static void updateTitle(GtkWindow* w, DocWin* s) {
    std::string t = (s->doc->dirty() ? "• " : "") + s->doc->title() + " – " + s->name;
    gtk_window_set_title(w, t.c_str());
    auto* g = G_ACTION_MAP(w);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(g, "undo")), s->doc->history.canUndo());
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(g, "redo")), s->doc->history.canRedo());
}

void bindDocumentActions(GtkApplicationWindow* win, Document& doc, const std::string& appName,
                         std::function<void()> onSave, std::function<void()> onOpen,
                         std::function<void()> onPrint) {
    auto* s = new DocWin{&doc, appName, std::move(onSave), std::move(onOpen), std::move(onPrint)};
    g_object_set_data_full(G_OBJECT(win), "wenuy-doc", s, +[](gpointer p) { delete static_cast<DocWin*>(p); });

    struct A { const char* name; std::function<void(DocWin*)> fn; };
    const A acts[] = {
        {"undo", [](DocWin* s) { s->doc->history.undo(); }},
        {"redo", [](DocWin* s) { s->doc->history.redo(); }},
        {"save", [](DocWin* s) { if (s->onSave) s->onSave(); }},
        {"open", [](DocWin* s) { if (s->onOpen) s->onOpen(); }},
        {"print", [](DocWin* s) { if (s->onPrint) s->onPrint(); }},
    };
    for (const auto& a : acts) {
        auto* act = g_simple_action_new(a.name, nullptr);
        auto* fn = new std::function<void(DocWin*)>(a.fn);
        g_object_set_data_full(G_OBJECT(act), "fn", fn, +[](gpointer p) { delete static_cast<std::function<void(DocWin*)>*>(p); });
        g_signal_connect(act, "activate", CB(+[](GSimpleAction* a, GVariant*, gpointer w) {
            auto* f = static_cast<std::function<void(DocWin*)>*>(g_object_get_data(G_OBJECT(a), "fn"));
            (*f)(static_cast<DocWin*>(g_object_get_data(G_OBJECT(w), "wenuy-doc")));
        }), win);
        g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(act));
        g_object_unref(act);
    }
    if (auto* app = gtk_window_get_application(GTK_WINDOW(win))) {
        const char* undo[] = {"<Control>z", nullptr};
        const char* redo[] = {"<Control><Shift>z", "<Control>y", nullptr};
        const char* save[] = {"<Control>s", nullptr};
        const char* open[] = {"<Control>o", nullptr};
        const char* prnt[] = {"<Control>p", nullptr};
        gtk_application_set_accels_for_action(app, "win.undo", undo);
        gtk_application_set_accels_for_action(app, "win.redo", redo);
        gtk_application_set_accels_for_action(app, "win.save", save);
        gtk_application_set_accels_for_action(app, "win.open", open);
        gtk_application_set_accels_for_action(app, "win.print", prnt);
    }
    doc.onChanged([win, s]() { updateTitle(GTK_WINDOW(win), s); });
    updateTitle(GTK_WINDOW(win), s);

    // Osparade ändringar vid stängning
    g_signal_connect(win, "close-request", CB(+[](GtkWindow* w, gpointer) -> gboolean {
        auto* s = static_cast<DocWin*>(g_object_get_data(G_OBJECT(w), "wenuy-doc"));
        if (s->closing || !s->doc->dirty()) { s->doc->discardRecovery(); return FALSE; }
        auto* d = adw_alert_dialog_new(tr("Save changes?", "Spara ändringarna?"), nullptr);
        adw_alert_dialog_format_body(ADW_ALERT_DIALOG(d), "%s",
            (std::string(tr("“", "”")) + s->doc->title() + tr("” has unsaved changes.", "” har osparade ändringar.")).c_str());
        adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(d), "cancel", tr("Cancel", "Avbryt"),
                                       "discard", tr("Don't save", "Spara inte"), "save", tr("Save", "Spara"), nullptr);
        adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(d), "discard", ADW_RESPONSE_DESTRUCTIVE);
        adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(d), "save", ADW_RESPONSE_SUGGESTED);
        adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(d), "save");
        adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(d), "cancel");
        g_signal_connect(d, "response", CB(+[](AdwAlertDialog*, const char* r, gpointer w) {
            auto* s = static_cast<DocWin*>(g_object_get_data(G_OBJECT(w), "wenuy-doc"));
            if (g_str_equal(r, "save") && s->onSave) {
                s->onSave();
                if (s->doc->dirty()) return;    // användaren avbröt "Spara som"
            } else if (!g_str_equal(r, "discard")) {
                return;
            }
            s->closing = true;
            gtk_window_close(GTK_WINDOW(w));
        }), w);
        adw_dialog_present(ADW_DIALOG(d), GTK_WIDGET(w));
        return TRUE;
    }), nullptr);
}

// ================================================================ PDF och utskrift
bool exportPdf(const std::string& path, int pages, const PageDrawer& draw, double width, double height,
               std::string* error) {
    std::string tmp = path + ".part";
    cairo_surface_t* surf = cairo_pdf_surface_create(tmp.c_str(), width, height);
    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        if (error) *error = cairo_status_to_string(cairo_surface_status(surf));
        cairo_surface_destroy(surf);
        return false;
    }
    cairo_pdf_surface_set_metadata(surf, CAIRO_PDF_METADATA_CREATOR, "WenuyOS");
    cairo_t* cr = cairo_create(surf);
    for (int i = 0; i < pages; ++i) {
        cairo_save(cr);
        draw(cr, i, width, height);
        cairo_restore(cr);
        cairo_show_page(cr);
    }
    cairo_destroy(cr);
    cairo_surface_finish(surf);
    const bool ok = cairo_surface_status(surf) == CAIRO_STATUS_SUCCESS;
    cairo_surface_destroy(surf);
    if (!ok || g_rename(tmp.c_str(), path.c_str()) != 0) {
        g_unlink(tmp.c_str());
        if (error) *error = tr("Could not write the PDF", "Kunde inte skriva PDF-filen");
        return false;
    }
    return true;
}

void print(GtkWindow* parent, int pages, PageDrawer draw) {
    auto* op = gtk_print_operation_new();
    gtk_print_operation_set_n_pages(op, pages);
    gtk_print_operation_set_unit(op, GTK_UNIT_POINTS);
    gtk_print_operation_set_embed_page_setup(op, TRUE);
    auto* fn = new PageDrawer(std::move(draw));
    g_object_set_data_full(G_OBJECT(op), "draw", fn, +[](gpointer p) { delete static_cast<PageDrawer*>(p); });
    g_signal_connect(op, "draw-page", CB(+[](GtkPrintOperation* op, GtkPrintContext* ctx, int page, gpointer) {
        auto* f = static_cast<PageDrawer*>(g_object_get_data(G_OBJECT(op), "draw"));
        (*f)(gtk_print_context_get_cairo_context(ctx), page, gtk_print_context_get_width(ctx),
             gtk_print_context_get_height(ctx));
    }), nullptr);
    gtk_print_operation_run(op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, parent, nullptr);
    g_object_unref(op);
}

}  // namespace wenuy
