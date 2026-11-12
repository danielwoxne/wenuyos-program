// libwenuy – tillägg (plugins) med Lua i alla WenuyOS-program.
//
// Programmet bestämmer vad tilläggen får göra: det registrerar egna funktioner (wenuy.<namn>(…)) och
// skickar händelser (hooks). Tilläggen ligger i ~/.local/share/<app-id>/plugins/*.lua och
// /usr/share/<app-id>/plugins/*.lua. De körs i en sandlåda: inga filer, inget nätverk, inga program –
// bara det programmet uttryckligen ger dem – med tak för minne (16 MB) och tid (0,5 s per anrop).
//
// I Lua:
//   plugin = { name = "Ordräknare", description = "Visar antal ord" }
//   wenuy.on("save", function(text) return text:gsub("%s+$", "") end)   -- filter: returnerar ny text
//   wenuy.action("Räkna ord", function() wenuy.notify(#wenuy.text():gsub("%S+", "")) end)
#pragma once
#include <glib-object.h>

G_BEGIN_DECLS

#define WENUY_TYPE_SCRIPT (wenuy_script_get_type())
G_DECLARE_FINAL_TYPE(WenuyScript, wenuy_script, WENUY, SCRIPT, GObject)

/**
 * WenuyScriptFunc:
 * @args: (array zero-terminated=1): argumenten från Lua som text
 * @user_data: (closure): data
 *
 * Returns: (transfer full) (nullable): svaret till Lua (text), eller %NULL för nil
 */
typedef char* (*WenuyScriptFunc)(const char* const* args, gpointer user_data);

/**
 * wenuy_script_new:
 * @app_id: programmets id (bestämmer var tilläggen ligger)
 *
 * Returns: (transfer full): en ny sandlåda
 */
WenuyScript* wenuy_script_new(const char* app_id);
/**
 * wenuy_script_register:
 * @name: funktionens namn i Lua (wenuy.<name>)
 * @func: (scope notified) (closure data) (destroy destroy): anropas när ett tillägg kallar funktionen
 * @data: data
 * @destroy: frigör data
 */
void wenuy_script_register(WenuyScript* self, const char* name, WenuyScriptFunc func, gpointer data, GDestroyNotify destroy);
/* Läser in alla tillägg. Returnerar antalet som laddades utan fel. */
int wenuy_script_load_plugins(WenuyScript* self);
/**
 * wenuy_script_load:
 * @path: en .lua-fil
 * Returns: %TRUE om tillägget laddades
 */
gboolean wenuy_script_load(WenuyScript* self, const char* path, GError** error);
/**
 * wenuy_script_run:
 * @code: Lua-kod
 * Returns: (transfer full) (nullable): resultatet som text
 */
char* wenuy_script_run(WenuyScript* self, const char* code, GError** error);
/**
 * wenuy_script_emit:
 * @hook: händelsens namn, t.ex. "save", "open", "export"
 * @value: (nullable): värdet som skickas till tilläggen
 *
 * Alla tillägg som lyssnar på @hook anropas i tur och ordning. Returnerar ett tillägg en text blir den
 * värdet för nästa (filter).
 * Returns: (transfer full) (nullable): slutligt värde
 */
char* wenuy_script_emit(WenuyScript* self, const char* hook, const char* value);
/**
 * wenuy_script_list_actions:
 * Returns: (transfer full) (array zero-terminated=1): namnen på åtgärder som tilläggen lagt till
 */
char** wenuy_script_list_actions(WenuyScript* self);
gboolean wenuy_script_run_action(WenuyScript* self, const char* name);
/**
 * wenuy_script_list_plugins:
 * Returns: (transfer full) (array zero-terminated=1): "namn – beskrivning" för varje laddat tillägg
 */
char** wenuy_script_list_plugins(WenuyScript* self);
/* Signaler: "error" (const char* plugin, const char* message), "log" (const char* message),
 * "actions-changed" (). */

G_END_DECLS
