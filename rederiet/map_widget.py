"""Lightweight GTK4 world overview; draws its own vectors with Cairo.

No map downloads, textures, network connection, timers or OpenGL. All visible
colours are derived from the active GTK theme's foreground/accent palette.
"""
import math
import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk
from world import CONTINENTS, PORTS, project, sea_route, unwrap_segments

# Lat/longitude view bounds: west, east, south, north.
VIEWS = {
    'world': (-180, 180, -82, 82),
    'europe': (-14, 25, 43, 65),
    'atlantic': (-90, 25, -45, 65),
    'asia': (53, 180, -45, 55),
}


class WorldMap(Gtk.DrawingArea):
    def __init__(self, select_destination, select_ship=None):
        super().__init__()
        self.game = None
        self.selected = 'Oslo'
        self.region = 'world'
        self.port_pulse = False
        self.select_destination = select_destination
        self.select_ship = select_ship
        self.set_content_width(760)
        self.set_content_height(280)
        self.set_hexpand(True)
        self.set_vexpand(False)
        self.set_draw_func(self.draw)
        self.set_tooltip_text('Select a destination port / Välj destinationshamn')
        click = Gtk.GestureClick.new()
        click.connect('pressed', self.pressed)
        self.add_controller(click)

    def xy(self, lat, lon, width, height):
        if self.region == 'world':
            return project(lat, lon, width, height)
        west, east, south, north = VIEWS[self.region]
        return ((lon-west) * width/(east-west), (north-lat)*height/(north-south))

    @staticmethod
    def color(cr, rgba, opacity):
        cr.set_source_rgba(rgba.red, rgba.green, rgba.blue,
                           max(0.0, min(1.0, opacity * rgba.alpha)))

    def accent(self):
        context = self.get_style_context()
        for name in ('accent_color', 'theme_selected_bg_color', 'accent_bg_color'):
            result = context.lookup_color(name)
            if result[0] and result[1] is not None:
                return result[1]
        return context.get_color()

    def land(self, cr, width, height, fg):
        for polygon in CONTINENTS:
            cr.new_path()
            first = True
            for lon, lat in polygon:
                x, y = self.xy(lat, lon, width, height)
                if first:
                    cr.move_to(x, y)
                    first = False
                else:
                    cr.line_to(x, y)
            cr.close_path()
            self.color(cr, fg, .12)
            cr.fill_preserve()
            cr.set_line_width(.8)
            self.color(cr, fg, .36)
            cr.stroke()

    def route(self, cr, coords, width, height, accent, opacity=.88):
        cr.save()
        self.color(cr, accent, opacity)
        cr.set_line_width(2.0)
        cr.set_dash((7, 5))
        for part in unwrap_segments(coords):
            cr.new_path()
            for i, (lat, lon) in enumerate(part):
                x, y = self.xy(lat, lon, width, height)
                if i == 0:
                    cr.move_to(x, y)
                else:
                    cr.line_to(x, y)
            if len(part) > 1:
                cr.stroke()
        cr.restore()

    def draw_ship(self, cr, x, y, accent, fg, selected=False):
        cr.save()
        cr.translate(x, y)
        if selected:
            cr.scale(1.3, 1.3)
        x, y = 0, 0
        # Small arrow/ship silhouette, rendered directly rather than as an image.
        cr.new_path()
        cr.move_to(x, y-10)
        cr.line_to(x+7, y+7)
        cr.line_to(x, y+4)
        cr.line_to(x-7, y+7)
        cr.close_path()
        self.color(cr, accent, 1)
        cr.fill_preserve()
        cr.set_line_width(1.7)
        self.color(cr, fg, 1)
        cr.stroke()
        cr.restore()

    def ship_pixels(self, width, height):
        """Keep collocated vessels clickable without shifting actual coordinates."""
        if not self.game:
            return []
        counts = {}
        out = []
        for ship in self.game.ships:
            lat, lon = ship.position()
            px, py = self.xy(lat, lon, width, height)
            key = (round(px / 8), round(py / 8))
            offset = counts.get(key, 0)
            counts[key] = offset + 1
            out.append((ship, px + 9 * (offset % 3), py + 9 * (offset // 3)))
        return out

    def draw_rivals(self, cr, width, height, fg):
        if not self.game:
            return
        for rival in self.game.rivals:
            lat, lon = rival.position()
            x, y = self.xy(lat, lon, width, height)
            if 0 <= x <= width and 0 <= y <= height:
                cr.new_path()
                cr.move_to(x, y - 5)
                cr.line_to(x + 5, y)
                cr.line_to(x, y + 5)
                cr.line_to(x - 5, y)
                cr.close_path()
                self.color(cr, fg, .7)
                cr.set_line_width(1.5)
                cr.stroke()

    def draw(self, area, cr, width, height):
        cr.save()
        cr.rectangle(0, 0, width, height)
        cr.clip()
        fg = self.get_style_context().get_color()
        accent = self.accent()
        west, east, south, north = VIEWS[self.region]
        # Transparent ocean inherits the active GTK view background.
        self.color(cr, fg, .09)
        cr.set_line_width(.5)
        for longitude in range(-180, 181, 30):
            if west < longitude < east:
                x, _ = self.xy(0, longitude, width, height)
                cr.move_to(x, 0)
                cr.line_to(x, height)
        for latitude in range(-60, 81, 30):
            if south < latitude < north:
                _, y = self.xy(latitude, 0, width, height)
                cr.move_to(0, y)
                cr.line_to(width, y)
        cr.stroke()
        self.land(cr, width, height, fg)
        if self.game is not None:
            g = self.game
            # Draw all active voyages; selected vessel gets the brightest route.
            for ship in g.ships:
                if ship.voyage:
                    coords = sea_route(ship.voyage.get('origin', ship.port),
                                       ship.voyage['destination'])
                    self.route(cr, coords, width, height, accent,
                               .93 if ship.id == g.selected_ship_id else .29)
            # Rival lanes are intentionally subdued so the player's routes
            # remain legible even at 1024x600 and on software-only rendering.
            for rival in g.rivals:
                if rival.destination and rival.origin:
                    self.route(cr, sea_route(rival.origin, rival.destination),
                               width, height, fg, .19)
            if not g.voyage and self.selected != g.port:
                self.route(cr, sea_route(g.port, self.selected),
                           width, height, accent, .9)

        # Display labels in priority order; avoid overlapping text in overview.
        order = [self.selected]
        if self.game:
            order.append(self.game.port)
        order += list(PORTS)
        occupied = []
        for port in dict.fromkeys(order):
            lat, lon = PORTS[port]
            x, y = self.xy(lat, lon, width, height)
            if not (-8 <= x <= width+8 and -8 <= y <= height+8):
                continue
            selected = port == self.selected
            current = self.game and port == self.game.port
            cr.arc(x, y, 5.5 if selected else 3.0, 0, 2 * math.pi)
            self.color(cr, accent if (selected or current) else fg, 1 if selected else .75)
            cr.fill()
            if self.game and self.port_pulse:
                # Single-frame flash following Next day, using theme colours.
                cr.new_path()
                cr.arc(x, y, 6 + 2 * self.game.port_activity(port), 0, 2 * math.pi)
                self.color(cr, accent, .38)
                cr.set_line_width(1.4)
                cr.stroke()
            # Pixel-aligned labels, no bitmap resource or font package needed.
            if any(abs(x - other_x) < 80 and abs(y - other_y) < 17
                   for other_x, other_y in occupied):
                continue
            occupied.append((x, y))
            if width < 450 and not (selected or current):
                continue
            label = port
            cr.select_font_face('Sans', 0, 0)
            cr.set_font_size(10.5)
            text_width = cr.text_extents(label).width
            tx = x + 7 if x + text_width + 12 < width else x - text_width - 8
            ty = max(12, min(height - 5, y - 7))
            self.color(cr, fg, 1)
            cr.move_to(tx, ty)
            cr.show_text(label)
        self.draw_rivals(cr, width, height, fg)
        for ship, x, y in self.ship_pixels(width, height):
            if -15 < x < width + 15 and -15 < y < height + 15:
                selected = ship.id == self.game.selected_ship_id
                self.draw_ship(cr, x, y, accent if selected else fg, fg, selected)
        cr.restore()

    def pressed(self, gesture, count, x, y):
        width = self.get_width()
        height = self.get_height()
        if self.select_ship and self.game:
            nearest = min(self.ship_pixels(width, height),
                          key=lambda item: (item[1] - x) ** 2 + (item[2] - y) ** 2)
            if (nearest[1] - x) ** 2 + (nearest[2] - y) ** 2 <= 14 ** 2:
                self.select_ship(nearest[0].id)
                self.queue_draw()
                return
        best = None
        best_d = 15 ** 2
        for port, (lat, lon) in PORTS.items():
            px, py = self.xy(lat, lon, width, height)
            d = (x-px)**2 + (y-py)**2
            if d < best_d:
                best, best_d = port, d
        if best:
            self.selected = best
            self.select_destination(best)
            self.queue_draw()
