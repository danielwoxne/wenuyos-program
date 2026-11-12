// Översiktskort för lärandekartan. Anroparen ansvarar för filtrering och navigering.
#pragma once
#include "Mattekarta.h"
#include <gtk/gtk.h>

namespace matte13 {
GtkWidget* skapaKartaOversikt(const KartaSummering& summering, bool svenska);
} // namespace matte13
