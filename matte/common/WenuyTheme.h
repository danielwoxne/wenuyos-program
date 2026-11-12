// libwenuy – det centrala temat (wenuyos theme). Programmen läser färger, typsnitt, hörn och
// animationsnivå härifrån och uppdateras direkt när användaren byter tema.
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WENUY_TYPE_THEME (wenuy_theme_get_type())
G_DECLARE_FINAL_TYPE(WenuyTheme, wenuy_theme, WENUY, THEME, GObject)

/**
 * wenuy_theme_get_default:
 * Returns: (transfer none): det aktiva temat (samma objekt i hela programmet)
 */
WenuyTheme* wenuy_theme_get_default(void);
/**
 * wenuy_theme_get:
 * @section: t.ex. "Colors", "Fonts", "Shape", "Motion", "Sounds", "Accessibility"
 * @key: t.ex. "accent", "ui", "radius", "level"
 * Returns: (transfer full) (nullable): värdet
 */
char* wenuy_theme_get(WenuyTheme* self, const char* section, const char* key);
/**
 * wenuy_theme_get_name:
 * Returns: (transfer full): temats namn, t.ex. "wenuyos"
 */
char* wenuy_theme_get_name(WenuyTheme* self);
/**
 * wenuy_theme_get_accent:
 * @rgba: (out caller-allocates): accentfärgen för nuvarande ljust/mörkt
 */
void wenuy_theme_get_accent(WenuyTheme* self, GdkRGBA* rgba);
int wenuy_theme_get_radius(WenuyTheme* self);
/* "full", "reduced" eller "off" */
const char* wenuy_theme_get_motion(WenuyTheme* self);
gboolean wenuy_theme_is_high_contrast(WenuyTheme* self);
/* Lägger temats accent och hörn i programmets stil och håller dem aktuella (anropas av wenuy::loadStyle). */
void wenuy_theme_apply_style(void);
/**
 * wenuy_play_sound:
 * @event: händelse enligt freedesktop, t.ex. "complete", "message-new-instant", "dialog-warning"
 *
 * Spelar ljudet ur det valda ljudtemat (tyst om användaren stängt av systemljud).
 */
void wenuy_play_sound(const char* event);
/* Signal "changed" (): användaren bytte tema eller ändrade något i det. */

G_END_DECLS
