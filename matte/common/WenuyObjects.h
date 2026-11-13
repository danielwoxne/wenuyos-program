// libwenuy – GObject-skal runt C++-delarna, så att de kan användas från alla språk
// (Python, JavaScript/GJS, Rust, Vala, Lua …) via GObject Introspection (Wenuy-0.typelib).
//
//   WenuyDocument   spara säkert, osparad-status, autosparning + återställning, ångra/gör om
//   WenuySheet      kalkylmotorn: formler, beroenden, CSV, rader/kolumner
//   wenuy_text_*    formaterad text i en GtkTextBuffer, filformat, PDF
//   WenuyTimelineModel + wenuy_timeline_view_new()   tidslinje för ljud/video
//
// C++-program kan fortsätta använda wenuy::Document m.fl. direkt – det här är samma kod.
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* ------------------------------------------------------------------ WenuyDocument */
#define WENUY_TYPE_DOCUMENT (wenuy_document_get_type())
G_DECLARE_FINAL_TYPE(WenuyDocument, wenuy_document, WENUY, DOCUMENT, GObject)

/**
 * WenuyDocumentSerializeFunc:
 * @document: dokumentet
 * @user_data: (closure): data
 *
 * Returns: (transfer full): dokumentets innehåll som text
 */
typedef char* (*WenuyDocumentSerializeFunc)(WenuyDocument* document, gpointer user_data);

/**
 * WenuyDocumentLoadFunc:
 * @document: dokumentet
 * @data: innehållet som ska läsas in
 * @user_data: (closure): data
 *
 * Returns: %TRUE om innehållet kunde läsas
 */
typedef gboolean (*WenuyDocumentLoadFunc)(WenuyDocument* document, const char* data, gpointer user_data);

/**
 * wenuy_document_new:
 * @app_id: programmets id, t.ex. "se.exempel.Skrivare" (för återställningsmappen)
 *
 * Returns: (transfer full): ett nytt dokument
 */
WenuyDocument* wenuy_document_new(const char* app_id);

/**
 * wenuy_document_set_serializer:
 * @self: dokumentet
 * @func: (scope notified) (closure data) (destroy destroy): dokument → text (för spara, autosparning, ångra)
 * @data: data
 * @destroy: frigör data
 */
void wenuy_document_set_serializer(WenuyDocument* self, WenuyDocumentSerializeFunc func, gpointer data, GDestroyNotify destroy);
/**
 * wenuy_document_set_loader:
 * @self: dokumentet
 * @func: (scope notified) (closure data) (destroy destroy): text → dokument (för öppna, återställ, ångra)
 * @data: data
 * @destroy: frigör data
 */
void wenuy_document_set_loader(WenuyDocument* self, WenuyDocumentLoadFunc func, gpointer data, GDestroyNotify destroy);
/**
 * wenuy_document_set_format: (skip)
 * Båda på en gång (för C/C++).
 */
void wenuy_document_set_format(WenuyDocument* self,
                               WenuyDocumentSerializeFunc serialize, gpointer serialize_data, GDestroyNotify serialize_destroy,
                               WenuyDocumentLoadFunc load, gpointer load_data, GDestroyNotify load_destroy);

gboolean wenuy_document_open(WenuyDocument* self, const char* path, GError** error);
gboolean wenuy_document_save(WenuyDocument* self, GError** error);
gboolean wenuy_document_save_as(WenuyDocument* self, const char* path, GError** error);
void wenuy_document_new_empty(WenuyDocument* self);

/**
 * wenuy_document_get_path:
 * Returns: (nullable): sökvägen, eller %NULL för ett nytt dokument
 */
const char* wenuy_document_get_path(WenuyDocument* self);
/**
 * wenuy_document_get_title:
 * Returns: (transfer full): filnamnet eller "Namnlös"
 */
char* wenuy_document_get_title(WenuyDocument* self);
gboolean wenuy_document_is_dirty(WenuyDocument* self);
void wenuy_document_set_autosave(WenuyDocument* self, guint seconds);

