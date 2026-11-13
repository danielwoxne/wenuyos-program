// GObject-skal runt libwenuys C++-delar – se WenuyObjects.h.
#include "WenuyObjects.h"

#include <memory>
#include <string>

#include "WenuyDoc.h"
#include "WenuySheet.h"
#include "WenuyText.h"
#include "WenuyTimeline.h"

/* ================================================================ WenuyDocument */
struct _WenuyDocument {
    GObject parent;
    wenuy::Document* doc;
    std::string* last;   // senaste ögonblicksbild (för checkpoint)
    WenuyDocumentSerializeFunc ser;
    gpointer serData;
    GDestroyNotify serDestroy;
    WenuyDocumentLoadFunc load;
    gpointer loadData;
    GDestroyNotify loadDestroy;
};

G_DEFINE_FINAL_TYPE(WenuyDocument, wenuy_document, G_TYPE_OBJECT)

enum { DOC_CHANGED, DOC_SAVE, DOC_OPEN, DOC_PRINT, DOC_N };
static guint docSig[DOC_N];

static void wenuy_document_init(WenuyDocument* self) {
    self->doc = nullptr;
    self->last = new std::string;
    self->ser = nullptr;
    self->load = nullptr;
    self->serData = self->loadData = nullptr;
    self->serDestroy = self->loadDestroy = nullptr;
}

static void clearFormat(WenuyDocument* self) {
    if (self->serDestroy && self->serData) self->serDestroy(self->serData);
    if (self->loadDestroy && self->loadData) self->loadDestroy(self->loadData);
    self->ser = nullptr;
    self->load = nullptr;
    self->serData = self->loadData = nullptr;
    self->serDestroy = self->loadDestroy = nullptr;
}

static void wenuy_document_finalize(GObject* o) {
    auto* self = WENUY_DOCUMENT(o);
    clearFormat(self);
    delete self->doc;
    delete self->last;
    G_OBJECT_CLASS(wenuy_document_parent_class)->finalize(o);
}

