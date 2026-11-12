// libwenuy – färdiga byggstenar för WenuyOS-program (GTK 4, ritas av grafikkortet).
// Alla följer användarens animationsval (fulla / reducerade / av) automatiskt.
//
//   WenuyCard   kort med ikon, rubrik, text – lyfter vid hover, klickbart
//   WenuyChart  stapel- eller linjediagram som "växer fram"
//   wenuy_empty_state()      tom vy med ikon, rubrik och knapp
//   wenuy_page_stack_new()   sidbyten som glider/tonar enligt animationsvalet
//   wenuy_motion_level()     0 av, 1 reducerad, 2 fulla – för egna animationer
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

// ---------- WenuyCard ----------
#define WENUY_TYPE_CARD (wenuy_card_get_type())
G_DECLARE_FINAL_TYPE(WenuyCard, wenuy_card, WENUY, CARD, GtkWidget)
GtkWidget* wenuy_card_new(const char* icon, const char* title, const char* subtitle);
void wenuy_card_set_title(WenuyCard* self, const char* title);
void wenuy_card_set_subtitle(WenuyCard* self, const char* subtitle);
void wenuy_card_set_accent(WenuyCard* self, gboolean accent);   // markerad (t.ex. vald)
// Signal "activated" (inga argument) när kortet klickas eller får Enter/mellanslag.

// ---------- WenuyChart ----------
typedef enum { WENUY_CHART_BARS, WENUY_CHART_LINE } WenuyChartKind;
#define WENUY_TYPE_CHART (wenuy_chart_get_type())
G_DECLARE_FINAL_TYPE(WenuyChart, wenuy_chart, WENUY, CHART, GtkWidget)
GtkWidget* wenuy_chart_new(WenuyChartKind kind);
// Värden och etiketter (etiketter kan vara NULL). Diagrammet växer fram igen vid nya värden.
void wenuy_chart_set_data(WenuyChart* self, const double* values, const char* const* labels, guint n);

// ---------- hjälpfunktioner ----------
int wenuy_motion_level(void);
GtkWidget* wenuy_empty_state(const char* icon, const char* title, const char* description,
                             const char* button_label, GCallback on_click, gpointer data);
GtkWidget* wenuy_page_stack_new(void);
// "⋯"-knapp för titelraden: öppnar WenuyOS fönsterfunktioner (alltid överst, halva skärmen, skärmbild …)
GtkWidget* wenuy_window_actions_button(void);
/**
 * wenuy_header_bar_new:
 *
 * Titelrad i WenuyOS-stil: programikon och fet titel till vänster (följer fönstrets titel),
 * knapparna till höger. Samma som adw_header_bar_new() i övrigt.
 *
 * Returns: (transfer floating): en AdwHeaderBar
 */
GtkWidget* wenuy_header_bar_new(void);
/**
 * wenuy_header_bar_set_title:
 * @header_bar: från wenuy_header_bar_new()
 * @title: fast titel (t.ex. programnamnet i en sidopanel) i stället för fönstrets titel
 */
void wenuy_header_bar_set_title(GtkWidget* header_bar, const char* title);


// ---------- WenuyOS-fönstret: samma grund för alla program ----------
/**
 * wenuy_app_window_new:
 * @app: programmet
 * @title: fönstrets titel (visas fet till vänster bredvid programikonen)
 *
 * Ett programfönster i WenuyOS-stil: titelrad (wenuy_header_bar_new), innehåll, valfri statusrad
 * längst ned och plats för korta meddelanden (toast). Standardstorlek 900×640, minst 360×240,
 * kan ändras i storlek. Lägg knappar i titelraden med adw_header_bar_pack_start/end på
 * wenuy_app_window_get_header_bar().
 *
 * Returns: (transfer floating): ett AdwApplicationWindow
 */
GtkWidget* wenuy_app_window_new(GtkApplication* app, const char* title);
/**
 * wenuy_app_window_get_header_bar:
 * @window: från wenuy_app_window_new()
 *
 * Returns: (transfer none): titelraden (AdwHeaderBar)
 */
GtkWidget* wenuy_app_window_get_header_bar(GtkWidget* window);
/**
 * wenuy_app_window_set_content:
 * @window: från wenuy_app_window_new()
 * @content: (nullable): fönstrets innehåll
 */
void wenuy_app_window_set_content(GtkWidget* window, GtkWidget* content);
/**
 * wenuy_app_window_set_status:
 * @window: från wenuy_app_window_new()
 * @start: (nullable): text till vänster i statusraden
 * @end: (nullable): text till höger (t.ex. ledigt utrymme, zoom)
 *
 * Visar statusraden längst ned; båda NULL döljer den.
 */
