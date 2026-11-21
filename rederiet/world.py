"""Tiny offline maritime atlas for Rederiet.

Coordinates are illustrative WGS84 port/coastal positions. Shipping lanes are
schematic gameplay routes, not real-world navigation advice. No online map data.
"""
import heapq
from math import asin, ceil, cos, radians, sin, sqrt

# Latitude, longitude. Kept deliberately compact for low-memory computers.
PORTS = {
    'Göteborg': (57.70, 11.96),
    'Hamburg': (53.55, 9.99),
    'Rotterdam': (51.92, 4.48),
    'London': (51.50, 0.35),
    'Oslo': (59.91, 10.75),
    'New York': (40.66, -74.05),
    'Rio de Janeiro': (-22.91, -43.17),
    'Kapstaden': (-33.91, 18.43),
    'Dubai': (25.28, 55.27),
    'Mumbai': (18.95, 72.84),
    'Singapore': (1.28, 103.84),
    'Shanghai': (31.23, 121.50),
    'Tokyo': (35.45, 139.74),
    'Sydney': (-33.86, 151.21),
    'Los Angeles': (33.74, -118.27),
}

# Waypoints on water, including simplified Suez/Panama canal connections.
WAYPOINTS = {
    'skagerrak': (58.10, 8.70),
    'north_sea': (55.5, 4.5),
    'elbe': (54.1, 8.4),
    'thames': (51.60, 1.55),
    'channel': (50.0, -1.5),
    'biscay': (44.3, -9.0),
    'gibraltar': (36.0, -5.5),
    'med': (35.5, 18.0),
    'suez': (30.0, 32.55),
    'red_sea': (19.0, 38.5),
    'aden': (12.3, 45.5),
    'arabian': (14.0, 62.0),
    'gulf_oman': (24.5, 59.5),
    'sri_lanka': (5.5, 80.5),
    'malacca': (3.6, 99.3),
    'south_china': (13.2, 113.7),
    'east_china': (27.5, 126.0),
    'atl_ne': (47.0, -14.0),
    'atl_mid': (35.0, -40.0),
    'us_east': (40.0, -67.0),
    'atl_equator': (-1.0, -30.0),
    'atl_south': (-31.0, -14.0),
    'indian': (-20.0, 62.0),
    'java': (-5.2, 113.3),
    'timor': (-9.0, 127.0),
    'coral': (-17.2, 155.0),
    'north_pacific': (40.0, 175.0),
    'east_pacific': (39.0, -157.0),
    'south_pacific': (-16.0, -166.0),
    'panama_p': (8.7, -81.0),
    'panama_a': (10.0, -79.8),
    'caribbean': (20.5, -74.0),
    'us_west': (31.0, -119.0),
}

