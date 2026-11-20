# Commodore 64

Commodore 64 i Retrospel med VICE: dina egna C64-ROM:ar eller fria Open ROMs, joystickport (t.ex. TheC64-joysticken),
helskärm och den blå BASIC-skärmen.

Förut en del av Retrospel och Inställningar → Emulatorer i WenuyOS; sedan 20 nov 2026 ett eget program (Daniel 19 nov:
"Ta även bort Commodore emuleringen" → bara C64, eget program; New-wenuyos förslag #352). Amiga stannar i Retrospel.

| | |
|---|---|
| `retro-tillagg.json` | beskrivningen som Retrospel läser: systemen, filtyperna, emulatorn, BIOS/ROM-krav, tipsen – bara data |
| `retro_kor.py` | programmets kommando: Retrospel startar det i en egen process när du trycker Spela (`play`) eller Lägg in (`add`) |
| `retro-kod.py` | koden bakom kommandot (flyttad ur Retrospel): emulatorernas kommandon, BIOS/ROM-igenkänning |
| `main.py` | fönstret: starta en C64, ROM-filerna, helskärm, joystickport (`main.py --basic` = direkt till BASIC) |
| `tests/` | `WENUYOS_RETRO=…/wenuyos-retro bash tests/run.sh` |

VICE och Open ROMs är Debian-paket (`requires`): att installera Commodore 64 är att välja C64-emulering, så de installeras
med programmet (installationsfönstret visar dem). Knappen Installera i fönstret finns bara om någon tagit bort VICE efteråt.
Inga Commodore-ROM:ar följer med.

**Säkerhet (ChatGPT 20 nov):** Retrospel kör aldrig programmets kod för att läsa in det – bara `retro-tillagg.json`,
och bara för program som installerats med wenuyos-get (`retro = "retro-tillagg.json"` i `wenuyos-app.toml`).
