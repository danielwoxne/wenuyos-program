#pragma once

#include <gtk/gtk.h>

namespace matte13 {
// Gemensam pedagogisk arbetsyta som återanvänder de fem redan befintliga
// GTK-komponenterna. Inga egna matematikberäkningar eller skrivningar till elevdata.
GtkWidget* skapaLektionsyta(GtkWidget* brak, GtkWidget* hundraruta,
                           GtkWidget* tallinje, GtkWidget* geometri,
                           GtkWidget* steghjalp);

// Visa bara verktyg som passar uppgiften. Hjälpen visas bara om originalkortet
// faktiskt innehåller en ledtråd eller förberedda lösningssteg.
void uppdateraLektionsyta(GtkWidget* lektionsyta, bool brak,
                         bool tallinje, bool geometri, bool hjalp);
} // namespace matte13