# Shown as simple polygons on the world overview. Intentionally low detail.
CONTINENTS = (
    # North America
    ((-168, 68), (-156, 72), (-141, 69), (-130, 61), (-125, 52), (-124, 43),
     (-117, 32), (-112, 29), (-106, 24), (-97, 16), (-91, 17), (-86, 21),
     (-83, 25), (-81, 30), (-75, 35), (-69, 44), (-60, 47), (-57, 53),
     (-65, 59), (-78, 62), (-93, 73), (-113, 72), (-130, 72), (-147, 66)),
    # Greenland
    ((-53, 59), (-43, 60), (-39, 66), (-41, 75), (-49, 82), (-58, 83), (-62, 75), (-60, 67)),
    # South America
    ((-80, 11), (-73, 13), (-66, 10), (-60, 11), (-51, 5), (-48, 0),
     (-35, -5), (-37, -14), (-41, -23), (-48, -29), (-53, -34),
     (-63, -41), (-68, -55), (-75, -50), (-72, -37), (-75, -20), (-81, -5)),
    # Europe + northern Asia
    ((-10, 36), (-8, 44), (-10, 50), (-6, 54), (0, 58), (4, 56),
     (8, 57), (10, 62), (16, 70), (23, 71), (28, 66), (33, 59),
     (28, 53), (36, 45), (44, 42), (51, 41), (59, 48), (70, 50),
     (80, 54), (90, 52), (99, 55), (109, 56), (119, 54), (130, 49),
     (138, 53), (145, 50), (153, 60), (164, 64), (178, 67),
     (179, 51), (166, 50), (151, 42), (140, 39), (132, 34), (125, 38),
     (121, 33), (116, 23), (109, 19), (105, 9), (100, 2),
     (97, 8), (99, 15), (104, 20), (101, 24), (94, 22), (89, 21),
     (85, 20), (79, 9), (73, 8), (68, 23), (60, 25), (55, 25),
     (49, 30), (43, 36), (38, 40), (33, 41), (28, 40), (23, 38),
     (18, 40), (14, 44), (9, 44), (4, 43), (-1, 43), (-5, 36)),
    # Africa
    ((-17, 30), (-14, 35), (-5, 36), (4, 37), (11, 34), (19, 32),
     (25, 31), (33, 30), (36, 22), (43, 12), (51, 11), (49, 1),
     (42, -11), (35, -22), (32, -29), (26, -35), (18, -34),
     (13, -28), (10, -16), (12, -6), (9, 4), (5, 5), (-2, 5),
     (-9, 11), (-17, 20)),
    # Arabia
    ((34, 30), (43, 28), (48, 30), (55, 25), (58, 20), (52, 16),
     (44, 12), (42, 16), (39, 23)),
    # Australia
    ((113, -22), (115, -16), (125, -13), (135, -12), (138, -16),
     (145, -15), (153, -25), (151, -34), (145, -38), (135, -35),
     (126, -34), (115, -31)),
    # Antarctica
    ((-180, -76), (-145, -72), (-105, -75), (-64, -72), (-30, -74),
     (10, -71), (50, -76), (85, -73), (130, -72), (180, -76), (180, -85), (-180, -85)),
    # Japan (schematic)
    ((130, 32), (134, 35), (138, 38), (141, 43), (145, 43), (144, 39), (140, 35), (136, 33)),
    # UK
    ((-6, 50), (-3, 51), (0, 55), (-2, 58), (-5, 57), (-6, 54)),
    # Madagascar
    ((48, -12), (50, -17), (49, -24), (46, -26), (44, -19)),
)

# Undirected links. Ports are connected to nearest safe waterway nodes.
EDGES = (
    ('Göteborg', 'skagerrak'), ('Oslo', 'skagerrak'), ('skagerrak', 'north_sea'),
    ('Göteborg', 'north_sea'), ('Hamburg', 'elbe'), ('elbe', 'north_sea'),
    ('Rotterdam', 'north_sea'), ('Rotterdam', 'channel'),
    ('London', 'thames'), ('thames', 'channel'),
    ('north_sea', 'channel'), ('channel', 'biscay'),
    ('biscay', 'gibraltar'), ('gibraltar', 'med'), ('med', 'suez'),
    ('suez', 'red_sea'), ('red_sea', 'aden'), ('aden', 'arabian'),
    ('arabian', 'gulf_oman'), ('gulf_oman', 'Dubai'),
    ('arabian', 'Mumbai'), ('arabian', 'sri_lanka'),
    ('sri_lanka', 'malacca'), ('malacca', 'Singapore'),
    ('Singapore', 'south_china'), ('south_china', 'east_china'),
    ('east_china', 'Shanghai'), ('east_china', 'Tokyo'),
    ('Shanghai', 'Tokyo'),
    ('Singapore', 'java'), ('java', 'timor'), ('timor', 'coral'),
    ('coral', 'Sydney'),
    ('gibraltar', 'atl_ne'), ('atl_ne', 'atl_mid'),
    ('atl_mid', 'us_east'), ('us_east', 'New York'),
    ('atl_mid', 'atl_equator'), ('atl_equator', 'Rio de Janeiro'),
    ('atl_equator', 'atl_south'), ('atl_south', 'Rio de Janeiro'),
    ('atl_south', 'Kapstaden'), ('Kapstaden', 'indian'),
    ('indian', 'sri_lanka'), ('indian', 'aden'),
    ('Tokyo', 'north_pacific'), ('north_pacific', 'east_pacific'),
    ('east_pacific', 'us_west'), ('us_west', 'Los Angeles'),
    ('Los Angeles', 'panama_p'), ('panama_p', 'panama_a'),
    ('panama_a', 'caribbean'), ('caribbean', 'New York'),
    ('caribbean', 'atl_equator'),
    ('coral', 'south_pacific'), ('south_pacific', 'panama_p'),
)

