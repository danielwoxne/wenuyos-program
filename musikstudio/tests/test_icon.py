# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
from xml.etree import ElementTree
root=Path(__file__).resolve().parents[1]
id="se.wenuyos.app.musikstudio"
svg=root/"data"/(id+".svg")
desktop=root/"data"/(id+".desktop")
assert svg.stat().st_size > 250, "The source SVG icon must not be empty"
element=ElementTree.parse(svg).getroot()
assert element.tag.endswith("svg")
assert len(list(element)) >= 2, "The SVG must contain artwork"
text=desktop.read_text(encoding="utf-8")
for line in ("[Desktop Entry]", "Name[sv]=", "Name=", "Icon="+id, "Exec=wenuy-studio"):
    assert line in text, line
print("Wenuy Studio icon + launch metadata PASS")
