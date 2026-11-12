// libwenuy – skrivbordsbakgrunden som byggsten (GTK 4).
//
// WenuyBackdrop ritar användarens bakgrund (bild eller färg, från temat: ~/.config/wenuyos/wallpaper
// och wallpaper-color) och följer bytet direkt. Används av skrivbordet, startskärmen i surfplatteläget,
// appnätet i helskärm – och kan användas av vilket program som helst (t.ex. ett spels meny).
//
// Fördelar mot att varje del läser bilden själv:
//  - bilden läses EN gång per program och delas av alla WenuyBackdrop (skalad till skärmen – lite minne)
//  - helt täckande: en helskärmsvy med bakgrund behöver inget genomskinligt lager över fönstren
//    (snabbare på svag grafik, och fönstren under behöver inte ritas)
//  - mörkning ritas i samma lager (ingen alfa ovanpå innehåll)
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WENUY_TYPE_BACKDROP (wenuy_backdrop_get_type())
G_DECLARE_FINAL_TYPE(WenuyBackdrop, wenuy_backdrop, WENUY, BACKDROP, GtkWidget)

/**
 * wenuy_backdrop_new:
 *
 * Returns: (transfer floating): en ny bakgrund som visar användarens skrivbordsbakgrund
 */
GtkWidget* wenuy_backdrop_new(void);
/**
 * wenuy_backdrop_set_dim:
 * @self: bakgrunden
 * @amount: 0 (ingen) till 0.8 – mörkare bakgrund bakom innehåll, t.ex. appnätet
 */
void wenuy_backdrop_set_dim(WenuyBackdrop* self, double amount);
/**
 * wenuy_backdrop_preview:
 * @self: bakgrunden
 * @what: (nullable): bildsökväg eller "#rrggbb" att visa tillfälligt; NULL = tillbaka till användarens val
 */
void wenuy_backdrop_preview(WenuyBackdrop* self, const char* what);
/**
 * wenuy_backdrop_get_current:
 *
 * Returns: (transfer full): användarens bakgrund – bildsökväg eller "#rrggbb"
 */
char* wenuy_backdrop_get_current(void);
/**
 * wenuy_backdrop_set_current:
 * @what: bildsökväg eller "#rrggbb"
 *
 * Byter användarens bakgrund (via temat, så att låsskärm och inloggning följer med).
 * Alla WenuyBackdrop i alla program visar den nya bakgrunden.
 */
void wenuy_backdrop_set_current(const char* what);
/**
 * wenuy_backdrop_set_monitor:
 * @self: bakgrunden
 * @connector: (nullable): skärmens namn (t.ex. "HDMI-A-1"); en egen bakgrund för den skärmen
 *   (~/.config/wenuyos/wallpaper-<namn>, bild eller "#rrggbb") används i stället för den gemensamma
 */
void wenuy_backdrop_set_monitor(WenuyBackdrop* self, const char* connector);
/**
 * wenuy_backdrop_set_for_monitor:
 * @connector: skärmens namn
 * @what: (nullable): bild eller "#rrggbb" bara för den skärmen; NULL = samma som de andra
 */
void wenuy_backdrop_set_for_monitor(const char* connector, const char* what);
/**
 * wenuy_backdrop_set_slideshow:
 * @folder: (nullable): mapp med bilder som byts efter tur; NULL = inget bildspel
 * @minutes: hur ofta bilden byts (minst 1)
 */
void wenuy_backdrop_set_slideshow(const char* folder, int minutes);

G_END_DECLS
