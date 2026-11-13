// libwenuy – mätning och buggkontroll i alla WenuyOS-program.
//
// Startas automatiskt av wenuy::loadStyle() och wenuy_i18n_init() (alla mallar), eller med
// wenuy_diag_init(). Programmet får då, utan extra kod:
//   – starttid (från processstart till första fönstret) och minne (nu och högsta)
//   – varningar och kritiska fel från GLib/GTK räknade och de senaste sparade
//   – krasch-upptäckt: en körning som aldrig avslutades syns nästa gång
// Rapporten skrivs till ~/.local/state/wenuyos/diag/<app-id>.json och läses av `wenuyos check`.
//
// Egna kontroller: wenuy_diag_add_check("databas", fn) – fn returnerar NULL om allt är bra, annars
// ett felmeddelande. Med WENUY_SELFTEST=1 kör programmet sina kontroller när första fönstret visats,
// skriver rapporten och avslutas (felkod = antal fel) – så kan hela systemet provas automatiskt.
#pragma once
#include <glib.h>

G_BEGIN_DECLS

/**
 * wenuy_diag_init:
 * @app_id: (nullable): programmets id (%NULL = programnamnet)
 *
 * Får anropas flera gånger; bara första gången gör något.
 */
void wenuy_diag_init(const char* app_id);
/**
 * wenuy_diag_mark:
 * @name: t.ex. "data-loaded"
 *
 * Sparar tid sedan start och minne just nu i rapporten.
 */
void wenuy_diag_mark(const char* name);
/* Millisekunder sedan processen startade. */
gint64 wenuy_diag_elapsed_ms(void);
/* Minne som används nu (RSS, kB). */
guint64 wenuy_diag_rss_kb(void);

/**
 * WenuyDiagCheckFunc:
 * @user_data: (closure): data
 *
 * Returns: (transfer full) (nullable): %NULL om kontrollen gick bra, annars vad som är fel
 */
typedef char* (*WenuyDiagCheckFunc)(gpointer user_data);
/**
 * wenuy_diag_add_check:
 * @name: kontrollens namn, t.ex. "databasen går att öppna"
 * @func: (scope notified) (closure data) (destroy destroy): kontrollen
 * @data: data
 * @destroy: frigör data
 */
void wenuy_diag_add_check(const char* name, WenuyDiagCheckFunc func, gpointer data, GDestroyNotify destroy);
/* Kör alla kontroller och skriver rapporten. Returnerar antal fel. */
int wenuy_diag_run_checks(void);
/* Skriv rapporten nu (sker också automatiskt vid första fönstret, efter kontroller och vid avslut). */
void wenuy_diag_write(void);

/**
 * wenuy_diag_measure_begin:
 * @name: funktionens namn, t.ex. "open-folder" eller "search"
 *
 * Mät hur lång tid en funktion tar (och hur ofta den misslyckas). Rapporten får per namn:
 * antal, misslyckade, medel-, max- och senaste tid. Läses av `wenuyos check` och mätrapporten.
 *
 * Returns: en biljett till wenuy_diag_measure_end()
 */
guint64 wenuy_diag_measure_begin(const char* name);
/**
 * wenuy_diag_measure_end:
 * @token: biljetten från wenuy_diag_measure_begin()
 * @ok: om funktionen lyckades
 */
void wenuy_diag_measure_end(guint64 token, gboolean ok);
/**
 * wenuy_diag_event:
 * @name: händelsens namn, t.ex. "notification-shown"
 * @ok: om det gick bra
 *
 * Räkna en händelse utan tid.
 */
void wenuy_diag_event(const char* name, gboolean ok);

G_END_DECLS

#ifdef __cplusplus
namespace wenuy {
// Mät ett block: { wenuy::Measure m("open-folder"); … if (fel) m.fail(); }
class Measure {
public:
    explicit Measure(const char* name) : t_(wenuy_diag_measure_begin(name)) {}
    ~Measure() { wenuy_diag_measure_end(t_, ok_); }
    void fail() { ok_ = FALSE; }
    Measure(const Measure&) = delete;
    Measure& operator=(const Measure&) = delete;
private:
    guint64 t_;
    gboolean ok_ = TRUE;
};
}  // namespace wenuy
#endif