NODES = dict(WAYPOINTS, **PORTS)
GRAPH = {p: [] for p in NODES}


def nautical_miles(a, b):
    """Great-circle distance between (latitude, longitude) pairs."""
    lat1, lon1 = map(radians, a)
    lat2, lon2 = map(radians, b)
    x = sin((lat2-lat1)/2)**2 + cos(lat1)*cos(lat2)*sin((lon2-lon1)/2)**2
    return 3440.065 * 2 * asin(min(1.0, sqrt(max(0.0, x))))


for start, end in EDGES:
    d = nautical_miles(NODES[start], NODES[end])
    GRAPH[start].append((end, d))
    GRAPH[end].append((start, d))


def sea_route(origin, destination):
    """Coordinates of the shortest *schematic* lane through the fixed graph."""
    if origin not in PORTS or destination not in PORTS:
        raise ValueError('bad_destination')
    if origin == destination:
        return (PORTS[origin],)
    queue = [(0.0, origin)]
    lengths = {origin: 0.0}
    previous = {}
    while queue:
        dist, node = heapq.heappop(queue)
        if dist > lengths[node]:
            continue
        if node == destination:
            result = [node]
            while result[-1] != origin:
                result.append(previous[result[-1]])
            return tuple(NODES[p] for p in reversed(result))
        for neighbor, length in GRAPH[node]:
            new = dist + length
            if new < lengths.get(neighbor, float('inf')):
                lengths[neighbor] = new
                previous[neighbor] = node
                heapq.heappush(queue, (new, neighbor))
    raise ValueError('route_missing')


def route_length(points):
    return sum(nautical_miles(a, b) for a, b in zip(points, points[1:]))


def travel_days(a, b):
    """Game days, not nautical forecasts. Short hops are at least two days."""
    if a == b:
        return 0
    return max(2, ceil(route_length(sea_route(a, b)) / 510))


def ship_position(origin, destination, fraction):
    """Interpolated point along route, weighted by sailing miles, dateline safe."""
    points = sea_route(origin, destination)
    if len(points) == 1 or fraction <= 0:
        return points[0]
    if fraction >= 1:
        return points[-1]
    lengths = [nautical_miles(a, b) for a, b in zip(points, points[1:])]
    remaining = float(fraction) * sum(lengths)
    for (lat1, lon1), (lat2, lon2), length in zip(points, points[1:], lengths):
        if remaining <= length:
            alpha = remaining / length if length else 0
            longitude_step = ((lon2 - lon1 + 180) % 360) - 180
            longitude = ((lon1 + alpha * longitude_step + 180) % 360) - 180
            return (lat1 + alpha * (lat2 - lat1), longitude)
        remaining -= length
    return points[-1]


def project(latitude, longitude, width, height):
    """Equirectangular projection of world, with cropped polar caps."""
    return ((longitude + 180) * width / 360,
            (82 - latitude) * height / 164)


def unwrap_segments(coords):
    """Split at longitude ±180 so map routes do not cross the full screen."""
    if not coords:
        return []
    lines = [[coords[0]]]
    for lat, lon in coords[1:]:
        plat, plon = lines[-1][-1]
        delta = lon - plon
        if abs(delta) <= 180:
            lines[-1].append((lat, lon))
            continue
        adjusted = lon - 360 if delta > 180 else lon + 360
        target = -180 if delta > 180 else 180
        t = (target - plon)/(adjusted - plon)
        middle_lat = plat + (lat - plat) * t
        lines[-1].append((middle_lat, target))
        lines.append([(middle_lat, -target), (lat, lon)])
    return lines
