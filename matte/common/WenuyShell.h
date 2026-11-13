// libwenuy – skalets byggstenar: startmenyn, sökningen och aktivitetsfältets knappar.
//
// Panelen (wenuyos-panel) använder dem, men de är vanliga GTK-widgetar: ett annat skal, ett
// helskärmsläge eller ett program kan visa samma startmeny och sökning. Biblioteket gör jobbet
// (starta program, öppna filer, inställningar, räkna, kommandon, logga ut …); den som visar
// widgeten lägger den i ett fönster och stänger fönstret när "klar"-funktionen anropas.
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * WenuyShellDoneFunc:
 * @user_data: (closure): data från wenuy_shell_set_done_func()
 *
 * Anropas när startmenyn eller sökningen är klar (något startades, eller Esc) – stäng fönstret.
 */
typedef void (*WenuyShellDoneFunc)(gpointer user_data);

/**
 * wenuy_shell_set_done_func:
 * @func: (scope notified) (nullable): vad som ska hända när menyn/sökningen är klar
 * @user_data: (closure func): data till @func
 */
void wenuy_shell_set_done_func(WenuyShellDoneFunc func, gpointer user_data);

/**
 * WenuyShellLaunchFunc:
 * @app: programmet som startas
 * @user_data: (closure): data
 */
typedef void (*WenuyShellLaunchFunc)(GAppInfo* app, gpointer user_data);
/**
 * wenuy_shell_set_launch_func:
 * @func: (scope notified) (nullable): anropas när startmenyn eller sökningen startar ett program
 *   (t.ex. för att visa "startar …" i aktivitetsfältet)
 * @user_data: (closure func): data till @func
 */
void wenuy_shell_set_launch_func(WenuyShellLaunchFunc func, gpointer user_data);

/**
 * WenuyShellSwitchFunc:
 * @text: (nullable): text att börja sökningen med (bara för sökningen)
 * @user_data: (closure): data
 */
typedef void (*WenuyShellSwitchFunc)(const char* text, gpointer user_data);
/**
 * wenuy_shell_set_switch_funcs:
 * @open_search: (scope notified) (nullable): används inte längre (Start är en yta)
 * @open_start: (scope notified) (nullable): visa startmenyn (pilen i sökfältet)
 * @user_data: (closure): data till båda
 *
 * Startmenyn och sökningen visas i var sitt fönster av den som använder dem; de här byter mellan dem.
 */
void wenuy_shell_set_switch_funcs(WenuyShellSwitchFunc open_search, WenuyShellSwitchFunc open_start, gpointer user_data);

/**
 * WenuyShellWindowsFunc:
 * @user_data: (closure): data
 *
 * Returns: (transfer full) (nullable): öppna fönster, ett per rad: "id\tapp-id\ttitel"
 */
typedef char* (*WenuyShellWindowsFunc)(gpointer user_data);
/**
 * WenuyShellActivateFunc:
 * @id: fönstrets id (från #WenuyShellWindowsFunc)
 * @user_data: (closure): data
 */
typedef void (*WenuyShellActivateFunc)(const char* id, gpointer user_data);
/**
 * wenuy_shell_set_windows_func:
 * @list: (scope notified) (nullable): de öppna fönstren – visas överst i Start och i sökningen
 * @activate: (scope notified) (nullable): byt till ett fönster
 * @user_data: (closure): data till båda
 *
 * Skalet har ingen aktivitetsrad (0.4.0): fönster nås med Alt+Tab, i Start och via sökningen.
 */
void wenuy_shell_set_windows_func(WenuyShellWindowsFunc list, WenuyShellActivateFunc activate, gpointer user_data);

/**
 * WenuyShellCommandFunc:
 * @command: kommandoraden som skrevs i sökfältet
 * @user_data: (closure): data
 *
 * Returns: %TRUE om kommandot kördes (t.ex. i mini-terminalen); %FALSE = kör i Konsol
 */
typedef gboolean (*WenuyShellCommandFunc)(const char* command, gpointer user_data);
/**
 * wenuy_shell_set_command_func:
 * @func: (scope notified) (nullable): kör ett kommando från sökfältet (Enter); Start står kvar
 * @user_data: (closure func): data
 */
void wenuy_shell_set_command_func(WenuyShellCommandFunc func, gpointer user_data);

/**
 * WenuyShellTabListFunc:
 * @list: alternativen vid Tab när flera passar, åtskilda med två mellanslag
 * @user_data: (closure): samma data som till wenuy_shell_set_command_func()
 */
