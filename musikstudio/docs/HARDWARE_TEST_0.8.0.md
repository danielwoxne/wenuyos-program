# Wenuy Studio 0.8.0 – hardware test release
This branch carries the complete X11/XWayland 0.8.0 preview source for hardware testing. It supersedes the earlier incomplete 0.4.1/GTK4 import for this **explicitly experimental** release.
- C++17, offline, GPL-3.0-or-later.
- Demo, multitrack MIDI, effects, loop, waveform, basic recording, undo/redo.
- Code builds only when X11 development headers are available (no silent CLI-only install).
- Runtime must supply libX11, xwayland and libportaudio2.
- CI shall run `wenuyos verify`; physical microphone and user hardware are not yet tested.
- UI still has known English-only messages and hardcoded X11 styling: blockers for a stable WenuyOS-native release.
- .wstudio v5 opens v2-v4 but older versions cannot read v5; back up older projects.
- No binaries, ZIP, WAPP, tokens or secrets are committed.
