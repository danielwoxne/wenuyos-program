// libwenuy – programdata: inställningar och databas, samma för alla språk (C, C++, Python, JavaScript …).
//
//   WenuySettings  inställningar i ~/.config/<app-id>/settings.ini
//                  standardvärden från /usr/share/<app-id>/settings.ini och /etc/xdg/<app-id>/settings.ini
//                  (administratören kan låsa värden under [locked]); sparas säkert, signal "changed::<nyckel>",
//                  bind() kopplar en nyckel till en widgets egenskap; ändringar från andra processer märks direkt.
//   WenuyStore     SQLite i ~/.local/share/<app-id>/<namn>.db (WAL, säker skrivning), versionsstyrda
//                  ändringar av tabeller (migrate), frågor med parametrar, resultat som GListModel
//                  (kan visas direkt i GtkListView/GtkColumnView) och ett enkelt nyckel/värde-lager.
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* ------------------------------------------------------------------ WenuySettings */
#define WENUY_TYPE_SETTINGS (wenuy_settings_get_type())
G_DECLARE_FINAL_TYPE(WenuySettings, wenuy_settings, WENUY, SETTINGS, GObject)

/**
 * wenuy_settings_new:
 * @app_id: programmets id, t.ex. "se.exempel.Budget"
 *
 * Returns: (transfer full): programmets inställningar
 */
WenuySettings* wenuy_settings_new(const char* app_id);
/**
 * wenuy_settings_get_string:
 * @fallback: (nullable): värde om nyckeln saknas
 * Returns: (transfer full) (nullable): värdet
 */
char* wenuy_settings_get_string(WenuySettings* self, const char* key, const char* fallback);
int wenuy_settings_get_int(WenuySettings* self, const char* key, int fallback);
double wenuy_settings_get_double(WenuySettings* self, const char* key, double fallback);
gboolean wenuy_settings_get_boolean(WenuySettings* self, const char* key, gboolean fallback);
/**
 * wenuy_settings_get_strv:
 * Returns: (transfer full) (array zero-terminated=1): listan (tom om nyckeln saknas)
 */
char** wenuy_settings_get_strv(WenuySettings* self, const char* key);
/* Sätt-funktionerna returnerar FALSE om administratören har låst nyckeln. */
gboolean wenuy_settings_set_string(WenuySettings* self, const char* key, const char* value);
gboolean wenuy_settings_set_int(WenuySettings* self, const char* key, int value);
gboolean wenuy_settings_set_double(WenuySettings* self, const char* key, double value);
gboolean wenuy_settings_set_boolean(WenuySettings* self, const char* key, gboolean value);
/**
 * wenuy_settings_set_strv:
 * @value: (array zero-terminated=1): listan
 */
gboolean wenuy_settings_set_strv(WenuySettings* self, const char* key, const char* const* value);
gboolean wenuy_settings_has_key(WenuySettings* self, const char* key);
gboolean wenuy_settings_is_locked(WenuySettings* self, const char* key);
/* Tar bort användarens värde så att standardvärdet gäller igen. */
void wenuy_settings_reset(WenuySettings* self, const char* key);
/**
 * wenuy_settings_list_keys:
 * Returns: (transfer full) (array zero-terminated=1): alla nycklar (användarens och standard)
 */
char** wenuy_settings_list_keys(WenuySettings* self);
/**
 * wenuy_settings_bind:
 * @object: (type GObject.Object): t.ex. en GtkSwitch
 * @property: egenskapen, t.ex. "active"
 *
 * Håller nyckeln och egenskapen lika åt båda hållen (typ: sträng, heltal, decimal eller sant/falskt).
 */
void wenuy_settings_bind(WenuySettings* self, const char* key, gpointer object, const char* property);
/* Skriver till disk direkt (annars sker det automatiskt strax efter en ändring). */
gboolean wenuy_settings_sync(WenuySettings* self);
/**
 * wenuy_settings_get_path:
 * Returns: sökvägen till användarens fil
 */
const char* wenuy_settings_get_path(WenuySettings* self);
/* Signal "changed" (detaljerad: "changed::nyckel") (const char* key). */

/* ------------------------------------------------------------------ WenuyRow */
#define WENUY_TYPE_ROW (wenuy_row_get_type())
G_DECLARE_FINAL_TYPE(WenuyRow, wenuy_row, WENUY, ROW, GObject)
int wenuy_row_get_n_columns(WenuyRow* self);
/**
 * wenuy_row_get_column_name:
 * Returns: (nullable): kolumnens namn
 */
