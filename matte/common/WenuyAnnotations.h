/* Annoteringar för GObject Introspection (läses bara av g-ir-scanner, inget att inkludera).
 * Gör att libwenuy fungerar likadant från Python, JavaScript, Rust, Vala och Lua. */

/**
 * wenuy_color_icon_new:
 * @name: samma namn som för wenuy_icon_new (t.ex. "settings", "wifi") eller en färgikons id
 * @size: storlek i pixlar
 *
 * Returns: (transfer none): färgikonen i lerstil, eller streckikonen om ingen färgikon finns
 */

/**
 * wenuy_icon_new:
 * @name: ikonens namn (se /usr/share/wenuyos/icons.json)
 * @size: storlek i pixlar
 *
 * Returns: (transfer none): en animerad Wenuy-ikon
 */

/**
 * wenuy_card_new:
 * @icon: (nullable): ikonnamn
 * @title: rubrik
 * @subtitle: (nullable): text under rubriken
 *
 * Returns: (transfer none): ett kort som lyfter vid hover (signal "activated")
 */

/**
 * wenuy_chart_new:
 * Returns: (transfer none): ett diagram
 */

/**
 * wenuy_chart_set_data:
 * @self: diagrammet
 * @values: (array length=n): värden
 * @labels: (array length=n) (nullable): etiketter
 * @n: antal
 */

/**
 * wenuy_empty_state: (skip)
 */

/**
 * wenuy_page_stack_new:
 * Returns: (transfer none): en GtkStack med animationer efter användarens val
 */

/**
 * wenuy_window_actions_button:
 * Returns: (transfer none): "⋯"-knappen för titelraden
 */

/**
 * WenuyCanvasDrawFunc:
 * @canvas: ritytan
 * @snapshot: rita här, i innehållets koordinater
 * @visible: den synliga delen av innehållet
 * @zoom: aktuell zoom
 * @data: (closure): data
 */

/**
 * wenuy_canvas_new:
 * Returns: (transfer none): en rityta med zoom och panorering
 */

/**
 * wenuy_canvas_set_draw_func:
 * @self: ritytan
 * @fn: (scope notified) (closure data) (destroy destroy) (nullable): ritfunktion
 * @data: data
 * @destroy: (destroy data): frigör data
 */

/**
 * wenuy_canvas_to_content:
 * @self: ritytan
 * @x: x i ytan
 * @y: y i ytan
 * @cx: (out): x i innehållet
 * @cy: (out): y i innehållet
 */

/**
 * wenuy_file_view_new:
 * Returns: (transfer none): en visare för alla filtyper
 */

/**
 * wenuy_file_view_get_file:
 * Returns: (transfer none) (nullable): filen som visas
 */

/**
 * wenuy_document_checkpoint:
 * @self: dokumentet
 * @label: (nullable): vad som ändrades ("Skrivning")
 * @merge_key: (nullable): snabba ändringar med samma nyckel blir ett ångra-steg
 */

/**
 * wenuy_document_bind_window:
 * @self: dokumentet
 * @window: fönstret
 * @app_name: (nullable): programmets namn i titeln
 */

/**
 * wenuy_sheet_set:
 * @self: bladet
 * @col: kolumn (0 = A)
 * @row: rad (0 = 1)
 * @input: (nullable): "12", "Hej", "=A1*2" (tomt/%NULL tömmer)
 */
