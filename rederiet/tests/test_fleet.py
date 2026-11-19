"""Regressions for fleet operations, contract capacity and save migration."""
import json
import tempfile
import unittest
from pathlib import Path

from engine import Game, Ship, Contract, SHIP_TYPES, MAX_FLEET
from world import PORTS


class FleetTests(unittest.TestCase):
    def test_purchase_select_and_sale(self):
        g = Game()
        new_ship = g.buy_ship('freighter')
        self.assertEqual(len(g.ships), 2)
        self.assertEqual(g.ship.id, new_ship.id)
        self.assertEqual(g.money, 3000)
        self.assertEqual(g.capacity, 2)
        g.select_ship(1)
        self.assertEqual(g.port, 'Göteborg')
        g.select_ship(new_ship.id)
        quote = g.sale_quote(g.ship)
        self.assertEqual(g.sell_ship(), quote)
        self.assertEqual(len(g.ships), 1)
        self.assertEqual(g.money, 3000 + quote)
        self.assertEqual(g.ship.id, 1)

    def test_unaffordable_unknown_max_fleet(self):
        g = Game()
        with self.assertRaisesRegex(ValueError, 'bad_ship_type'):
            g.buy_ship('starship')
        with self.assertRaisesRegex(ValueError, 'not_enough_money'):
            g.buy_ship('container')
        g.money = 1000000
        for _ in range(MAX_FLEET - 1):
            g.buy_ship('freighter')
        with self.assertRaisesRegex(ValueError, 'fleet_full'):
            g.buy_ship('coaster')
        with self.assertRaisesRegex(ValueError, 'unknown_ship'):
            g.select_ship(10000)

    def test_last_vessel_not_for_sale(self):
        with self.assertRaisesRegex(ValueError, 'last_ship'):
            Game().sell_ship()

    def test_freighter_can_carry_two_contracts_on_one_route(self):
        g = Game()
        g.buy_ship('freighter')
        contracts = g.offers[:2]
        self.assertEqual(contracts[0].destination, contracts[1].destination)
        self.assertEqual([c.size for c in contracts], [1, 2])
        g.accept(contracts[0].id)
        with self.assertRaisesRegex(ValueError, 'cargo_capacity'):
            g.accept(contracts[1].id)
        # An ocean ship with four slots can load both before sailing.
        g.money += 100000
        g.buy_ship('ocean')
        g.advance()  # The market refreshes; previously accepted lots stay taken.
        fresh = g.offers[:2]
        self.assertEqual(fresh[0].destination, fresh[1].destination)
        g.accept(fresh[0].id)
        g.accept(fresh[1].id)
        self.assertEqual(g.ship.used_capacity, 3)
        g.sail(fresh[0].destination)
        for _ in range(g.voyage['total']):
            g.advance()
        self.assertEqual(g.delivered, 2)
        self.assertEqual(len(g.ship.cargo), 0)

    def test_incompatible_destination_or_sale_with_cargo(self):
        g = Game()
        g.buy_ship('freighter')
        g.accept(g.offers[0].id)
        with self.assertRaisesRegex(ValueError, 'cargo_route_mismatch'):
            g.accept(g.offers[2].id)
        with self.assertRaisesRegex(ValueError, 'cargo_route_mismatch'):
            g.sail(g.offers[2].destination)
        with self.assertRaisesRegex(ValueError, 'ship_busy'):
            g.sell_ship()

    def test_two_ships_advance_independently(self):
        g = Game()
        g.buy_ship('freighter')
        first_id, second_id = 1, g.ship.id
        g.select_ship(first_id)
        first_offer = g.offers[0]
        g.accept(first_offer.id)
        g.sail(first_offer.destination)
        pos_before = g.position()
        g.select_ship(second_id)
        other_offer = g.offers[2]
        g.accept(other_offer.id)
        g.sail(other_offer.destination)
        g.advance()
        g.select_ship(first_id)
        self.assertNotEqual(g.position(), pos_before)
        g.select_ship(second_id)
        self.assertTrue(g.voyage)
        for _ in range(35):
            g.advance()
        self.assertGreaterEqual(g.delivered, 2)
        self.assertTrue(all(s.voyage is None for s in g.ships))

    def test_wages_are_per_ship(self):
        g = Game()
        g.buy_ship('freighter')
        previous = g.money
        g.advance()
        self.assertEqual(g.money, previous - SHIP_TYPES['coaster'].crew - SHIP_TYPES['freighter'].crew)

    def test_refuel_pricing_and_independent_fuel(self):
        g = Game()
        g.buy_ship('freighter')
        id_b = g.ship.id
        g.fuel = 50
        quote = g.refuel_quote()
        self.assertEqual(quote, (125 - 50) * g.fuel_price())
        start = g.money
        g.money += 10000
        g.refuel()
        self.assertEqual(g.fuel, 125)
        self.assertEqual(g.money, start + 10000 - quote)
        g.select_ship(1)
        self.assertEqual(g.fuel, 100)
        g.select_ship(id_b)
        self.assertEqual(g.fuel, 125)

    def test_partial_refuel_and_repair_in_a_cash_crunch(self):
        g = Game()
        g.fuel = 0
        g.condition = 40
        fuel_cost = g.fuel_price()
        g.money = fuel_cost * 7 + 1
        g.refuel()
        self.assertEqual(g.fuel, 7)
        self.assertEqual(g.money, 1)
        with self.assertRaisesRegex(ValueError, 'repair_unavailable'):
            g.repair()
        g.money = 125
        g.repair()
        self.assertEqual(g.condition, 42)
        self.assertEqual(g.money, 5)
        self.assertEqual(g.expenses, fuel_cost * 7 + 120)

    def test_sail_with_cargo_blocked_from_wrong_port(self):
        g = Game()
        offer = g.offers[0]
        g.accept(offer.id)
        with self.assertRaisesRegex(ValueError, 'cargo_route_mismatch'):
            g.sail('Singapore' if offer.destination != 'Singapore' else 'Oslo')

    def test_save_reload_including_two_vessels_at_sea(self):
        g = Game()
        g.buy_ship('freighter')
        g.sail('New York')
        g.select_ship(1)
        g.sail('Oslo')
        g.advance()
        with tempfile.TemporaryDirectory() as temp:
            save = Path(temp) / 'save.json'
            g.save(save)
            h = Game.load(save)
            self.assertEqual(h.money, g.money)
            self.assertEqual(h.selected_ship_id, 1)
            self.assertEqual([s.position() for s in h.ships],
                             [s.position() for s in g.ships])
            self.assertEqual(len(h.ships), 2)

    def test_legacy_cargo_file(self):
        old = {'day': 3, 'money': 10000, 'port': 'Göteborg', 'fuel': 94,
               'condition': 98, 'capacity': 1, 'voyage': None,
               'cargo': {'origin': 'Göteborg', 'destination': 'Oslo',
                         'pay': 6400, 'deadline': 10, 'id': 5},
               'delivered': 0, 'next_id': 6, 'offers': [], 'log': ['Day 1: Cargo loaded']}
        with tempfile.TemporaryDirectory() as temp:
            save = Path(temp) / 'game.json'
            save.write_text(json.dumps(old), encoding='utf-8')
            g = Game.load(save)
            self.assertEqual(g.cargo.destination, 'Oslo')
            self.assertEqual(g.cargo.size, 1)
            g.sail('Oslo')
            for _ in range(g.voyage['total']):
                g.advance()
            self.assertEqual(g.delivered, 1)

    def test_invalid_current_saves_rejected(self):
        base = Game()
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'save.json'
            base.save(path)
            original = json.loads(path.read_text(encoding='utf-8'))
            mutations = [
                lambda d: d['ships'][0].update(kind='xyz'),
                lambda d: d['ships'][0].update(fuel=100000),
                lambda d: d.update(selected_ship_id=999),
                lambda d: d['ships'][0].update(port='Atlantis'),
                lambda d: d['ships'][0].update(id=-100),
                lambda d: d.update(day=-42),
                lambda d: d.update(accepted_offer_ids=list(range(40))),
                lambda d: d.update(log=['x'] * 300),
                lambda d: d['ships'].append(d['ships'][0]),
            ]
            for mutate in mutations:
                with self.subTest(case=mutate):
                    data = json.loads(json.dumps(original))
                    mutate(data)
                    path.write_text(json.dumps(data), encoding='utf-8')
                    with self.assertRaises(ValueError):
                        Game.load(path)

    def test_ship_supply_not_same_as_real_world_navigation(self):
        g = Game()
        self.assertEqual(len(PORTS), 15)
        self.assertNotEqual(g.fuel_price('Oslo'), g.fuel_price('Singapore'))
        self.assertTrue(all(g.fuel_needed(dest) <= g.ship.specs.tank
                            for dest in PORTS if dest != 'Göteborg'))

    def test_offer_generation_deterministic_no_duplicate_after_loading(self):
        g = Game()
        first = g.offers
        self.assertEqual(first, g.offers)
        g.accept(first[0].id)
        self.assertNotIn(first[0].id, [x.id for x in g.offers])
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'game.json'
            g.save(path)
            h = Game.load(path)
            self.assertEqual(h.offers, g.offers)
            h.advance()
            self.assertTrue(h.offers)

    def test_no_money_generation_buy_then_sell(self):
        g = Game()
        first = g.money
        ship = g.buy_ship('freighter')
        g.sell_ship()
        self.assertLess(g.money, first)


if __name__ == '__main__':
    unittest.main()
