"""New 0.4 regressions: weather, storms, rival companies and legacy saves."""
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from engine import Game, SHIP_TYPES
from simulation import Rival, SeaWeather, market_index, port_activity, weather_at
from world import PORTS


class SimulationTests(unittest.TestCase):
    def test_weather_repeatable_and_wide_range(self):
        for day in range(1, 60):
            for lat, lon in PORTS.values():
                self.assertEqual(weather_at(day, lat, lon), weather_at(day, lat, lon))
        severities = {weather_at(day, 58.0, 12.0).severity for day in range(1, 120)}
        self.assertEqual(severities, {0, 1, 2, 3})

    def test_port_activity_is_bounded(self):
        self.assertEqual({port_activity('Göteborg', d) for d in range(1, 20)}, {1, 2, 3})
        with self.assertRaisesRegex(ValueError, 'bad_destination'):
            port_activity('Atlantis', 2)

    def test_storm_delays_capped_then_arrival(self):
        game = Game()
        game.sail('Hamburg')
        base_days = game.voyage['total']
        with patch('engine.weather_at', return_value=SeaWeather('storm', 3)):
            for _ in range(base_days + 3):
                game.advance()
        self.assertEqual(game.port, 'Hamburg')
        self.assertIsNone(game.voyage)
        self.assertEqual(game.weather_delays, 3)
        self.assertEqual(game.ship.weather_incidents, base_days + 3)
        self.assertLess(game.condition, 100)

    def test_rough_seas_cause_extra_wear_not_delay(self):
        game = Game()
        game.sail('Oslo')
        with patch('engine.weather_at', return_value=SeaWeather('rough', 2)):
            game.advance()
        self.assertEqual(game.condition, 100 - SHIP_TYPES['coaster'].wear - 2)
        self.assertEqual(game.weather_delays, 0)

    def test_rival_departure_and_delivery(self):
        game = Game()
        rival = game.rivals[0]
        first = rival.position()
        for _ in range(100):
            game.advance()
        self.assertGreater(rival.deliveries, 0)
        self.assertGreater(rival.earnings, 0)
        self.assertIn(rival.port, PORTS)
        self.assertEqual(len(game.rivals), 3)
        self.assertNotEqual(rival.position(), first)

    def test_competition_depresses_local_prices(self):
        game = Game()
        before = game.market_index()
        old = game.offers[0].pay
        game.rivals[0].port = 'Göteborg'
        self.assertLess(game.market_index(), before)
        self.assertLess(game.offers[0].pay, old)
        self.assertGreater(market_index('Göteborg', 1, game.rivals), 0)

    def test_harbour_fees_charged_on_arrival_only(self):
        game = Game()
        game.sail('Oslo')
        game.advance()
        self.assertEqual(game.port_fees, 0)
        for _ in range(10):
            if game.voyage is None:
                break
            game.advance()
        self.assertIsNone(game.voyage)
        self.assertEqual(game.port_fees, 50 + game.port_activity('Oslo') * 30)

    def test_rivals_and_weather_survive_save_load(self):
        game = Game()
        game.sail('Hamburg')
        for _ in range(15):
            game.advance()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'save.json'
            game.save(path)
            loaded = Game.load(path)
            self.assertEqual([vars(r) for r in loaded.rivals],
                             [vars(r) for r in game.rivals])
            self.assertEqual(loaded.weather_delays, game.weather_delays)
            self.assertEqual(loaded.port_fees, game.port_fees)
            self.assertEqual(loaded.ship.weather_incidents, game.ship.weather_incidents)

    def test_legacy_v3_voyage_eta_kept(self):
        old = Game()
        old.sail('Rotterdam')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'save.json'
            old.save(path)
            state = json.loads(path.read_text())
            del state['rivals'], state['weather_delays'], state['port_fees']
            del state['ships'][0]['weather_incidents']
            del state['ships'][0]['voyage']['storm_delays']
            state['version'] = 3
            path.write_text(json.dumps(state))
            updated = Game.load(path)
            self.assertEqual(updated.voyage['storm_delays'], 3)
            with patch('engine.weather_at', return_value=SeaWeather('storm', 3)):
                for _ in range(updated.voyage['total']):
                    updated.advance()
            self.assertEqual(updated.port, 'Rotterdam')

    def test_reject_invalid_rivals_and_bad_weather_stats(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'save.json'
            Game().save(path)
            baseline = json.loads(path.read_text())
            for change in (
                lambda s: s['rivals'][0].update(port='Moon'),
                lambda s: s['rivals'][0].update(name='Forged'),
                lambda s: s['rivals'][0].update(days_left=1),
                lambda s: s.update(weather_delays=-1),
                lambda s: s.update(port_fees=1_000_000_001),
                lambda s: s['ships'][0].update(weather_incidents=-2),
                lambda s: s['rivals'].append(s['rivals'][0]),
            ):
                s = json.loads(json.dumps(baseline))
                change(s)
                path.write_text(json.dumps(s))
                with self.subTest(data=s):
                    with self.assertRaises(ValueError):
                        Game.load(path)

    def test_simulated_500_day_stress_on_low_hardware(self):
        game = Game()
        for _ in range(500):
            game.advance()
        game.validate()
        self.assertEqual(game.day, 501)
        self.assertLessEqual(len(game.log), 35)
        self.assertTrue(all(r.deliveries >= 1 for r in game.rivals))


if __name__ == '__main__':
    unittest.main()
