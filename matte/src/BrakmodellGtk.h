#pragma once
#include "Brakmodell.h"
#include <gtk/gtk.h>

namespace matte13 {
GtkWidget* skapaBrakpanel();
void stallInBrakpanel(GtkWidget* panel, Brak b);
}
