// libwenuy – Arbetsminne (0.3.18): var man var i ett program överlever utloggning, omstart och krasch.
//
// Programmet säger vad det vill minnas med wenuy_app_remember() (fil, rad, rullning och egna nycklar);
// biblioteket skriver ~/.local/state/wenuyos/arbetsminne/<program>/<fönster-id>.toml högst var 2:a sekund
// vid ändring (säker skrivning: tempfil + rename) och tar bort filen när användaren själv stänger
// fönstret. Kvar blir bara det som var öppet vid utloggning, omstart eller krasch. "<program> --resume
// <fönster-id>" startar programmet där det var. Av med `wenuyos set arbetsminne off`.
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * wenuy_app_remember:
 * @window: programmets fönster (t.ex. från wenuy_app_window_new())
 * @key: nyckel – "fil", "rad", "rullning" eller en egen (bokstäver, siffror, punkt, bindestreck)
 * @value: (nullable): värdet som text; %NULL tar bort nyckeln
 *
 * Kom ihåg något om fönstret. Skrivs till disk inom 2 sekunder (och direkt om programmet avslutas
 * med SIGTERM/SIGHUP, t.ex. vid utloggning). Billigt att anropa ofta: oförändrade värden skrivs inte.
 */
void wenuy_app_remember(GtkWidget* window, const char* key, const char* value);

/**
 * wenuy_app_recall:
 * @window: fönstret
 * @key: nyckel
 *
 * Returns: (transfer full) (nullable): värdet som minns för fönstret (efter wenuy_app_window_resume())
 */
char* wenuy_app_recall(GtkWidget* window, const char* key);

/**
 * wenuy_app_window_resume:
 * @window: ett nytt fönster
 * @id: fönster-id från "--resume <id>"
 *
 * Läser in vad som mindes för @id. Fönstret behåller samma id (samma fil skrivs över).
 *
 * Returns: %TRUE om det fanns något att återställa
 */
gboolean wenuy_app_window_resume(GtkWidget* window, const char* id);

/**
 * wenuy_app_window_get_memory_id:
 * @window: fönstret
 *
 * Returns: (transfer none) (nullable): fönstrets id i arbetsminnet (finns efter första wenuy_app_remember())
 */
const char* wenuy_app_window_get_memory_id(GtkWidget* window);

/**
 * wenuy_app_take_resume_arg:
 * @argc: (inout): antal argument
 * @argv: (array length=argc) (inout): argumenten
 *
 * Plockar bort "--resume <id>" (eller "--resume=<id>") ur argumenten, före g_application_run().
 *
 * Returns: (transfer full) (nullable): id att återställa, eller %NULL
 */
char* wenuy_app_take_resume_arg(int* argc, char** argv);

/**
 * wenuy_app_memory_flush:
 *
 * Skriv allt som väntar nu (görs själv vid SIGTERM/SIGHUP).
 */
void wenuy_app_memory_flush(void);

G_END_DECLS

#ifdef __cplusplus
#include <map>
#include <string>
// Kärnan utan GTK (enhetstestas i wenuy-memory-test)
namespace wenuy {
namespace memory {
struct Entry {
    std::string program, id, exec;
    std::map<std::string, std::string> values;
};
bool enabled();                                    // ~/.config/wenuyos/arbetsminne != "off"
std::string root();                                // ~/.local/state/wenuyos/arbetsminne
std::string pathFor(const std::string& program, const std::string& id);
std::string newId();                               // "20261108-091500-a3f2" – sorteras efter tid
std::string toToml(const Entry& e);
bool fromToml(const std::string& text, Entry& e);
bool save(const std::string& path, const Entry& e);   // tempfil + rename
bool load(const std::string& path, Entry& e);
}  // namespace memory
}  // namespace wenuy
#endif
