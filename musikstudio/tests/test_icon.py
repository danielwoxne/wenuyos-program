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

# Protect every destructive entry point, including shell/file-manager open and window close.
assert '#include "wenuystudio/ProjectGuard.hpp"' in code
assert 'ws::ProjectGuard guard{}' in code
assert 'requestProtected(st,{ws::ProjectActionKind::New,{}})' in code
assert 'requestProtected(s,{ws::ProjectActionKind::Open,path})' in code
assert 'requestProtected(st,{ws::ProjectActionKind::Close,{}})' in code
assert 'g_signal_connect(s->window,"close-request"' in code
assert 'adw_message_dialog_new(GTK_WINDOW(s->window)' in code
assert 'st->guard.beginSave()' in code
assert 's->guard.finishedSave(true)' in code
assert 's->guard.finishedSave(false)' in code
assert 'ws::Project loaded=ws::loadProject(action.path)' in code
assert code.index('ws::Project loaded=ws::loadProject(action.path)') < code.index('s->project=std::move(loaded)')
# System Datatyper is a conditional capability; never remove the native WAV fallback.
assert '#include "wenuystudio/AudioImport.hpp"' in code
assert 'ws::importAudioFile(path,importDir.u8string())' in code
assert 'tr("Import audio","Importera ljud")' in code
assert 'tr("Import audio…","Importera ljud…")' in code
# The 3D renderer runs in a separate process: the GTK client never links GFX.
assert 'GSubprocess* process=g_subprocess_newv' in code
assert 'Pick::ExportGfx' in code
assert 'tr("Export 3D note map…","Exportera 3D-notkarta…")' in code
assert 'WenuyGFX::wenuygfx' not in (root/"gui"/"GtkStudio.cpp").read_text()
# Virtual keyboard: live GTK controls must be present on both languages.
assert '#include "wenuystudio/KeyboardInput.hpp"' in code
assert '#include "wenuystudio/MidiTake.hpp"' in code
assert 'createKeyboard(s)' in code
assert 'ws::keyboardKeyAt(x,y' in code
assert 'ws::keyboardComputerKey(gdk_keyval_to_lower(keyval))' in code
assert 'tr("Record MIDI","Spela in MIDI")' in code
assert 'tr("Stop MIDI recording","Stoppa MIDI-inspelning")' in code
assert 's->midi.noteOn(key,g_get_monotonic_time(),velocity)' in code
assert 's->midi.noteOff(key,g_get_monotonic_time())' in code
assert 's->midi.finish(g_get_monotonic_time())' in code
assert 's->midi.recording()' in code
assert 's->audition.start(preview,0)' in code
# External MIDI is opt-in only; no enumeration or connection occurs at startup.
assert '#include "wenuystudio/MidiInput.hpp"' in code
assert 'gtk_button_new_with_label(tr("Scan MIDI devices","Sök MIDI-enheter"))' in code
assert 'tr("Connect MIDI","Anslut MIDI")' in code
assert 'tr("Disconnect MIDI","Koppla från MIDI")' in code
assert 's->midiInput.scan(error)' in code
assert 's->midiInput.connect(source,' in code
assert 's->midiInput.disconnect()' in code
assert 'releaseExternalKeys(s)' in code
assert 's->externalKeys' in code
assert 'createExternalMidiUi(s)' in code
print("Desktop, MIME, project guard, icon and compact layout PASS")
