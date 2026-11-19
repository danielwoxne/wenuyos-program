"""Rederiet: deterministic, offline fleet management for low-memory hardware.

All monetary figures, weather, rates and sailing times are *game values*.
The only external dependency is ``world`` (a tiny static routing graph).
"""
from dataclasses import dataclass, asdict, field
from math import ceil
import json
from pathlib import Path
from world import PORTS as WORLD_PORTS, travel_days, ship_position
from simulation import Rival, RIVAL_NAMES, RIVAL_HOME, weather_at, port_activity, market_index

PORTS = tuple(WORLD_PORTS)
SAVE_VERSION = 4
MAX_FLEET = 8


@dataclass(frozen=True)
class ShipType:
    price: int
    capacity: int
    tank: int
    speed: float
    burn: int
    crew: int
    wear: int


# Prices in fictitious in-game kronor. Not quotes from real shipping markets.
SHIP_TYPES = {
    'coaster': ShipType(15000, 1, 100, 1.0, 3, 160, 2),
    'freighter': ShipType(19000, 2, 125, 1.0, 3, 240, 2),
    'ocean': ShipType(48000, 4, 220, 1.18, 3, 390, 2),
    'container': ShipType(105000, 8, 300, 1.4, 3, 620, 1),
}


def distance(a, b):
    if a not in WORLD_PORTS or b not in WORLD_PORTS:
        raise ValueError('bad_destination')
    return travel_days(a, b)


@dataclass
class Contract:
    origin: str
    destination: str
    pay: int
    deadline: int
    id: int
    size: int = 1


@dataclass
class Ship:
    id: int
    name: str
    kind: str
    port: str
    fuel: int = 100
    condition: int = 100
    voyage: dict | None = None
    cargo: list[Contract] = field(default_factory=list)
    voyages: int = 0
    weather_incidents: int = 0

    @property
    def specs(self):
        return SHIP_TYPES[self.kind]

    @property
    def capacity(self):
        return self.specs.capacity

    @property
    def used_capacity(self):
        return sum(c.size for c in self.cargo)

    def position(self):
        if not self.voyage:
            return WORLD_PORTS[self.port]
        v = self.voyage
        total = max(1, int(v['total']))
        elapsed = max(0, min(1, (total - int(v['days_left'])) / total))
        return ship_position(v.get('origin', self.port), v['destination'], elapsed)


