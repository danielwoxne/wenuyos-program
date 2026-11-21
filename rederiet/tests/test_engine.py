import unittest
import tempfile
from pathlib import Path
from engine import Game, PORTS, distance

class GameTests(unittest.TestCase):
    def test_symmetric_distances(self):
        for a in PORTS:
            for b in PORTS:
                self.assertEqual(distance(a,b),distance(b,a))
    def test_delivery(self):
        g=Game(); offer=g.offers[0]; g.accept(offer.id); g.sail(offer.destination)
        for _ in range(distance(offer.origin,offer.destination)): g.advance()
        self.assertEqual(g.delivered,1)
        self.assertIsNone(g.cargo)
        self.assertEqual(g.port,offer.destination)
    def test_save_load(self):
        g=Game(); g.accept(g.offers[0].id)
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'save.json'; g.save(p); h=Game.load(p)
            self.assertEqual(h.cargo.destination,g.cargo.destination)
            self.assertEqual(h.money,g.money)
    def test_cannot_sail_without_fuel(self):
        g=Game(); g.fuel=0
        with self.assertRaisesRegex(ValueError,'not_enough_fuel'):g.sail('Oslo')
    def test_no_duplicate_accept(self):
        g=Game(); offer=g.offers[0];g.accept(offer.id)
        with self.assertRaises(ValueError):g.accept(offer.id)

if __name__=='__main__':unittest.main()
