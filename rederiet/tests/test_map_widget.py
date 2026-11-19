"""Cairo draw smoke tests with a narrow GTK stub (not a true GTK window test)."""
import importlib.util
import sys
import types
import unittest
from pathlib import Path
from unittest.mock import patch

import cairo
from engine import Game


class Colour:
    red = .31
    green = .52
    blue = .74
    alpha = 1.0


class Style:
    def get_color(self):
        return Colour()

    def lookup_color(self, name):
        return (True, Colour())


class DrawingArea:
    def __init__(self):
        self.draw_callback = None

    def set_content_width(self, n):
        pass

    def set_content_height(self, n):
        pass

    def set_hexpand(self, value):
        pass

    def set_vexpand(self, value):
        pass

    def set_draw_func(self, callback):
        self.draw_callback = callback

    def set_tooltip_text(self, value):
        pass

    def add_controller(self, controller):
        pass

    def get_style_context(self):
        return Style()

    def get_width(self):
        return 1000

    def get_height(self):
        return 460

    def queue_draw(self):
        pass


class Click:
    @classmethod
    def new(cls):
        return cls()

    def connect(self, name, callback):
        pass


def load_widget():
    gi = types.ModuleType('gi')
    gi.require_version = lambda *args: None
    repository = types.ModuleType('gi.repository')
    repository.Gtk = types.SimpleNamespace(DrawingArea=DrawingArea, GestureClick=Click)
    gi.repository = repository
    location = Path(__file__).parent.parent / 'map_widget.py'
    spec = importlib.util.spec_from_file_location('rederiet_test_map_widget', location)
    module = importlib.util.module_from_spec(spec)
    with patch.dict(sys.modules, {'gi': gi, 'gi.repository': repository}):
        spec.loader.exec_module(module)
    return module


class MapDrawTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.map = load_widget()

    def setUp(self):
        self.game = Game()
        self.game.buy_ship('freighter')
        self.game.sail('New York')
        self.game.select_ship(1)
        self.game.sail('Singapore')
        self.game.advance()

    def test_draw_all_regions(self):
        widget = self.map.WorldMap(lambda _: None)
        widget.game = self.game
        for region in self.map.VIEWS:
            with self.subTest(region=region):
                widget.region = region
                image = cairo.ImageSurface(cairo.FORMAT_ARGB32, 1000, 460)
                cr = cairo.Context(image)
                widget.draw(widget, cr, 1000, 460)
                image.flush()
                self.assertEqual(image.get_width(), 1000)

    def test_marker_count(self):
        widget = self.map.WorldMap(lambda _: None)
        widget.game = self.game
        pixels = widget.ship_pixels(1000, 460)
        self.assertEqual(len(pixels), 2)
        self.assertNotEqual(pixels[0][1:], pixels[1][1:])

    def test_pick_ship(self):
        selected = []
        widget = self.map.WorldMap(lambda _: None, selected.append)
        widget.game = self.game
        ship, x, y = widget.ship_pixels(1000, 460)[0]
        widget.pressed(None, 1, x, y)
        self.assertEqual(selected, [ship.id])

    def test_collocated_ships_remain_clickable(self):
        g = Game()
        g.money += 30000
        g.buy_ship('freighter')
        g.buy_ship('freighter')
        widget = self.map.WorldMap(lambda _: None)
        widget.game = g
        coords = [(x, y) for _, x, y in widget.ship_pixels(1000, 460)]
        self.assertEqual(len(set(coords)), len(g.ships))


if __name__ == '__main__':
    unittest.main()
