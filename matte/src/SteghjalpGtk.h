#pragma once
#include <gtk/gtk.h>
#include <functional>
#include <string>
#include <vector>

namespace matte13 {
// Ingen matematikrättning: läser förberedda ledtrådar och lösningssteg från originalinnehållet.
GtkWidget* skapaSteghjalp(std::function<void(const std::string&)> lasUpp);
void stallInSteghjalp(GtkWidget* panel, const std::vector<std::string>& steg);
}
