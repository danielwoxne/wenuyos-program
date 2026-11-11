# Provprotokoll – Wenuy Studio 0.8.0 (11 november 2026)

## Prov genomförda på Debian 13-utvecklingsvärd

- Kompilering med GCC 14 / CMake, Release x86_64: godkänd.
- `ctest --test-dir build --output-on-failure`: 4 av 4 godkända (integrerat 0.7-prov, MIDI-sequencer 0.5, mixer/arrangemang 0.6, EditHistory 0.8).
- GUI-startprov `xvfb-run -a build/wenuy-studio --smoke-test`: godkänt (X11-fönster mappas).
- Startprov med `WENUYOS_PERF=low WENUYOS_GFX=soft` och begränsad virtuell minnesrymd (600 MB): godkänt. Detta visar inte att en komplett session fungerar på 2 GB RAM.
- Kommandoradsexport av exempelprojekt: giltig stereo-PCM16-WAV, 48 kHz.
- Kontroll av `.wapp`-innehåll: 6 filkontrollsummor matchar.
- Ingen nätverksfunktion byggs in eller startas av musikstudion.

## Ej genomförda prov

- `wenuyos verify`, `bash tools/check/pr-kontroll.sh` i en lokal New-wenuyos-klon.
- Full GUI-provkörning i WenuyOS på dator med 2 GB RAM och utan GPU.
- Fysisk PortAudio-uppspelning/inspelning, flerkanalsstresstest och latency/underrun-mätning.
- Automatisk fullständig översättningsgranskning: flera status-/feltexter är ännu endast engelska.
- Verifiering av signatur/installation på WenuyOS, eller Windows-installation.

## GitHub-integrering

Se `danielwoxne/wenuyos-program` PR #6. Denna lokala X11-förhandsversion är inte samma sak som GitHub-grenens GTK4-baserade 0.4.1 + EditModel-integrering. Skillnaden måste lösas innan ett officiellt 0.8-släpp.
