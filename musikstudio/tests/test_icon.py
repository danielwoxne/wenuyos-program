# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
from xml.etree import ElementTree
root=Path(__file__).resolve().parents[1]
id="se.wenuyos.app.musikstudio"
svg=root/"data"/(id+".svg")
desktop=root/"data"/(id+".desktop")
mime=root/"data"/"mime"/"packages"/(id+".xml")
code=(root/"gui"/"GtkStudio.cpp").read_text(encoding="utf-8")
assert svg.stat().st_size>250,"SVG artwork must not be empty"
assert ElementTree.parse(svg).getroot().tag.endswith("svg")
d=desktop.read_text(encoding="utf-8")
for line in ("[Desktop Entry]","Name=","Name[sv]=","Icon="+id,
             "Exec=wenuy-studio %f","MimeType=application/x-wenuy-studio;"):
    assert line in d,line
rootxml=ElementTree.parse(mime).getroot()
assert rootxml.tag.endswith("mime-info")
assert any(e.get("pattern")=="*.wstudio" for e in rootxml.iter() if e.tag.endswith("glob"))
assert "G_APPLICATION_HANDLES_OPEN" in code
assert 'g_signal_connect(app,"open"' in code and "openProjectFiles" in code
assert "fileAction(st,Pick::Open,filename)" in code
assert "gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu)" in code
assert "adw_header_bar_pack_end" in code
assert "buttonScroll" not in code
assert "gtk_widget_set_size_request(s->window,690,480)" in code

# Reviewer #3: abandoned X11/duplicate WAPP packagers must not ship.
for old in ("gui/XStudio.cpp", "tools/build-wapp.sh", "tools/make-wapp.py"):
    assert not (root/old).exists(), f"Obsolete file should be removed: {old}"
# Reviewer #4: test artifacts must be unique across all program pull requests.
workflow=(root.parent/".github"/"workflows"/"program.yml").read_text(encoding="utf-8")
assert 'name: program-test-${{ github.event.pull_request.number }}' in workflow
print("Desktop, MIME, native file open, icon and compact layout PASS")
