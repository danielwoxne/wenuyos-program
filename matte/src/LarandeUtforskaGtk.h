#pragma once
#include "LarandeUtforska.h"
#include <gtk/gtk.h>
namespace matte13 {
GtkWidget* skapaTallinjepanel();
void stallInTallinjepanel(GtkWidget* panel, Tallinje modell);
GtkWidget* skapaGeometripanel();
void stallInGeometripanel(GtkWidget* panel);
}
