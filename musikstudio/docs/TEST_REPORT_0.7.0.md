# Testprotokoll – Wenuy Studio 0.7.0 (2026-11-11)

- Byggmiljö: x86-64 Debianbaserad container, GCC C++17, CMake Release.
- `ctest`: 3/3 godkända: integrerad projekt/WAV/MIDI/FX, 0.5 MIDI/sequencer, 0.6 FX/arrangemang.
- WAV renderad: 48 kHz, 16-bit stereo (kontrollerad med `file`).
- GUI: Xvfb-fönster mappades korrekt med `--smoke-test`. En skärmdump sparades.
- Kvar: fysisk mikrofon, fysisk ljudutgång, uppmätt latens och CPU på målmaskin, 2GB RAM med stora projekt, WenuyOS-kompatibilitet med XWayland, säker migrering från gamla GTK-gränssnittet, LV2.
