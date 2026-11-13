// Räknepappret: rutat papper där eleven räknar – en siffra per ruta (tangentbord), minnessiffror, streck
// under en rad, penna och sudd. "Ställ upp" sätter upp en uträkning (48 + 15) i kolumner.
// Rättas inte. Ritas på vitt papper med ljusblå rutor, som ett mattehäfte.
#pragma once
#include <gtk/gtk.h>

#include "PapperLagring.h"

#include <functional>
#include <string>

namespace matte {

// Koppla varaktig sparning till ändringar (efter färdigt penndrag och tangenttryckning).
// Varna om en hård minnes-/punktgräns nås; ingen tyst radering av handskrift.
void raknepapper_andringar(GtkWidget* papper, std::function<void()> spara,
                            std::function<void()> begransning);
void raknepapper_ny_session(GtkWidget* papper, const std::string& id,
                           const std::string& kort, const std::string& fraga,
                           int mall, int variant);
bool raknepapper_ladda_session(GtkWidget* papper, const PapperData& data);
PapperData raknepapper_hamta(GtkWidget* papper);

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
