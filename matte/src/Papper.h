// Räknepappret: rutat papper där eleven räknar – en siffra per ruta (tangentbord), minnessiffror, streck
// under en rad, penna och sudd. "Ställ upp" sätter upp en uträkning (48 + 15) i kolumner.
// Rättas inte. Ritas på vitt papper med ljusblå rutor, som ett mattehäfte.
#pragma once
#include <gtk/gtk.h>

#include <string>

namespace matte {

GtkWidget* raknepapper_new();                                   // pappret (fokuserbart, fyller sin bredd)
GtkWidget* raknepapper_verktyg(GtkWidget* papper);              // raden ovanför: Skriv, Penna, Sudda, Liten siffra …
void raknepapper_rensa(GtkWidget* papper);
void raknepapper_skriv(GtkWidget* papper, const std::string& tecken);   // sifferknapparna ("⌫" suddar)
void raknepapper_zoom(GtkWidget* papper, double zoom);
int raknepapper_rader(GtkWidget* papper);                        // rader med något på (till loggen)
bool raknepapper_tomt(GtkWidget* papper);
// Ställ upp en uträkning ur en text ("48 + 15 = ?"): false om texten inte har två tal med + − · ×
bool raknepapper_kan_stalla_upp(const std::string& text);
bool raknepapper_stall_upp(GtkWidget* papper, const std::string& text);
void raknepapper_satt_uppgift(GtkWidget* papper, const std::string& text);   // texten "Ställ upp" använder
// Testkrok: skriv rad för rad ("48|+15") från rad 1, kolumn 2
void raknepapper_fyll(GtkWidget* papper, const std::string& rader);

}  // namespace matte
