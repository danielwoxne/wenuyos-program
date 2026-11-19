# Commodore 64

Commodore 64 i Retrospel med VICE: dina egna C64-ROM:ar eller fria Open ROMs, joystickport (t.ex. TheC64-joysticken),
helskärm och den blå BASIC-skärmen.

Förut en del av Retrospel och Inställningar → Emulatorer i WenuyOS; sedan 20 nov 2026 ett eget program (Daniel 19 nov:
"Ta även bort Commodore emuleringen" → bara C64, eget program; New-wenuyos förslag #352). Amiga stannar i Retrospel.

| | |
|---|---|
| `retro-tillagg.py` | C64 i Retrospel: filtyperna, ROM:arna (känns igen på innehållet), VICE-kommandot |
| `main.py` | fönstret: starta en C64, ROM-filerna, helskärm, joystickport (`main.py --basic` = direkt till BASIC) |
| `tests/` | `WENUYOS_RETRO=…/wenuyos-retro bash tests/run.sh` |

VICE och Open ROMs är Debian-paket (`requires`) som WenuyOS installerar med programmet. Inga Commodore-ROM:ar följer med.