/* Ångra/gör om med ögonblicksbilder: anropa checkpoint() efter varje ändring.
 * merge_key (valfri): snabba ändringar med samma nyckel blir ett steg (t.ex. "typing"). */
void wenuy_document_checkpoint(WenuyDocument* self, const char* label, const char* merge_key);
void wenuy_document_undo(WenuyDocument* self);
void wenuy_document_redo(WenuyDocument* self);
gboolean wenuy_document_can_undo(WenuyDocument* self);
gboolean wenuy_document_can_redo(WenuyDocument* self);

/**
 * wenuy_document_recoverable:
 * @app_id: programmets id
 *
 * Returns: (transfer full) (array zero-terminated=1): sökvägar till osparade versioner
 */
char** wenuy_document_recoverable(const char* app_id);
gboolean wenuy_document_restore(WenuyDocument* self, const char* recovery_file, GError** error);

/**
 * wenuy_document_bind_window:
 * @self: dokumentet
 * @window: fönstret
 * @app_name: programmets namn (i titeln)
 *
 * Ctrl+Z/Ctrl+Shift+Z, "• osparad" i titeln och frågan "Spara ändringarna?" vid stängning.
 * Signalerna "save-requested", "open-requested" och "print-requested" skickas för Ctrl+S/O/P.
 */
void wenuy_document_bind_window(WenuyDocument* self, GtkApplicationWindow* window, const char* app_name);
/* Signal "changed" (): något ändrades (ny ändring, ångra, sparat). */

/* ------------------------------------------------------------------ text */
/**
 * wenuy_text_setup:
 * @buffer: textbufferten
 * @document: (nullable): dokument vars ångra/gör om ska omfatta texten (även formatering)
 *
 * Skapar formaten bold, italic, underline, strike, code, h1, h2, quote, center.
 * Med @document kopplas texten till dokumentet: spara/öppna använder .wdoc-formatet.
 */
void wenuy_text_setup(GtkTextBuffer* buffer, WenuyDocument* document);
/**
 * wenuy_text_serialize:
 * Returns: (transfer full): texten i .wdoc-format
 */
char* wenuy_text_serialize(GtkTextBuffer* buffer);
gboolean wenuy_text_deserialize(GtkTextBuffer* buffer, const char* data);
void wenuy_text_toggle_tag(GtkTextBuffer* buffer, const char* tag);
gboolean wenuy_text_selection_has(GtkTextBuffer* buffer, const char* tag);
gboolean wenuy_text_export_pdf(GtkTextBuffer* buffer, const char* path, GError** error);
int wenuy_text_count_pages(GtkTextBuffer* buffer);

/* ------------------------------------------------------------------ WenuySheet */
#define WENUY_TYPE_SHEET (wenuy_sheet_get_type())
G_DECLARE_FINAL_TYPE(WenuySheet, wenuy_sheet, WENUY, SHEET, GObject)
/**
 * wenuy_sheet_new:
 * Returns: (transfer full): ett tomt kalkylblad
 */
WenuySheet* wenuy_sheet_new(void);
void wenuy_sheet_set(WenuySheet* self, int col, int row, const char* input);
/**
 * wenuy_sheet_set_by_name:
 * @cell: t.ex. "B3"
 */
gboolean wenuy_sheet_set_by_name(WenuySheet* self, const char* cell, const char* input);
/**
 * wenuy_sheet_get_input:
 * Returns: (transfer full): det som skrevs i cellen
 */
char* wenuy_sheet_get_input(WenuySheet* self, int col, int row);
/**
 * wenuy_sheet_get_display:
 * Returns: (transfer full): cellens värde som det visas (t.ex. "48,5" eller "#DIV/0!")
 */
char* wenuy_sheet_get_display(WenuySheet* self, int col, int row);
/**
 * wenuy_sheet_get_number:
 * @ok: (out) (optional): %TRUE om cellen är ett tal
 */
