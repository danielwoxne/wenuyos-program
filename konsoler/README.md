# Konsoler och hemdatorer

Spelkonsoler och hemdatorer i Retrospel: NES, SNES, Game Boy/Color/Advance, Sega Mega Drive/Master System/Game Gear,
PC Engine, PlayStation, Nintendo 64, Nintendo DS, GameCube/Wii och PSP – och Atari ST, ZX Spectrum, MSX och Atari 800.

Förut en del av Retrospel i WenuyOS; sedan 20 nov 2026 ett eget program (Daniel 20 nov: "Ta bort även konsolerna" →
konsoler och hemdatorer, eget program; New-wenuyos förslag #352). Retrospel läser `retro-tillagg.py` och visar spelen
i samma lista som förut.

| | |
|---|---|
| `retro-tillagg.py` | systemen, emulatorerna (Mednafen, Hatari, Fuse, openMSX, Atari800, Mupen64Plus, DeSmuME, Dolphin, PPSSPP), kommandona, BIOS/TOS, tangenterna |
| `main.py` | fönstret: systemen och deras emulatorer, BIOS och TOS, helskärm |
| `tests/` | `WENUYOS_RETRO=…/wenuyos-retro bash tests/run.sh` (på WenuyOS hittas Retrospel själv) |

**Nätverk:** inget av sig självt. En emulator installeras från Debian (PSP: Flathub) först när du trycker Installera,
i ett terminalfönster. Inga spel, BIOS eller ROM:ar följer med.
