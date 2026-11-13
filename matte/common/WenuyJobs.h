// libwenuy – bakgrundsjobb och aviseringar.
//
//   WenuyJob      ett långt arbete (export, import, beräkning …): syns i panelen med förlopp och
//                 avbryt-knapp, hindrar viloläge medan det pågår, håller programmet igång om fönstret
//                 stängs och aviserar när det är klart.
//   wenuy_notify  avisering via skrivbordets aviseringstjänst; samma rubrik ersätter den förra
//                 (ingen spam), "Stör ej" och tema sköts centralt av WenuyOS.
#pragma once
#include <gio/gio.h>

G_BEGIN_DECLS

#define WENUY_TYPE_JOB (wenuy_job_get_type())
G_DECLARE_FINAL_TYPE(WenuyJob, wenuy_job, WENUY, JOB, GObject)

/**
 * wenuy_job_new:
 * @title: vad som görs, t.ex. "Exporterar video"
 *
 * Startar jobbet direkt (syns i panelen).
 * Returns: (transfer full): jobbet
 */
WenuyJob* wenuy_job_new(const char* title);
/**
 * wenuy_job_set_progress:
 * @fraction: 0–1, eller negativt om det inte går att veta
 * @text: (nullable): t.ex. "Bild 120 av 900"
 *
 * Billigt att anropa ofta – panelen uppdateras högst 5 gånger/s.
 */
void wenuy_job_set_progress(WenuyJob* self, double fraction, const char* text);
/* TRUE när användaren har tryckt Avbryt (i panelen eller via wenuy_job_cancel). Kontrollera i loopen. */
gboolean wenuy_job_is_cancelled(WenuyJob* self);
void wenuy_job_cancel(WenuyJob* self);
/**
 * wenuy_job_get_cancellable:
 * Returns: (transfer none): avbryts samtidigt som jobbet (till GIO-anrop)
 */
GCancellable* wenuy_job_get_cancellable(WenuyJob* self);
/**
 * wenuy_job_finish:
 * @success: lyckades jobbet
 * @message: (nullable): visas i aviseringen, t.ex. filnamnet eller felet
 *
 * Aviserar (om jobbet tog mer än några sekunder eller misslyckades) och släpper viloläget.
 */
void wenuy_job_finish(WenuyJob* self, gboolean success, const char* message);
/* Avisera även för korta jobb (standard: bara jobb längre än 5 s). */
void wenuy_job_set_notify(WenuyJob* self, gboolean always);
/* Signal "cancelled" (): användaren tryckte Avbryt. */

/**
 * wenuy_notify:
 * @title: rubrik
 * @body: (nullable): text
 * @icon: (nullable): ikonnamn, t.ex. "dialog-information"
 * @urgent: brådskande (visas även i "Stör ej")
 *
 * Returns: aviseringens id (0 om ingen aviseringstjänst finns)
 */
guint wenuy_notify(const char* title, const char* body, const char* icon, gboolean urgent);
void wenuy_notify_close(guint id);

G_END_DECLS
