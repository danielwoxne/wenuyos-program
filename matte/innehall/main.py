#!/usr/bin/env python3
"""Matte – ett WenuyOS-program i Python (GTK 4 + libadwaita + libwenuy).
Kör: `wenuyos run` (eller `python3 main.py`). Utan argument öppnas fönstret;
`python3 main.py check|kort|regelbok|ova` är kommandoraden (se matte/__main__.py)."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from matte.__main__ import main
sys.exit(main() or 0)
