import json
import tempfile
import unittest
from pathlib import Path

from engine import Game, PORTS, distance
from world import (PORTS as COORDS, project, sea_route, ship_position,
                   nautical_miles, travel_days, unwrap_segments)


class WorldTests(unittest.TestCase):
    def test_all_ports_connected_with_symmetric_routes(self):
        self.assertGreaterEqual(len(PORTS), 14)
        for a in PORTS:
            for b in PORTS:
                self.assertEqual(distance(a, b), distance(b, a))
                coords = sea_route(a, b)
                self.assertEqual(coords[0], COORDS[a])
                self.assertEqual(coords[-1], COORDS[b])
                self.assertEqual(travel_days(a, b) == 0, a == b)

    def test_geographic_distance(self):
        self.assertEqual(nautical_miles(COORDS['Göteborg'], COORDS['Göteborg']), 0)
        self.assertGreater(nautical_miles(COORDS['Tokyo'], COORDS['New York']), 5000)

    def test_dateline_routes_split(self):
        # Line across the Pacific must not render as one across Eurasia.
        coords = sea_route('Tokyo', 'Los Angeles')
        parts = unwrap_segments(coords)
        self.assertGreaterEqual(len(parts), 2)
        for part in parts:
            for (_, lon1), (_, lon2) in zip(part, part[1:]):
                self.assertLessEqual(abs(lon2 - lon1), 180.001)

    def test_interpolation_endpoints(self):
        for a, b in [('Göteborg', 'Hamburg'), ('Tokyo', 'Los Angeles'),
                     ('Singapore', 'Sydney'), ('Oslo', 'Oslo')]:
            self.assertEqual(ship_position(a, b, 0), COORDS[a])
            self.assertEqual(ship_position(a, b, 1), COORDS[b])
            for f in (0.125, 0.25, 0.5, 0.75, .875):
                lat, lon = ship_position(a, b, f)
                self.assertLessEqual(abs(lat), 90)
                self.assertLessEqual(abs(lon), 180)

    def test_map_projection(self):
        self.assertEqual(project(82, -180, 720, 328), (0, 0))
        self.assertEqual(project(-82, 180, 720, 328), (720, 328))

    def test_ship_moves_each_day_and_arrives(self):
        g = Game()
        g.sail('Singapore')
        start = g.position()
        total = g.voyage['total']
        g.advance()
        self.assertNotEqual(g.position(), start)
        for _ in range(total - 1):
            g.advance()
        self.assertIsNone(g.voyage)
        self.assertEqual(g.position(), COORDS['Singapore'])

    def test_mid_voyage_load(self):
        g = Game()
        g.sail('Tokyo')
        for _ in range(4):
            g.advance()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'game.json'
            g.save(path)
            loaded = Game.load(path)
            self.assertEqual(loaded.position(), g.position())
            self.assertEqual(loaded.voyage['origin'], 'Göteborg')

    def test_old_save_without_origin(self):
        g = Game()
        g.sail('Rotterdam')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'game.json'
            g.save(path)
            data = json.loads(path.read_text(encoding='utf-8'))
            del data['voyage']['origin']  # field added in 0.2.0
            path.write_text(json.dumps(data), encoding='utf-8')
            loaded = Game.load(path)
            self.assertEqual(loaded.position(), COORDS['Göteborg'])
            for _ in range(loaded.voyage['days_left']):
                loaded.advance()
            self.assertEqual(loaded.port, 'Rotterdam')

    def test_contracts_cover_near_and_far(self):
        g = Game()
        self.assertEqual(len(g.offers), 3)
        choices = [distance(g.port, o.destination) for o in g.offers]
        self.assertGreater(max(choices), min(choices))
        self.assertTrue(all(c.origin == g.port and c.pay > 0 for c in g.offers))

    def test_long_contract_delivery(self):
        g = Game()
        contract = max(g.offers, key=lambda c: distance(c.origin, c.destination))
        g.accept(contract.id)
        g.sail(contract.destination)
        for _ in range(g.voyage['total']):
            g.advance()
        self.assertEqual(g.port, contract.destination)
        self.assertEqual(g.delivered, 1)
        self.assertIsNone(g.cargo)

    def test_no_invalid_ports_in_load(self):
        g = Game()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'game.json'
            g.save(path)
            data = json.loads(path.read_text(encoding='utf-8'))
            data['port'] = 'Atlantis'
            path.write_text(json.dumps(data), encoding='utf-8')
            with self.assertRaises(ValueError):
                Game.load(path)


if __name__ == '__main__':
    unittest.main()
