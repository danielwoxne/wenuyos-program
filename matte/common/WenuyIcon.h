// WenuyIcon – ritar en Wenuy-ikon som vektorväg direkt på grafikkortet (GSK)
// och animerar den enligt ikonspråkets regler (docs/ICONS.md):
//   * ritas in vid första visningen (strecket "tecknas", 320 ms)
//   * lyfter lite vid hover, trycks ihop vid klick
//   * signaturrörelse för vissa ikoner: spin (refresh), pulse (sök), drop (uppdatera)
//   * "fyren" (statuspunkt) tonar mellan grå och grön
// Reducerade animationer: bara in-toning. Av: ingenting. Allt skalar efter storlek.
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS
#define WENUY_TYPE_ICON (wenuy_icon_get_type())
G_DECLARE_FINAL_TYPE(WenuyIcon, wenuy_icon, WENUY, ICON, GtkWidget)

GtkWidget* wenuy_icon_new(const char* name, int size);
void wenuy_icon_set_name(WenuyIcon* self, const char* name);   // byter med kort korstoning
void wenuy_icon_set_beacon(WenuyIcon* self, gboolean on);     // statuspunkt på/av
void wenuy_icon_set_accent(WenuyIcon* self, gboolean accent);  // ritas i accentfärg
void wenuy_icon_play(WenuyIcon* self);                        // kör signaturrörelsen en gång
// Färgikon i lerstil (temat Wenuy/color) för samma namn; streckikonen om ingen finns
GtkWidget* wenuy_color_icon_new(const char* name, int size);
G_END_DECLS
