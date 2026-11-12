#pragma once
#include "Hundraruta.h"
#include <gtk/gtk.h>

namespace matte13 {
// Frivillig GTK4/Cairo-laboration. Skapar ingen elevdata och ändrar inget svar.
GtkWidget* skapaHundrarutapanel();
void stallInHundrarutapanel(GtkWidget* panel, Brak ursprung);
}
