// libwenuy – fönster-API: program ber WenuyOS om fönsterfunktioner utan att känna till
// fönsterhanteraren. Anropen går via D-Bus (org.wenuyos.Window, i panelen) och fungerar med
// Wayfire; under labwc ignoreras det som inte stöds (wenuy_window_api_available() säger vilket).
//
//   wenuy_window_set_keep_above(win, TRUE)          alltid överst
//   wenuy_window_set_sticky(win, TRUE)              på alla arbetsytor
//   wenuy_window_set_picture_in_picture(win, TRUE)  litet, överst, i hörnet (video, samtal)
//   wenuy_window_tile(win, "left")                  left right top bottom max center
//   wenuy_window_set_opacity(win, 0.85)             genomskinlighet (0.15–1.0)
//
// Fönstret identifieras med programmets process och fönstrets titel.
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean wenuy_window_api_available(void);
void wenuy_window_set_keep_above(GtkWindow* window, gboolean on);
void wenuy_window_set_sticky(GtkWindow* window, gboolean on);
void wenuy_window_set_picture_in_picture(GtkWindow* window, gboolean on);
void wenuy_window_tile(GtkWindow* window, const char* where);
void wenuy_window_set_opacity(GtkWindow* window, double opacity);

G_END_DECLS
