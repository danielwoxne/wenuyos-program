"""Small deterministic, offline shipping management model. Values are fictional."""
from dataclasses import dataclass, asdict
import json
from pathlib import Path
from world import PORTS as WORLD_PORTS, travel_days, ship_position

PORTS = tuple(WORLD_PORTS)


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

class Game:
    def __init__(self):
        self.day = 1
        self.money = 22000
        self.port = 'Göteborg'
        self.fuel = 100
        self.condition = 100
        self.capacity = 1
        self.voyage = None
        self.cargo = None
        self.delivered = 0
        self.next_id = 1
        self.offers = []
        self.log = []
        self.generate_offers()

    def note(self, msg):
        self.log.insert(0, f'Day {self.day}: {msg}')
        self.log = self.log[:25]

    def generate_offers(self):
        # Predictable offers; no external service or network access.
        self.offers = []
        here = self.port if self.voyage is None else self.voyage['destination']
        # One short, one medium and one long route. Rotate periodically.
        options = sorted((distance(here, p), p) for p in PORTS if p != here)
        buckets = (options[:5], options[5:10], options[10:])
        for bucket in buckets:
            if not bucket:
                continue
            length, destination = bucket[(self.day // 4) % len(bucket)]
            self.offers.append(Contract(here, destination, 4000 + length * 1700,
                                        self.day + length + 5, self.next_id))
            self.next_id += 1

    def accept(self, contract_id):
        if self.voyage or self.cargo:
            raise ValueError('ship_busy')
        offer = next((o for o in self.offers if o.id == contract_id), None)
        if not offer or offer.origin != self.port:
            raise ValueError('offer_unavailable')
        self.cargo = offer
        self.offers.clear()
        self.note('Cargo loaded')

    def sail(self, destination):
        if self.voyage:
            raise ValueError('already_sailing')
        if destination not in PORTS or destination == self.port:
            raise ValueError('bad_destination')
        days = distance(self.port, destination)
        fuel_cost = days * 3
        if self.fuel < fuel_cost:
            raise ValueError('not_enough_fuel')
        if self.condition < days * 4:
            raise ValueError('needs_repair')
        self.fuel -= fuel_cost
        self.voyage = {'origin': self.port, 'destination': destination, 'days_left': days, 'total': days}
        self.note(f'Sailed for {destination}')

    def position(self):
        if not self.voyage:
            return WORLD_PORTS[self.port]
        v = self.voyage
        origin = v.get('origin', self.port)  # save files created by 0.1.0
        total = max(1, int(v['total']))
        elapsed = (total - int(v['days_left'])) / total
        return ship_position(origin, v['destination'], min(1, max(0, elapsed)))

    def advance(self):
        self.day += 1
        self.money -= 160  # daily crew costs
        if self.voyage:
            self.voyage['days_left'] -= 1
            self.condition = max(0, self.condition - 4)
            if self.voyage['days_left'] <= 0:
                self.port = self.voyage['destination']
                self.voyage = None
                self.note(f'Arrived in {self.port}')
                if self.cargo and self.cargo.destination == self.port:
                    self.money += self.cargo.pay if self.day <= self.cargo.deadline else self.cargo.pay // 2
                    self.delivered += 1
                    self.note('Cargo delivered')
                    self.cargo = None
                self.generate_offers()
        elif not self.cargo:
            self.generate_offers()
        if self.money < -15000:
            self.note('Company bankrupt')

    def refuel(self):
        if self.voyage:
            raise ValueError('at_sea')
        units = 100 - self.fuel
        price = units * 45
        if not units or self.money < price:
            raise ValueError('refuel_unavailable')
        self.money -= price
        self.fuel = 100
        self.note('Fuel purchased')

    def repair(self):
        if self.voyage:
            raise ValueError('at_sea')
        points = 100 - self.condition
        price = points * 70
        if not points or self.money < price:
            raise ValueError('repair_unavailable')
        self.money -= price
        self.condition = 100
        self.note('Ship repaired')

    def save(self, path):
        data = {k: getattr(self, k) for k in ('day','money','port','fuel','condition','capacity','voyage','delivered','next_id','log')}
        data['cargo'] = asdict(self.cargo) if self.cargo else None
        data['offers'] = [asdict(c) for c in self.offers]
        p = Path(path)
        p.parent.mkdir(parents=True, exist_ok=True)
        temp = p.with_suffix('.tmp')
        temp.write_text(json.dumps(data, ensure_ascii=False), encoding='utf-8')
        temp.replace(p)

    @classmethod
    def load(cls, path):
        data = json.loads(Path(path).read_text(encoding='utf-8'))
        g = cls()
        for key in ('day','money','port','fuel','condition','capacity','voyage','delivered','next_id','log'):
            setattr(g, key, data[key])
        g.cargo = Contract(**data['cargo']) if data['cargo'] else None
        g.offers = [Contract(**c) for c in data['offers']]
        if (g.port not in PORTS or not 0 <= g.fuel <= 100
                or not 0 <= g.condition <= 100
                or (g.voyage and (g.voyage.get('origin', g.port) not in PORTS
                    or g.voyage.get('destination') not in PORTS
                    or not 0 < g.voyage.get('total', 0) <= 100
                    or not 0 < g.voyage.get('days_left', 0) <= g.voyage['total']))
                or (g.cargo and (g.cargo.origin not in PORTS or g.cargo.destination not in PORTS))
                or any(o.origin not in PORTS or o.destination not in PORTS for o in g.offers)):
            raise ValueError('Invalid save')
        return g
