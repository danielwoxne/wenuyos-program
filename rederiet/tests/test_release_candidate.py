"""RC1 offline game goal and map fallback tests."""
import unittest
from pathlib import Path
from engine import Game
from test_map_widget import cairo, load_widget

class CampaignGoal(unittest.TestCase):
    def test_goal_not_reached_at_start(self):
        self.assertFalse(Game().won)

    def test_goal_requires_deliveries_and_value(self):
        g=Game()
        g.delivered=10
        self.assertFalse(g.won)
        g.money=50_000
        self.assertTrue(g.won)
        g.delivered=0
        self.assertFalse(g.won)

@unittest.skipUnless(cairo is not None, "PyCairo unavailable")
class Atlas(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.map=load_widget()

    def test_missing_asset_still_draws(self):
        widget=self.map.WorldMap(lambda _:None)
        widget._background_checked=True
        widget._background=None
        surface=cairo.ImageSurface(cairo.FORMAT_ARGB32,800,450)
        widget.draw(widget,cairo.Context(surface),800,450)

    def test_actual_atlas_when_packaged(self):
        asset=Path(__file__).resolve().parent.parent / 'share/world-map.png'
        if not asset.is_file():
            self.skipTest("Atlas PNG must be added by binary asset uploader")
        surface=cairo.ImageSurface.create_from_png(str(asset))
        self.assertGreater(surface.get_width(),1000)
        widget=self.map.WorldMap(lambda _:None)
        for region in self.map.VIEWS:
            widget.region=region
            img=cairo.ImageSurface(cairo.FORMAT_ARGB32,800,450)
            widget.draw(widget,cairo.Context(img),800,450)
            self.assertIsNotNone(widget._background)
