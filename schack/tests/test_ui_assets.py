#!/usr/bin/env python3
"""Lightweight regression checks for the Wenuy Schack launcher and compact UI.

No GTK display required. Runtime window launch is verified by wenuyos verify in CI.
"""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

root = Path(sys.argv[1])
desktop = (root / "data/se.wenuyos.app.schack.desktop").read_text(encoding="utf-8")
svg = root / "data/se.wenuyos.app.schack.svg"
gui = (root / "src/RuntimeGtk.cpp").read_text(encoding="utf-8")
manifest = (root / "wenuyos-app.toml").read_text(encoding="utf-8")

for entry in (
    "Name=Wenuy Chess",
    "Name[sv]=Wenuy Schack",
    "Categories=Game;BoardGame;",
    "Exec=schack",
    "Icon=se.wenuyos.app.schack",
):
    assert entry in desktop, f"Missing launcher metadata: {entry}"

ET.parse(svg)
assert 'version = "0.5.0"' in manifest
assert 'needs_network = false' in manifest
for marker in (
    "case 14:app.focusBoard=!app.focusBoard",
    "g.gtk_scrolled_window_set_child(app.sidePanel,side)",
    'tr("Focus on board","Fokus på brädet")',
    'tr("High contrast","Hög kontrast")',
    "app.palette=(app.palette+1)%4",
    "if(keyLower=='l'){perform(14);return 1;}",
):
    assert marker in gui, f"Missing UI functionality: {marker}"
print("OK: bilingual launcher, SVG, focus mode, scrolling panel and contrast palette")
