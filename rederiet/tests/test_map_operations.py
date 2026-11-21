"""GTK-stub drawing verification independent of Cairo installation.

These are *not* substitutes for real GTK4/labwc GUI tests.
"""
import unittest
from engine import Game
from test_map_widget import load_widget


class Context:
    def __init__(self):
        self.operations = {}
    def __getattr__(self, name):
        if name == 'text_extents':
            return lambda label: type('Extents', (), {'width': len(label) * 7})()
        def call(*args):
            self.operations[name] = self.operations.get(name, 0) + 1
        return call


class MapOperationsTests(unittest.TestCase):
    def test_all_views_and_harbour_pulse(self):
        module = load_widget()
        widget = module.WorldMap(lambda _: None)
        widget.game = Game()
        widget.port_pulse = True
        widget.game.advance()
        for region in module.VIEWS:
            widget.region = region
            cr = Context()
            widget.draw(widget, cr, 1000, 460)
            self.assertGreater(cr.operations.get('arc', 0), 0)
            self.assertGreater(cr.operations.get('stroke', 0), 0)

    def test_rivals_drawn_when_game_loaded(self):
        module = load_widget()
        widget = module.WorldMap(lambda _: None)
        widget.game = Game()
        cr = Context()
        widget.draw_rivals(cr, 1000, 460, type('Color', (), {
            'red': .5, 'green': .5, 'blue': .5, 'alpha': 1.0
        })())
        self.assertEqual(cr.operations.get('close_path', 0), len(widget.game.rivals))


if __name__ == '__main__':
    unittest.main()