void wenuy_app_window_set_status(GtkWidget* window, const char* start, const char* end);
/**
 * wenuy_app_window_add_top_bar:
 * @window: från wenuy_app_window_new()
 * @bar: rad under titelraden, t.ex. ett formatfält eller verktygsrad
 */
void wenuy_app_window_add_top_bar(GtkWidget* window, GtkWidget* bar);
/**
 * wenuy_app_window_add_bottom_bar:
 * @window: från wenuy_app_window_new()
 * @bar: rad längst ned i innehållet (ovanför statusraden), t.ex. Tillbaka/Nästa i en guide
 */
void wenuy_app_window_add_bottom_bar(GtkWidget* window, GtkWidget* bar);
/**
 * wenuy_app_window_toast:
 * @window: från wenuy_app_window_new()
 * @text: kort meddelande som försvinner av sig självt
 */
void wenuy_app_window_toast(GtkWidget* window, const char* text);
/**
 * wenuy_app_window_toast_action:
 * @window: från wenuy_app_window_new()
 * @text: kort meddelande
 * @button: (nullable): knappens text, t.ex. "Ångra"
 * @clicked: (scope notified) (nullable): anropas som void (*)(AdwToast*, gpointer) när knappen trycks
 * @data: (closure clicked): till @clicked
 *
 * Som wenuy_app_window_toast() men med en knapp (står kvar 8 s).
 */
void wenuy_app_window_toast_action(GtkWidget* window, const char* text, const char* button, GCallback clicked, gpointer data);
/**
 * WenuySidebarMode:
 * @WENUY_SIDEBAR_OVERLAY: smalt fönster: sidopanelen glider in över innehållet (knapp i titelraden) – t.ex. Filer
 * @WENUY_SIDEBAR_PAGES: smalt fönster: sidopanelen och innehållet visas som två sidor med Tillbaka – t.ex. Inställningar
 */
typedef enum { WENUY_SIDEBAR_OVERLAY, WENUY_SIDEBAR_PAGES } WenuySidebarMode;
/**
 * wenuy_app_window_new_with_sidebar:
 * @app: programmet
 * @title: programmets namn (står i sidopanelens titelrad bredvid ikonen)
 * @mode: hur sidopanelen beter sig i ett smalt fönster
 *
 * Samma fönstergrund som wenuy_app_window_new() men med sidopanel: två titelrader som alltid är
 * exakt lika höga (en obruten linje), sidopanel 200–260 px, innehåll med statusrad och toast.
 * Under 640 sp fälls sidopanelen ihop enligt @mode. wenuy_app_window_get_header_bar() ger innehållets
 * titelrad, wenuy_app_window_get_sidebar_header_bar() sidopanelens.
 *
 * Returns: (transfer floating): ett AdwApplicationWindow
 */
GtkWidget* wenuy_app_window_new_with_sidebar(GtkApplication* app, const char* title, WenuySidebarMode mode);
/**
 * wenuy_app_window_set_sidebar:
 * @window: från wenuy_app_window_new_with_sidebar()
 * @sidebar: (nullable): sidopanelens innehåll (under dess titelrad), t.ex. en lista i en GtkScrolledWindow
 */
void wenuy_app_window_set_sidebar(GtkWidget* window, GtkWidget* sidebar);
/**
 * wenuy_app_window_get_sidebar_header_bar:
 * @window: från wenuy_app_window_new_with_sidebar()
 *
 * Returns: (transfer none) (nullable): sidopanelens titelrad
 */
GtkWidget* wenuy_app_window_get_sidebar_header_bar(GtkWidget* window);
/**
 * wenuy_app_window_set_content_title:
 * @window: från wenuy_app_window_new_with_sidebar()
 * @title: (nullable): titel i innehållets titelrad, t.ex. vald sida
 */
void wenuy_app_window_set_content_title(GtkWidget* window, const char* title);
/**
 * wenuy_app_window_show_content:
 * @window: från wenuy_app_window_new_with_sidebar()
 *
 * Efter ett val i sidopanelen: i ett smalt fönster visas innehållet (sidopanelen göms).
 */
void wenuy_app_window_show_content(GtkWidget* window);
/**
 * wenuy_app_window_get_breakpoint:
 * @window: från wenuy_app_window_new_with_sidebar()
 *
 * Brytpunkten för smalt fönster (AdwBreakpoint) – lägg till egna värden med adw_breakpoint_add_setter().
 *
 * Returns: (transfer none) (nullable): brytpunkten
 */
GObject* wenuy_app_window_get_breakpoint(GtkWidget* window);
/**
 * wenuy_status_bar_new:
 *
 * Statusrad i WenuyOS-stil (1 px linje ovanför, liten dämpad text) för program med egen layout.
 *
 * Returns: (transfer floating): en horisontell GtkBox med CSS-klassen "wenuy-statusbar"
 */
GtkWidget* wenuy_status_bar_new(void);

G_END_DECLS