class Game:
    def __init__(self):
        self.day = 1
        self.money = 22000
        self.ships = [Ship(1, 'Nordstjärnan', 'coaster', 'Göteborg')]
        self.selected_ship_id = 1
        self.next_ship_id = 2
        self.delivered = 0
        self.late = 0
        self.income = 0
        self.expenses = 0
        self.reputation = 50
        self.accepted_offer_ids = set()
        self.log = []
        # Retain these for readers of old game versions.
        self.next_id = 1
        self.rivals = [Rival(i + 1, RIVAL_NAMES[i], RIVAL_HOME[i])
                       for i in range(len(RIVAL_NAMES))]
        self.weather_delays = 0
        self.port_fees = 0

    @property
    def ship(self):
        return next(ship for ship in self.ships if ship.id == self.selected_ship_id)

    def select_ship(self, ship_id):
        if not isinstance(ship_id, int) or not any(s.id == ship_id for s in self.ships):
            raise ValueError('unknown_ship')
        self.selected_ship_id = ship_id

    # Backwards-compatible single-vessel facade: 0.1.0/0.2.0 callers.
    @property
    def port(self):
        return self.ship.port

    @port.setter
    def port(self, value):
        self.ship.port = value

    @property
    def fuel(self):
        return self.ship.fuel

    @fuel.setter
    def fuel(self, value):
        self.ship.fuel = value

    @property
    def condition(self):
        return self.ship.condition

    @condition.setter
    def condition(self, value):
        self.ship.condition = value

    @property
    def capacity(self):
        return self.ship.capacity

    @property
    def voyage(self):
        return self.ship.voyage

    @voyage.setter
    def voyage(self, value):
        self.ship.voyage = value

    @property
    def cargo(self):
        return self.ship.cargo[0] if self.ship.cargo else None

    @cargo.setter
    def cargo(self, value):
        self.ship.cargo = [value] if value is not None else []

    def position(self):
        return self.ship.position()

    def note(self, message):
        self.log.insert(0, f'Day {self.day}: {message}')
        self.log = self.log[:35]

    def voyage_days(self, destination, ship=None):
        ship = ship or self.ship
        return ceil(distance(ship.port, destination) / ship.specs.speed)

    def fuel_needed(self, destination, ship=None):
        ship = ship or self.ship
        return self.voyage_days(destination, ship) * ship.specs.burn

    def fuel_price(self, port=None):
        """Small fixed per-port differences; never requires network access."""
        port = port or self.port
        if port not in PORTS:
            raise ValueError('bad_destination')
        return 32 + (PORTS.index(port) * 7) % 19

    def refuel_quote(self):
        return (self.ship.specs.tank - self.fuel) * self.fuel_price()

    def repair_quote(self):
        return (100 - self.condition) * 60

    def company_value(self):
        return self.money + sum(self.sale_quote(s) for s in self.ships)

    @staticmethod
    def sale_quote(ship):
        return ship.specs.price * ship.condition * 55 // 10000

    def weather(self, ship=None):
        ship = ship or self.ship
        return weather_at(self.day, *ship.position())

    def port_activity(self, port=None):
        return port_activity(port or self.port, self.day)

    def market_index(self, port=None):
        return market_index(port or self.port, self.day, self.rivals)

    def generate_offers(self):
        """Legacy API: offers are now computed per port/day without side effects."""
        return self.offers

    @property
    def offers(self):
        if self.voyage is not None:
            return []
        here = self.port
        options = sorted((distance(here, p), p) for p in PORTS if p != here)
        buckets = (options[:5], options[5:10], options[10:])
        offers = []
        for index, bucket in enumerate(buckets):
            if not bucket:
                continue
            length, destination = bucket[(self.day // 4 + index) % len(bucket)]
            for variant in range(2):
                # The same route has a small and larger consignment: a small
                # ship can take one, a larger ship may load both.
                size = 1 if variant == 0 else 2
                base = (2900 + length * 1350) * size
                market = 90 + ((self.day * 3 + PORTS.index(here) * 5 + index * 7) % 26)
                pay = base * market * self.market_index(here) // 10000
                identifier = self.day * 1000 + PORTS.index(here) * 20 + index * 2 + variant
                if identifier in self.accepted_offer_ids:
                    continue
                offers.append(Contract(here, destination, pay,
                                       self.day + length + 6 + (size - 1), identifier, size))
        return offers

    def accept(self, contract_id):
        ship = self.ship
        if ship.voyage:
            raise ValueError('ship_busy')
        offer = next((o for o in self.offers if o.id == contract_id), None)
        if not offer or offer.origin != ship.port:
            raise ValueError('offer_unavailable')
        if ship.cargo and ship.cargo[0].destination != offer.destination:
            raise ValueError('cargo_route_mismatch')
        if ship.used_capacity + offer.size > ship.capacity:
            raise ValueError('cargo_capacity')
        ship.cargo.append(offer)
        self.accepted_offer_ids.add(offer.id)
        self.note(f'{ship.name}: cargo loaded for {offer.destination} ({offer.size} TEU)')

    def sail(self, destination):
        ship = self.ship
        if ship.voyage:
            raise ValueError('already_sailing')
        if destination not in PORTS or destination == ship.port:
            raise ValueError('bad_destination')
        if ship.cargo and ship.cargo[0].destination != destination:
            raise ValueError('cargo_route_mismatch')
        days = self.voyage_days(destination)
        fuel_cost = self.fuel_needed(destination)
        if ship.fuel < fuel_cost:
            raise ValueError('not_enough_fuel')
        if ship.condition < days * ship.specs.wear:
            raise ValueError('needs_repair')
        ship.fuel -= fuel_cost
        ship.voyage = {'origin': ship.port, 'destination': destination,
                       'days_left': days, 'total': days, 'storm_delays': 0}
        self.note(f'{ship.name}: sailed for {destination}')

    def advance(self):
        self.day += 1
        daily_cost = sum(ship.specs.crew for ship in self.ships)
        self.money -= daily_cost
        self.expenses += daily_cost
        for ship in self.ships:
            if not ship.voyage:
                continue
            weather = self.weather(ship)
            # Ships move every day, with the occasional storm adding one day.
            # Maximum three storm delays per voyage prevents endless stranding.
            penalty = ship.specs.wear + (2 if weather.severity == 2 else
                                         5 if weather.severity == 3 else 0)
            ship.condition = max(0, ship.condition - penalty)
            if weather.severity >= 2:
                ship.weather_incidents += 1
                if weather.severity == 3 and ship.voyage.get('storm_delays', 0) < 3:
                    if ship.voyage['total'] < 95:
                        ship.voyage['storm_delays'] = ship.voyage.get('storm_delays', 0) + 1
                        ship.voyage['total'] += 1
                        ship.voyage['days_left'] += 1
                        self.weather_delays += 1
                        self.note(f'{ship.name}: storm delayed voyage +1 day')
            ship.voyage['days_left'] -= 1
            if ship.voyage['days_left'] > 0:
                continue
            ship.port = ship.voyage['destination']
            ship.voyage = None
            ship.voyages += 1
            self.note(f'{ship.name}: arrived in {ship.port}')
            # Port operations are a game cost. Port activity varies daily.
            dues = 50 + self.port_activity(ship.port) * 30
            self.money -= dues
            self.expenses += dues
            self.port_fees += dues
            self.note(f'{ship.name}: harbour dues -{dues} kr')
            for cargo in ship.cargo:
                if cargo.destination != ship.port:
                    continue
                on_time = self.day <= cargo.deadline
                earned = cargo.pay if on_time else cargo.pay // 2
                self.money += earned
                self.income += earned
                self.delivered += 1
                if not on_time:
                    self.late += 1
                    self.reputation = max(0, self.reputation - 3)
                else:
                    self.reputation = min(100, self.reputation + 1)
                self.note(f'{ship.name}: cargo delivered +{earned} kr' +
                          (' (late)' if not on_time else ''))
            ship.cargo = [cargo for cargo in ship.cargo if cargo.destination != ship.port]
        for rival in self.rivals:
            event = rival.advance(self.day)
            if event == 'arrived':
                self.note(f'{rival.name}: rival delivered cargo in {rival.port}')
        # Contracts are daily port-specific offers and expire automatically.
        self.accepted_offer_ids.clear()
        if self.money < -15000:
            self.note('Company bankrupt')

    def refuel(self):
        if self.voyage:
            raise ValueError('at_sea')
        unit_price = self.fuel_price()
        units = min(self.ship.specs.tank - self.fuel,
                    max(0, self.money // unit_price))
        if units <= 0:
            raise ValueError('refuel_unavailable')
        price = units * unit_price
        self.money -= price
        self.expenses += price
        self.fuel += units
        self.note(f'{self.ship.name}: fuel purchased +{units} -{price} kr')

    def repair(self):
        if self.voyage:
            raise ValueError('at_sea')
        points = min(100 - self.condition, max(0, self.money // 60))
        if points <= 0:
            raise ValueError('repair_unavailable')
        price = points * 60
        self.money -= price
        self.expenses += price
        self.condition += points
        self.note(f'{self.ship.name}: ship repaired +{points} -{price} kr')

    def buy_ship(self, kind):
        if kind not in SHIP_TYPES:
            raise ValueError('bad_ship_type')
        if self.voyage:
            raise ValueError('at_sea')
        if len(self.ships) >= MAX_FLEET:
            raise ValueError('fleet_full')
        specs = SHIP_TYPES[kind]
        if self.money < specs.price:
            raise ValueError('not_enough_money')
        ship = Ship(self.next_ship_id, f'{kind.title()} {self.next_ship_id}',
                    kind, self.port, specs.tank, 100)
        self.money -= specs.price
        self.expenses += specs.price
        self.next_ship_id += 1
        self.ships.append(ship)
        self.selected_ship_id = ship.id
        self.note(f'{ship.name}: ship purchased -{specs.price} kr')
        return ship

    def sell_ship(self):
        ship = self.ship
        if len(self.ships) <= 1:
            raise ValueError('last_ship')
        if ship.voyage or ship.cargo:
            raise ValueError('ship_busy')
        price = self.sale_quote(ship)
        self.money += price
        self.income += price
        self.ships.remove(ship)
        self.selected_ship_id = self.ships[0].id
        self.note(f'{ship.name}: ship sold +{price} kr')
        return price

    def save(self, path):
        data = {
            'version': SAVE_VERSION, 'day': self.day, 'money': self.money,
            'ships': [asdict(ship) for ship in self.ships],
            'selected_ship_id': self.selected_ship_id,
            'next_ship_id': self.next_ship_id,
            'delivered': self.delivered, 'late': self.late,
            'income': self.income, 'expenses': self.expenses,
            'reputation': self.reputation,
            'accepted_offer_ids': sorted(self.accepted_offer_ids),
            'log': self.log[:35],
            'weather_delays': self.weather_delays,
            'port_fees': self.port_fees,
            'rivals': [asdict(r) for r in self.rivals],
        }
        p = Path(path)
        p.parent.mkdir(parents=True, exist_ok=True)
        temp = p.with_name(p.name + '.tmp')
        temp.write_text(json.dumps(data, ensure_ascii=False), encoding='utf-8')
        temp.replace(p)

    @classmethod
    def load(cls, path):
        source = Path(path)
        if source.stat().st_size > 1024 * 1024:
            raise ValueError('Invalid save: too large')
        data = json.loads(source.read_text(encoding='utf-8'))
        if not isinstance(data, dict):
            raise ValueError('Invalid save')
        g = cls()
        try:
            g.day = data['day']
            g.money = data['money']
            g.delivered = data['delivered']
            g.log = data['log']
            if 'ships' not in data:
                # Version 0.1/0.2 had one global vessel and one cargo slot.
                legacy_cargo = [Contract(**data['cargo'])] if data.get('cargo') else []
                old_voyage = data.get('voyage')
                if old_voyage is not None and 'storm_delays' not in old_voyage:
                    old_voyage = {**old_voyage, 'storm_delays': 3}
                g.ships = [Ship(1, 'Nordstjärnan', 'coaster', data['port'],
                                data['fuel'], data['condition'], old_voyage,
                                legacy_cargo)]
                # In the old save `capacity` was an unused static field.
            else:
                g.ships = []
                for raw in data['ships']:
                    raw = dict(raw)
                    raw['cargo'] = [Contract(**c) for c in raw.get('cargo', [])]
                    if raw.get('voyage') is not None and 'storm_delays' not in raw['voyage']:
                        raw['voyage']['storm_delays'] = 3  # preserve pre-0.4 ETA
                    g.ships.append(Ship(**raw))
                g.selected_ship_id = data['selected_ship_id']
                g.next_ship_id = data['next_ship_id']
                g.late = data.get('late', 0)
                g.income = data.get('income', 0)
                g.expenses = data.get('expenses', 0)
                g.reputation = data.get('reputation', 50)
                g.accepted_offer_ids = set(data.get('accepted_offer_ids', []))
            # Preserve old 0.1-0.3 saved companies; rivals enter afresh.
            g.weather_delays = data.get('weather_delays', 0)
            g.port_fees = data.get('port_fees', 0)
            if 'rivals' in data:
                g.rivals = [Rival(**r) for r in data['rivals']]
            g.validate()
        except (KeyError, TypeError, AttributeError, OverflowError) as exc:
            raise ValueError('Invalid save') from exc
        return g

    def validate(self):
        def integer(v, lo, hi):
            return type(v) is int and lo <= v <= hi

        if not (integer(self.day, 1, 1_000_000) and
                integer(self.money, -1_000_000_000, 1_000_000_000) and
                integer(self.delivered, 0, 1_000_000) and
                integer(self.late, 0, self.delivered) and
                integer(self.income, 0, 1_000_000_000) and
                integer(self.expenses, 0, 1_000_000_000) and
                integer(self.reputation, 0, 100) and
                integer(self.weather_delays, 0, 1_000_000) and
                integer(self.port_fees, 0, 1_000_000_000) and
                isinstance(self.log, list) and len(self.log) <= 35 and
                all(isinstance(x, str) and len(x) <= 300 for x in self.log) and
                1 <= len(self.ships) <= MAX_FLEET):
            raise ValueError('Invalid save')
        ids = set()
        for s in self.ships:
            if (not integer(s.id, 1, 1_000_000) or s.id in ids or
                not isinstance(s.name, str) or len(s.name) > 60 or not s.name or
                s.kind not in SHIP_TYPES or s.port not in PORTS or
                not integer(s.fuel, 0, s.specs.tank) or
                not integer(s.condition, 0, 100) or
                not integer(s.voyages, 0, 1_000_000) or
                not integer(s.weather_incidents, 0, 1_000_000) or
                not isinstance(s.cargo, list) or len(s.cargo) > s.capacity):
                raise ValueError('Invalid save')
            ids.add(s.id)
            if sum(c.size for c in s.cargo) > s.capacity:
                raise ValueError('Invalid save')
            if s.cargo and len({c.destination for c in s.cargo}) != 1:
                raise ValueError('Invalid save')
            for c in s.cargo:
                if not (c.origin in PORTS and c.destination in PORTS and
                        integer(c.pay, 0, 1_000_000_000) and
                        integer(c.deadline, 1, 1_000_000) and
                        integer(c.id, 0, 1_000_000_000) and integer(c.size, 1, 8)):
                    raise ValueError('Invalid save')
            if s.voyage:
                v = s.voyage
                if not (isinstance(v, dict) and v.get('origin', s.port) in PORTS
                        and v.get('destination') in PORTS and
                        integer(v.get('total'), 1, 100) and
                        integer(v.get('storm_delays', 0), 0, 3) and
                        integer(v.get('days_left'), 1, v['total']) and
                        (not s.cargo or s.cargo[0].destination == v['destination'])):
                    raise ValueError('Invalid save')
        if (self.selected_ship_id not in ids or
                not integer(self.next_ship_id, max(ids) + 1, 1_000_001) or
                not isinstance(self.accepted_offer_ids, set) or
                len(self.accepted_offer_ids) > MAX_FLEET * 6 or
                not all(integer(x, 0, 1_000_000_000) for x in self.accepted_offer_ids)):
            raise ValueError('Invalid save')

        if (not isinstance(self.rivals, list) or len(self.rivals) != len(RIVAL_NAMES)):
            raise ValueError('Invalid save')
        rival_ids = set()
        for r in self.rivals:
            if (type(r.id) is not int or r.id not in range(1, len(RIVAL_NAMES) + 1)
                    or r.id in rival_ids or r.name != RIVAL_NAMES[r.id - 1]
                    or r.port not in PORTS
                    or not integer(r.deliveries, 0, 1_000_000)
                    or not integer(r.earnings, 0, 1_000_000_000)):
                raise ValueError('Invalid save')
            rival_ids.add(r.id)
            if r.destination is None:
                if (r.origin is not None or r.days_left != 0 or r.total != 0):
                    raise ValueError('Invalid save')
            elif (r.destination not in PORTS or r.origin not in PORTS
                  or r.destination == r.origin
                  or not integer(r.total, 1, 90)
                  or not integer(r.days_left, 1, r.total)):
                raise ValueError('Invalid save')
