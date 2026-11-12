"""Färger och typsnitt – samma palett som WenuyOS (skarpa hörn, 1 px-linjer,
orange accent) och de milda färgpennefärgerna från regelboken."""

TYPSNITT = "Inter, sans-serif"

TEXT = "#1f2328"
TEXT_SVAG = "#5c6670"
LINJE = "#3a3f44"
VIT = "#ffffff"
BAKGRUND = "#f4f4f2"
KORT = "#ffffff"
ACCENT = "#e8821c"       # WenuyOS orange (knappen Töm)
ACCENT_SVAG = "#fdeedc"
ROD = "#d1403a"
GRON = "#3f9a4a"

# Färgpennorna
FARGER = {
    "gul": "#f3ea8f",
    "bla": "#b9c6ea",
    "rosa": "#f0b7cf",
    "gron": "#b8d4a4",
    "ljusgul": "#efe9a9",
    "himmel": "#78b2dd",
    "sand": "#f2d08c",
    "aprikos": "#f2c6a6",
    "mint": "#b7d9bd",
    "beige": "#e6cfa8",
    "lila": "#6f5aa8",
    "orange": "#f1a63c",
    "magenta": "#e8609a",
    "skog": "#4f9a55",
    "citron": "#f0e24a",
    "rod": "#d9534f",
    "tegel": "#b85c4a",
    "stal": "#8c9ac2",
}


def farg(namn: str) -> str:
    """Slår upp en färgpenna; tar även emot en #hex-färg direkt."""
    if namn.startswith("#"):
        return namn
    return FARGER.get(namn, FARGER["gul"])
