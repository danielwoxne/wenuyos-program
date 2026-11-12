// libwenuy – färgikon (från ikontemat) med rörelse: uppdatering snurrar, klockan ringer,
// papperskorgen tar emot, mappen öppnas. Följer temats animationsnivå (full/reduced/off).
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WENUY_TYPE_MOTION_ICON (wenuy_motion_icon_get_type())
G_DECLARE_FINAL_TYPE(WenuyMotionIcon, wenuy_motion_icon, WENUY, MOTION_ICON, GtkWidget)

/**
 * wenuy_motion_icon_new:
 * @icon_name: ikonnamn, t.ex. "view-refresh", "preferences-system-notifications", "user-trash", "folder"
 * @size: storlek i pixlar
 * @motion: (nullable): "spin", "ring", "receive", "open" – %NULL väljer efter ikonnamnet
 *
 * Returns: (transfer none): widgeten
 */
GtkWidget* wenuy_motion_icon_new(const char* icon_name, int size, const char* motion);
/* Spela rörelsen en gång (t.ex. när en avisering kommer eller en fil läggs i papperskorgen). */
void wenuy_motion_icon_play(WenuyMotionIcon* self);
/* Upprepa tills den stängs av (t.ex. "spin" medan en uppdatering pågår). */
void wenuy_motion_icon_set_running(WenuyMotionIcon* self, gboolean running);
void wenuy_motion_icon_set_icon_name(WenuyMotionIcon* self, const char* icon_name);

G_END_DECLS
