"""Small deterministic offline simulation for sea weather and rival shipping.

All numbers and conditions are imaginary game mechanics, not real forecasts.
No global random state, network requests, or assets.
"""
from dataclasses import dataclass
from world import PORTS, ship_position, travel_days

WEATHER = ('calm', 'breeze', 'rough', 'storm')
RIVAL_NAMES = ('Atlantic Meridian', 'Baltic Star Lines', 'Pacific Union')
RIVAL_HOME = ('New York', 'Rotterdam', 'Singapore')


@dataclass(frozen=True)
class SeaWeather:
    key: str
    severity: int


def weather_at(day, latitude, longitude):
    """Repeatable position/day based weather; independent of player selection.

    Very infrequent storms, more likely in higher latitudes. This simplified
    game forecast is intentionally NOT suitable for real navigation.
    """
    lat = round(latitude * 2)
    lon = round(longitude * 2)
    seed = (day * 37 + lat * 11 + lon * 7 + (day // 30) * 13) % 43
    if seed == 0 or (abs(latitude) > 45 and seed == 1):
        return SeaWeather('storm', 3)
    if seed < 7:
        return SeaWeather('rough', 2)
    if seed < 20:
        return SeaWeather('breeze', 1)
    return SeaWeather('calm', 0)


def port_activity(port, day):
    """Port traffic, 1-3, determines decorative harbour signals and fees."""
    if port not in PORTS:
        raise ValueError('bad_destination')
    return 1 + (list(PORTS).index(port) * 7 + day // 2) % 3


@dataclass
class Rival:
    id: int
    name: str
    port: str
    destination: str | None = None
    origin: str | None = None
    days_left: int = 0
    total: int = 0
    deliveries: int = 0
    earnings: int = 0

    def position(self):
        if self.destination and self.origin and self.total > 0:
            ratio = max(0, min(1, (self.total - self.days_left) / self.total))
            return ship_position(self.origin, self.destination, ratio)
        return PORTS[self.port]

    def advance(self, day):
        """A rival sails on its own schedule; no user action or hidden network."""
        if self.destination:
            self.days_left -= 1
            if self.days_left > 0:
                return None
            self.port = self.destination
            self.destination = None
            self.origin = None
            self.days_left = self.total = 0
            self.deliveries += 1
            self.earnings += 2500 + (self.id * 370) + (day % 11) * 80
            return 'arrived'
        # Schedule a departure every other day; deterministic and bounded.
        if (day + self.id) % 3 != 0:
            candidates = tuple(p for p in PORTS if p != self.port)
            target = candidates[(day // 3 + self.id * 5 + self.deliveries) % len(candidates)]
            self.origin = self.port
            self.destination = target
            self.total = min(90, max(1, travel_days(self.port, target)))
            self.days_left = self.total
            return 'departed'
        return None


def market_index(port, day, rivals):
    """Freight market (%) from traffic cycle and rival capacity around port."""
    if port not in PORTS:
        raise ValueError('bad_destination')
    competition = sum(1 for r in rivals if (r.destination is None and r.port == port)
                      or r.destination == port)
    return max(75, min(110, 98 + (day + list(PORTS).index(port) * 3) % 9 - competition * 7))