typedef void (*WenuyShellTabListFunc)(const char* list, gpointer user_data);
/**
 * wenuy_shell_set_tab_list_func:
 * @func: (scope notified) (nullable): visa Tab-alternativen (mini-terminalen skriver ut dem)
 */
void wenuy_shell_set_tab_list_func(WenuyShellTabListFunc func);
/**
 * wenuy_shell_set_type_func:
 * @func: (scope forever): skriver texten i mini-terminalen utan Enter (kommandon ur AI-svaret i Start)
 */
void wenuy_shell_set_type_func(WenuyShellTabListFunc func);

/**
 * wenuy_start_menu_set_side:
 * @side: (nullable) (transfer floating): widget till höger i Start (mini-terminalen), %NULL = ingen
 */
void wenuy_start_menu_set_side(GtkWidget* side);

/**
 * wenuy_start_menu_set_anchor:
 * @x: kortets vänsterkant i fönstret (bildpunkter), -1 = som förut (inget ankare)
 * @bottom: avstånd från fönstrets nederkant till kortet
 *
 * Datorläget: kortet läggs så att sökfältet längst ned hamnar över sökfältet i fältet.
 */
void wenuy_start_menu_set_anchor(int x, int bottom);

/**
 * wenuy_start_menu_launch_pin:
 * @n: 1–9: det n:te fästa programmet (Super+1 … Super+9)
 *
 * Returns: %TRUE om det fanns ett sådant program
 */
gboolean wenuy_start_menu_launch_pin(int n);

/**
 * wenuy_start_menu_set_width:
 * @width: kortets bredd i bildpunkter (-1 = förval)
 *
 * Datorläget: Start lika bred som fältets sökfält, så att resten av fältet (notiserna) syns.
 */
void wenuy_start_menu_set_width(int width);

/**
 * wenuy_start_menu_set_side_expanded:
 * @expanded: %TRUE = platsen till höger (mini-terminalen) fyller hela Start
 */
void wenuy_start_menu_set_side_expanded(gboolean expanded);
/**
 * wenuy_start_menu_get_side_expanded:
 *
 * Returns: %TRUE om mini-terminalen fyller hela Start
 */
gboolean wenuy_start_menu_get_side_expanded(void);

/**
 * wenuy_start_menu_get:
 *
 * Start: sökfältet, appnätet, senaste filer och bottenraden (en per program; byggs första gången).
 *
 * Returns: (transfer none): startmenyns kort – lägg det i ett fönster
 */
GtkWidget* wenuy_start_menu_get(void);
/**
 * wenuy_start_menu_reset:
 *
 * Inför visning: tömmer sökfältet och sätter markören där.
 */
void wenuy_start_menu_reset(void);

/**
 * wenuy_start_menu_set_tablet:
 * @tablet: %TRUE i surfplatteläget
 *
 * Start i helskärm med större brickor (surfplatteläget) eller som panel ovanför dockan.
 */
void wenuy_start_menu_set_tablet(gboolean tablet);

/**
 * wenuy_search_box_get:
 *
 * Samma kort som wenuy_start_menu_get() – Start och sökningen är en yta sedan 0.3.18.
 *
 * Returns: (transfer none): Start-kortet
 */
GtkWidget* wenuy_search_box_get(void);
/**
 * wenuy_search_box_reset:
 * @text: (nullable): text att börja med (NULL = tom): Start visas med förslagen för texten
 */
void wenuy_search_box_reset(const char* text);

/**
 * wenuy_dock_button_new:
 * @icon_name: ikon ur temat
 * @tooltip: (nullable): text när musen vilar på knappen
 * @size: ikonens storlek i bildpunkter (0 = 30)
 *
 * Returns: (transfer floating): en fäst knapp i aktivitetsfältets stil
 */
GtkWidget* wenuy_dock_button_new(const char* icon_name, const char* tooltip, int size);
/**
 * wenuy_task_button_new:
 * @icon: (nullable): programmets ikon (se wenuy_app_icon_for_id())
 *
 * Returns: (transfer floating): en knapp för ett öppet fönster, med streck för aktivt/minimerat
 */
GtkWidget* wenuy_task_button_new(GIcon* icon);
/**
 * wenuy_task_button_set_state:
 * @button: från wenuy_task_button_new()
 * @active: fönstret har fokus
 * @minimized: fönstret är minimerat
 */
void wenuy_task_button_set_state(GtkWidget* button, gboolean active, gboolean minimized);
/**
 * wenuy_app_icon_for_id:
 * @app_id: fönstrets app-id (Wayland) eller .desktop-namn
 *
 * Returns: (transfer full): programmets ikon
 */
GIcon* wenuy_app_icon_for_id(const char* app_id);

G_END_DECLS