double wenuy_sheet_get_number(WenuySheet* self, int col, int row, gboolean* ok);
int wenuy_sheet_get_used_rows(WenuySheet* self);
int wenuy_sheet_get_used_cols(WenuySheet* self);
/**
 * wenuy_sheet_to_csv:
 * @separator: ',' eller ';'
 * Returns: (transfer full): CSV med beräknade värden
 */
char* wenuy_sheet_to_csv(WenuySheet* self, char separator);
gboolean wenuy_sheet_from_csv(WenuySheet* self, const char* csv);
/**
 * wenuy_sheet_serialize:
 * Returns: (transfer full): .wsheet-text (formler bevaras)
 */
char* wenuy_sheet_serialize(WenuySheet* self);
gboolean wenuy_sheet_deserialize(WenuySheet* self, const char* data);
void wenuy_sheet_insert_rows(WenuySheet* self, int at, int count);
void wenuy_sheet_remove_rows(WenuySheet* self, int at, int count);
void wenuy_sheet_insert_cols(WenuySheet* self, int at, int count);
void wenuy_sheet_remove_cols(WenuySheet* self, int at, int count);
/**
 * wenuy_sheet_cell_name:
 * Returns: (transfer full): t.ex. "B3" för (1, 2)
 */
char* wenuy_sheet_cell_name(int col, int row);
/* Signal "changed" (). */

/* ------------------------------------------------------------------ tidslinje */
#define WENUY_TYPE_TIMELINE_MODEL (wenuy_timeline_model_get_type())
G_DECLARE_FINAL_TYPE(WenuyTimelineModel, wenuy_timeline_model, WENUY, TIMELINE_MODEL, GObject)
/**
 * wenuy_timeline_model_new:
 * Returns: (transfer full): en tom tidslinje
 */
WenuyTimelineModel* wenuy_timeline_model_new(void);
void wenuy_timeline_model_add_track(WenuyTimelineModel* self, const char* name, double height);
/**
 * wenuy_timeline_model_add_clip:
 * @color: (nullable): klippets färg
 * Returns: klippets id
 */
int wenuy_timeline_model_add_clip(WenuyTimelineModel* self, int track, double start, double length,
                                  const char* label, const GdkRGBA* color, const char* source);
void wenuy_timeline_model_remove_clip(WenuyTimelineModel* self, int id);
/**
 * wenuy_timeline_model_get_clip:
 * @track: (out) (optional): spår
 * @start: (out) (optional): start i sekunder
 * @length: (out) (optional): längd i sekunder
 * @offset: (out) (optional): början i källan
 * @source: (out) (optional) (transfer full): källa (fil)
 * Returns: %TRUE om klippet finns
 */
gboolean wenuy_timeline_model_get_clip(WenuyTimelineModel* self, int id, int* track, double* start, double* length,
                                       double* offset, char** source);
/**
 * wenuy_timeline_model_get_clip_ids:
 * @n: (out): antal
 * Returns: (transfer full) (array length=n): alla klipps id
 */
int* wenuy_timeline_model_get_clip_ids(WenuyTimelineModel* self, int* n);
double wenuy_timeline_model_get_playhead(WenuyTimelineModel* self);
void wenuy_timeline_model_set_playhead(WenuyTimelineModel* self, double seconds);
double wenuy_timeline_model_get_duration(WenuyTimelineModel* self);
/**
 * wenuy_timeline_model_serialize:
 * Returns: (transfer full): .wtimeline-text
 */
char* wenuy_timeline_model_serialize(WenuyTimelineModel* self);
gboolean wenuy_timeline_model_deserialize(WenuyTimelineModel* self, const char* data);
/**
 * wenuy_timeline_view_new:
 * @model: modellen (widgeten håller en referens)
 * @document: (nullable): dokument vars ångra/gör om ska omfatta flytt/trimning
 *
 * Returns: (transfer none): tidslinjewidgeten (signaler: playhead-moved, selection-changed, clips-changed)
 */
GtkWidget* wenuy_timeline_view_new(WenuyTimelineModel* model, WenuyDocument* document);

G_END_DECLS