const char* wenuy_row_get_column_name(WenuyRow* self, int column);
/**
 * wenuy_row_get_string:
 * @column: kolumnens nummer (0 …)
 * Returns: (nullable): värdet som text, %NULL för SQL NULL
 */
const char* wenuy_row_get_string(WenuyRow* self, int column);
/**
 * wenuy_row_get:
 * @name: kolumnens namn
 * Returns: (nullable): värdet som text
 */
const char* wenuy_row_get(WenuyRow* self, const char* name);
gint64 wenuy_row_get_int(WenuyRow* self, int column);
double wenuy_row_get_double(WenuyRow* self, int column);
gboolean wenuy_row_is_null(WenuyRow* self, int column);

/* ------------------------------------------------------------------ WenuyStore */
#define WENUY_TYPE_STORE (wenuy_store_get_type())
G_DECLARE_FINAL_TYPE(WenuyStore, wenuy_store, WENUY, STORE, GObject)

/**
 * wenuy_store_open:
 * @app_id: programmets id
 * @name: (nullable): databasens namn ("data" om %NULL)
 * @error: fel
 *
 * Returns: (transfer full) (nullable): databasen, eller %NULL vid fel
 */
WenuyStore* wenuy_store_open(const char* app_id, const char* name, GError** error);
/**
 * wenuy_store_open_path:
 * @path: sökväg till en databasfil (":memory:" för en tillfällig)
 * Returns: (transfer full) (nullable): databasen
 */
WenuyStore* wenuy_store_open_path(const char* path, GError** error);
/**
 * wenuy_store_migrate:
 * @version: databasens nya version (1, 2, 3 …)
 * @sql: SQL som tar databasen från @version - 1 till @version
 *
 * Körs bara om databasen är äldre – anropa i ordning vid varje start.
 * Returns: %TRUE om databasen nu är minst @version
 */
gboolean wenuy_store_migrate(WenuyStore* self, int version, const char* sql, GError** error);
int wenuy_store_get_version(WenuyStore* self);
/**
 * wenuy_store_exec:
 * @sql: en eller flera SQL-satser; ? ersätts med @params i ordning
 * @params: (nullable) (array zero-terminated=1): värden (text; "" ≠ NULL)
 *
 * Returns: antal ändrade rader, eller -1 vid fel
 */
int wenuy_store_exec(WenuyStore* self, const char* sql, const char* const* params, GError** error);
/**
 * wenuy_store_query:
 * @params: (nullable) (array zero-terminated=1): värden för ?
 *
 * Returns: (transfer full) (nullable): raderna (WenuyRow) som en lista
 */
GListModel* wenuy_store_query(WenuyStore* self, const char* sql, const char* const* params, GError** error);
/**
 * wenuy_store_query_value:
 * @params: (nullable) (array zero-terminated=1): värden för ?
 * Returns: (transfer full) (nullable): första kolumnen i första raden
 */
char* wenuy_store_query_value(WenuyStore* self, const char* sql, const char* const* params, GError** error);
gint64 wenuy_store_last_insert_id(WenuyStore* self);
/* Flera ändringar som en enhet: begin → … → commit (eller rollback). Mycket snabbare för många rader. */
gboolean wenuy_store_begin(WenuyStore* self);
gboolean wenuy_store_commit(WenuyStore* self);
void wenuy_store_rollback(WenuyStore* self);
/* Nyckel/värde (tabellen _kv skapas vid behov). */
void wenuy_store_set(WenuyStore* self, const char* key, const char* value);
/**
 * wenuy_store_get:
 * Returns: (transfer full) (nullable): värdet
 */
char* wenuy_store_get(WenuyStore* self, const char* key);
/* Signal "changed" (const char* table): skickas efter exec() som ändrade rader. */

/* ------------------------------------------------------------------ språk */
/**
 * wenuy_i18n_init:
 * @app_id: programmets id (= gettext-domän; filerna heter <app-id>.mo)
 *
 * Läser översättningar från ~/.local/share/locale, /usr/share/locale eller $WENUY_LOCALEDIR.
 */
void wenuy_i18n_init(const char* app_id);
/**
 * wenuy_tr:
 * @en: texten på engelska (basspråk, och nyckel i .po-filerna)
 * @sv: texten på svenska (alltid med)
 *
 * Returns: (transfer none): texten på användarens språk
 */
const char* wenuy_tr(const char* en, const char* sv);

G_END_DECLS