static void wenuy_document_class_init(WenuyDocumentClass* k) {
    G_OBJECT_CLASS(k)->finalize = wenuy_document_finalize;
    docSig[DOC_CHANGED] = g_signal_new("changed", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
    docSig[DOC_SAVE] = g_signal_new("save-requested", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
    docSig[DOC_OPEN] = g_signal_new("open-requested", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
    docSig[DOC_PRINT] = g_signal_new("print-requested", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
}

static std::string serializeNow(WenuyDocument* self) {
    if (!self->doc->serialize) return {};
    return self->doc->serialize();
}

WenuyDocument* wenuy_document_new(const char* app_id) {
    auto* self = WENUY_DOCUMENT(g_object_new(WENUY_TYPE_DOCUMENT, nullptr));
    self->doc = new wenuy::Document(app_id ? app_id : "se.wenuyos.app");
    self->doc->onChanged([self]() { g_signal_emit(self, docSig[DOC_CHANGED], 0); });
    return self;
}

static void installHooks(WenuyDocument* self) {
    self->doc->serialize = [self]() {
        if (!self->ser) return std::string();
        char* s = self->ser(self, self->serData);
        std::string r = s ? s : "";
        g_free(s);
        return r;
    };
    self->doc->deserialize = [self](const std::string& d) { return self->load && self->load(self, d.c_str(), self->loadData); };
}

void wenuy_document_set_serializer(WenuyDocument* self, WenuyDocumentSerializeFunc func, gpointer data, GDestroyNotify destroy) {
    if (self->serDestroy && self->serData) self->serDestroy(self->serData);
    self->ser = func; self->serData = data; self->serDestroy = destroy;
    installHooks(self);
    *self->last = serializeNow(self);
}

void wenuy_document_set_loader(WenuyDocument* self, WenuyDocumentLoadFunc func, gpointer data, GDestroyNotify destroy) {
    if (self->loadDestroy && self->loadData) self->loadDestroy(self->loadData);
    self->load = func; self->loadData = data; self->loadDestroy = destroy;
    installHooks(self);
}

void wenuy_document_set_format(WenuyDocument* self, WenuyDocumentSerializeFunc serialize, gpointer sd, GDestroyNotify sdd,
                               WenuyDocumentLoadFunc load, gpointer ld, GDestroyNotify ldd) {
    wenuy_document_set_loader(self, load, ld, ldd);
    wenuy_document_set_serializer(self, serialize, sd, sdd);
}

static gboolean fail(GError** error, const std::string& msg) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, msg.c_str());
    return FALSE;
}

gboolean wenuy_document_open(WenuyDocument* self, const char* path, GError** error) {
    std::string e;
    if (!self->doc->open(path, &e)) return fail(error, e);
    *self->last = serializeNow(self);
    return TRUE;
}

gboolean wenuy_document_save(WenuyDocument* self, GError** error) {
    std::string e;
    return self->doc->save(&e) ? TRUE : fail(error, e);
}

gboolean wenuy_document_save_as(WenuyDocument* self, const char* path, GError** error) {
    std::string e;
    return self->doc->saveAs(path, &e) ? TRUE : fail(error, e);
}

void wenuy_document_new_empty(WenuyDocument* self) {
    self->doc->newDocument();
    *self->last = serializeNow(self);
}

const char* wenuy_document_get_path(WenuyDocument* self) {
    return self->doc->path().empty() ? nullptr : self->doc->path().c_str();
}

char* wenuy_document_get_title(WenuyDocument* self) { return g_strdup(self->doc->title().c_str()); }
gboolean wenuy_document_is_dirty(WenuyDocument* self) { return self->doc->dirty(); }
void wenuy_document_set_autosave(WenuyDocument* self, guint seconds) { self->doc->setAutosave(seconds); }

void wenuy_document_checkpoint(WenuyDocument* self, const char* label, const char* merge_key) {
    // ögonblicksbilder: enkelt att använda från alla språk (ingen ångra-funktion per ändring)
    const std::string before = *self->last, after = serializeNow(self);
    if (before == after) return;
    *self->last = after;
    auto apply = [self](std::string state) {
        if (self->doc->deserialize) self->doc->deserialize(state);
        *self->last = state;
    };
    self->doc->history.record(label ? label : "", [apply, after]() { apply(after); }, [apply, before]() { apply(before); },
                              merge_key ? merge_key : "");
}

void wenuy_document_undo(WenuyDocument* self) { self->doc->history.undo(); }
void wenuy_document_redo(WenuyDocument* self) { self->doc->history.redo(); }
gboolean wenuy_document_can_undo(WenuyDocument* self) { return self->doc->history.canUndo(); }
gboolean wenuy_document_can_redo(WenuyDocument* self) { return self->doc->history.canRedo(); }

char** wenuy_document_recoverable(const char* app_id) {
    auto list = wenuy::Document::recoverable(app_id);
    char** out = g_new0(char*, list.size() + 1);
    for (size_t i = 0; i < list.size(); ++i) out[i] = g_strdup(list[i].file.c_str());
    return out;
}

gboolean wenuy_document_restore(WenuyDocument* self, const char* file, GError** error) {
    wenuy::Recovery r{file, "", "", 0};
    // originalets sökväg står i .meta bredvid
    std::string meta = std::string(file).substr(0, std::string(file).size() - 5) + ".meta";
    GKeyFile* k = g_key_file_new();
    if (g_key_file_load_from_file(k, meta.c_str(), G_KEY_FILE_NONE, nullptr)) {
        gchar* p = g_key_file_get_string(k, "Recovery", "Path", nullptr);
        r.originalPath = p ? p : "";
        g_free(p);
    }
    g_key_file_free(k);
    std::string e;
    if (!self->doc->restore(r, &e)) return fail(error, e.empty() ? "restore failed" : e);
    *self->last = serializeNow(self);
    return TRUE;
}

void wenuy_document_bind_window(WenuyDocument* self, GtkApplicationWindow* window, const char* app_name) {
    wenuy::bindDocumentActions(window, *self->doc, app_name ? app_name : "",
        [self]() { g_signal_emit(self, docSig[DOC_SAVE], 0); },
        [self]() { g_signal_emit(self, docSig[DOC_OPEN], 0); },
        [self]() { g_signal_emit(self, docSig[DOC_PRINT], 0); });
    g_object_set_data_full(G_OBJECT(window), "wenuy-document-ref", g_object_ref(self), g_object_unref);
}

/* ================================================================ text */
void wenuy_text_setup(GtkTextBuffer* buffer, WenuyDocument* document) {
    wenuy::textTags(buffer);
    if (!document) return;
    wenuy::bindTextHistory(buffer, document->doc->history);
    document->doc->serialize = [buffer]() { return wenuy::textSerialize(buffer); };
    document->doc->deserialize = [buffer](const std::string& d) { return wenuy::textDeserialize(buffer, d); };
    g_object_set_data_full(G_OBJECT(buffer), "wenuy-document-ref", g_object_ref(document), g_object_unref);
}

char* wenuy_text_serialize(GtkTextBuffer* buffer) { return g_strdup(wenuy::textSerialize(buffer).c_str()); }
gboolean wenuy_text_deserialize(GtkTextBuffer* buffer, const char* data) { return wenuy::textDeserialize(buffer, data ? data : ""); }
void wenuy_text_toggle_tag(GtkTextBuffer* buffer, const char* tag) { wenuy::textToggleTag(buffer, tag); }
gboolean wenuy_text_selection_has(GtkTextBuffer* buffer, const char* tag) { return wenuy::textSelectionHas(buffer, tag); }
int wenuy_text_count_pages(GtkTextBuffer* buffer) { return wenuy::textPages(buffer)->count(); }

gboolean wenuy_text_export_pdf(GtkTextBuffer* buffer, const char* path, GError** error) {
    auto pages = wenuy::textPages(buffer);
    std::string e;
    if (wenuy::exportPdf(path, pages->count(), [&](cairo_t* cr, int i, double, double) { pages->draw(cr, i); }, 595, 842, &e))
        return TRUE;
    return fail(error, e);
}

/* ================================================================ WenuySheet */
struct _WenuySheet {
    GObject parent;
    wenuy::Sheet* sheet;
};

G_DEFINE_FINAL_TYPE(WenuySheet, wenuy_sheet, G_TYPE_OBJECT)
static guint sheetChanged;

static void wenuy_sheet_init(WenuySheet* self) { self->sheet = new wenuy::Sheet; }
static void wenuy_sheet_finalize(GObject* o) {
    delete WENUY_SHEET(o)->sheet;
    G_OBJECT_CLASS(wenuy_sheet_parent_class)->finalize(o);
}
static void wenuy_sheet_class_init(WenuySheetClass* k) {
    G_OBJECT_CLASS(k)->finalize = wenuy_sheet_finalize;
    sheetChanged = g_signal_new("changed", G_TYPE_FROM_CLASS(k), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
}

WenuySheet* wenuy_sheet_new(void) { return WENUY_SHEET(g_object_new(WENUY_TYPE_SHEET, nullptr)); }
static void changed(WenuySheet* s) { g_signal_emit(s, sheetChanged, 0); }

void wenuy_sheet_set(WenuySheet* self, int col, int row, const char* input) {
    self->sheet->set({col, row}, input ? input : "");
    changed(self);
}

gboolean wenuy_sheet_set_by_name(WenuySheet* self, const char* cell, const char* input) {
    wenuy::CellRef c;
    if (!cell || !wenuy::parseCellName(cell, &c)) return FALSE;
    wenuy_sheet_set(self, c.col, c.row, input);
    return TRUE;
}

char* wenuy_sheet_get_input(WenuySheet* self, int col, int row) { return g_strdup(self->sheet->input({col, row}).c_str()); }
char* wenuy_sheet_get_display(WenuySheet* self, int col, int row) { return g_strdup(self->sheet->display({col, row}).c_str()); }

double wenuy_sheet_get_number(WenuySheet* self, int col, int row, gboolean* ok) {
    auto v = self->sheet->value({col, row});
    if (ok) *ok = v.kind == wenuy::Value::Number;
    return v.kind == wenuy::Value::Number ? v.num : 0;
}

int wenuy_sheet_get_used_rows(WenuySheet* self) { return self->sheet->usedRows(); }
int wenuy_sheet_get_used_cols(WenuySheet* self) { return self->sheet->usedCols(); }
char* wenuy_sheet_to_csv(WenuySheet* self, char sep) { return g_strdup(self->sheet->toCsv(sep ? sep : ',').c_str()); }
gboolean wenuy_sheet_from_csv(WenuySheet* self, const char* csv) { const bool ok = self->sheet->fromCsv(csv ? csv : ""); changed(self); return ok; }
char* wenuy_sheet_serialize(WenuySheet* self) { return g_strdup(self->sheet->serialize().c_str()); }
gboolean wenuy_sheet_deserialize(WenuySheet* self, const char* d) { const bool ok = self->sheet->deserialize(d ? d : ""); changed(self); return ok; }
void wenuy_sheet_insert_rows(WenuySheet* self, int at, int n) { self->sheet->insertRows(at, n); changed(self); }
void wenuy_sheet_remove_rows(WenuySheet* self, int at, int n) { self->sheet->removeRows(at, n); changed(self); }
void wenuy_sheet_insert_cols(WenuySheet* self, int at, int n) { self->sheet->insertCols(at, n); changed(self); }
void wenuy_sheet_remove_cols(WenuySheet* self, int at, int n) { self->sheet->removeCols(at, n); changed(self); }
char* wenuy_sheet_cell_name(int col, int row) { return g_strdup(wenuy::cellName({col, row}).c_str()); }

/* ================================================================ tidslinje */
struct _WenuyTimelineModel {
    GObject parent;
    wenuy::Timeline* tl;
};

G_DEFINE_FINAL_TYPE(WenuyTimelineModel, wenuy_timeline_model, G_TYPE_OBJECT)

static void wenuy_timeline_model_init(WenuyTimelineModel* self) { self->tl = new wenuy::Timeline; }
static void wenuy_timeline_model_finalize(GObject* o) {
    delete WENUY_TIMELINE_MODEL(o)->tl;
    G_OBJECT_CLASS(wenuy_timeline_model_parent_class)->finalize(o);
}
static void wenuy_timeline_model_class_init(WenuyTimelineModelClass* k) { G_OBJECT_CLASS(k)->finalize = wenuy_timeline_model_finalize; }

WenuyTimelineModel* wenuy_timeline_model_new(void) { return WENUY_TIMELINE_MODEL(g_object_new(WENUY_TYPE_TIMELINE_MODEL, nullptr)); }

void wenuy_timeline_model_add_track(WenuyTimelineModel* self, const char* name, double height) {
    self->tl->tracks.push_back({name ? name : "", height > 0 ? height : 64});
}

int wenuy_timeline_model_add_clip(WenuyTimelineModel* self, int track, double start, double length, const char* label,
                                  const GdkRGBA* color, const char* source) {
    wenuy::Clip c;
    c.track = track;
    c.start = start;
    c.length = length;
    c.label = label ? label : "";
    if (color) c.color = *color;
    c.source = source ? source : "";
    return self->tl->addClip(c);
}

void wenuy_timeline_model_remove_clip(WenuyTimelineModel* self, int id) {
    auto& v = self->tl->clips;
    for (auto it = v.begin(); it != v.end(); ++it)
        if (it->id == id) { v.erase(it); break; }
}

gboolean wenuy_timeline_model_get_clip(WenuyTimelineModel* self, int id, int* track, double* start, double* length,
                                       double* offset, char** source) {
    auto* c = self->tl->find(id);
    if (!c) return FALSE;
    if (track) *track = c->track;
    if (start) *start = c->start;
    if (length) *length = c->length;
    if (offset) *offset = c->offset;
    if (source) *source = g_strdup(c->source.c_str());
    return TRUE;
}

int* wenuy_timeline_model_get_clip_ids(WenuyTimelineModel* self, int* n) {
    *n = (int)self->tl->clips.size();
    int* out = g_new(int, std::max(1, *n));
    for (int i = 0; i < *n; ++i) out[i] = self->tl->clips[i].id;
    return out;
}

double wenuy_timeline_model_get_playhead(WenuyTimelineModel* self) { return self->tl->playhead; }
void wenuy_timeline_model_set_playhead(WenuyTimelineModel* self, double s) { self->tl->playhead = s; }
double wenuy_timeline_model_get_duration(WenuyTimelineModel* self) { return self->tl->duration(); }
char* wenuy_timeline_model_serialize(WenuyTimelineModel* self) { return g_strdup(self->tl->serialize().c_str()); }
gboolean wenuy_timeline_model_deserialize(WenuyTimelineModel* self, const char* d) { return self->tl->deserialize(d ? d : ""); }

GtkWidget* wenuy_timeline_view_new(WenuyTimelineModel* model, WenuyDocument* document) {
    GtkWidget* w = wenuy_timeline_new(model->tl);
    g_object_set_data_full(G_OBJECT(w), "wenuy-model", g_object_ref(model), g_object_unref);
    if (document) {
        wenuy_timeline_set_history(w, &document->doc->history);
        g_object_set_data_full(G_OBJECT(w), "wenuy-document", g_object_ref(document), g_object_unref);
    }
    return w;
}
