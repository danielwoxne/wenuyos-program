# Wenuy Studio – Musikstudio

**Senaste godkända släpp:** [0.8.1](https://github.com/danielwoxne/wenuyos-program/releases/tag/musikstudio-v0.8.1) (signerad `musikstudio.wapp`, 12 november 2026).
**Nästa version under utveckling:** 0.9.0. Pianorullen från [PR #18](https://github.com/danielwoxne/wenuyos-program/pull/18) är sammanslagen med `main`; skydd för osparat arbete från [PR #19](https://github.com/danielwoxne/wenuyos-program/pull/19) är också sammanslaget. Instrumentbiblioteket utvecklas i [PR #21](https://github.com/danielwoxne/wenuyos-program/pull/21). **0.9 har ännu inte publicerats som `.wapp`.** All källkod finns i `musikstudio/` i `danielwoxne/wenuyos-program`.

C++17, GTK4/libadwaita (native Wayland), PortAudio, licens GPL-3.0-or-later. Programmet använder inga nätverkstjänster eller nedladdningar. Kräver inte XWayland. Målet är WenuyOS på Debian 13, även på 2 GB utan grafikdrivrutin.

## Det som finns

- Arrangemang med ljudklipp och MIDI-noter på högst 128 spår.
- Pianorulle med klick för toninmatning, och i utvecklingsversion 0.9 även val, borttagning och flytt direkt i rutnätet.
- **16 inbyggda syntetiska instrument** utan ljudfiler: sinus, triangel, orgel, varm lead, piano (syntes), elpiano, speldosa, plockad gitarr, elbas, stråkensemble, cello, flöjt, klarinett, bleckblås, körmatta och marimba. Instrumenten är C++-klanger – **inte inspelningar av verkliga instrument**. De fyra gamla ID-numren 0–3 är bevarade.
- Mixer med volym, mute, solo, EQ/kompressor och tillhörande kärnfunktioner för panorering.
- Import/export av standard-MIDI (SMF 0/1; toner och tempo) och PCM-WAV, WAV-mixdown och mikrofoninspelning med PortAudio.
- Projektfiler `.wstudio` version 5. Äldre v2–v4 kan läsas. Gamla versioner kan inte läsa v5 – **spara en kopia innan migrering**. Vid sparande skapas en `.bak`-kopia av befintlig projektfil.

## Instrumentbibliotek (utvecklingsversion 0.9)

Välj ett spår i Arrangemang och öppna fliken **Instrumentbibliotek**. Välj ett ljud under Tangentinstrument, Syntar, Stränginstrument, Blåsinstrument, Slagverk eller Röster. Du kan också välja direkt i varje spårs instrumentmeny. Valet sparas per spår i projektet och används både i uppspelning och WAV-rendering.

**Kompatibilitet:** `.wstudio` behåller formatversion 5 och de ursprungliga ID-numren 0–3. Äldre Musikstudio kan inte öppna projekt som använder något nytt instrument (ID 4–15), så spara en kopia innan du använder de nya klangerna. Instrumentens namn är översatta till svenska och engelska.

Detta är ett resurssnålt, helt lokalt syntesbibliotek utan SF2/SFZ, samplingar eller ljudbanker att ladda ner. En framtida separat etapp i [ärende #20](https://github.com/danielwoxne/wenuyos-program/issues/20) hanterar mer realistiska instrumentpaket samt stöd för systemets Datatyper.

## Importera ljud via WenuyOS Datatyper (utveckling)

I **Arkiv → Importera ljud** kan Musikstudion läsa vanlig WAV direkt. Med WenuyOS systembibliotek **Datatyper** installerat kan den också använda de ljudformat som systemet har aktiverat, exempelvis FLAC, Ogg, MP3, AIFF och Amiga 8SVX. Formaten beror på vilka Datatyper-tillägg som finns; Musikstudion hämtar inga codecs från nätet.

De format som inte är en direkt spelbar WAV avkodas till en permanent stereo-WAV i `~/.local/share/se.wenuyos.app.musikstudio/imports/`. Detta gör att ett sparat projekt kan öppnas igen efter en omstart. Flyttas projektet till en annan dator behöver även de importerade ljudfilerna kopieras. Systemets 2 GB-ljudgräns gäller, och importen avvisar mer än 64 MiB avkodade 16-bits ljuddata. Ingen konvertering görs i bakgrunden när man inte importerar.

I byggen som saknar Datatyper är vanlig WAV-import fortfarande möjlig. Se [Datatyper-etappen](https://github.com/danielwoxne/wenuyos-program/pull/23) och `docs/DATATYPER_0.9.md`.

## Valbar 3D-notkarta med WenuyGFX 0.9

När WenuyGFX 0.9-SDK finns vid bygget följer det separata verktyget `wenuy-studio-gfx` med. När verktyget finns i programmappen visas även **Arkiv → Exportera 3D-notkarta…**. Spara projektet först, välj sedan en **ny** fil med ändelsen `.ppm`. Verktyget genererar en statisk 3D-karta av MIDI-noternas placering, längd, tonhöjd och anslagsstyrka; det exporterar **inte** ljud eller en frekvensanalys.

Kommandoradsvarianten (när byggd): `wenuy-studio-gfx LÅT.wstudio NOTER.ppm --low`. En 2 GB-dator använder 320×180-bilden och WenuyGFX:s CPU-renderare utan grafikdrivrutin. Andra profiler använder 640×360. Högst 256 noter från de första 16 spåren ritas för att hålla minne/CPU nere.

3D-funktionen är **frivillig och isolerad i en egen process**. Vanlig GTK4-redigering, inspelning och uppspelning kräver varken WenuyGFX eller GPU. Om hjälpprogrammet inte byggts visas inte menyvalet. Se [WenuyGFX-PR #24](https://github.com/danielwoxne/wenuyos-program/pull/24) och `docs/WENUYGFX_0.9.md`.

## Virtuellt keyboard och live-MIDI (utkast PR #27)

I fliken **Pianorulle** finns två oktaver (C4–B5). Klicka och håll en tangent, eller fokusera keyboardet med mus/Tab och spela på datorns tangentbord:

- **Undre oktaven C4–B4:** Z/X/C/V/B/N/M (vita tangenter) och S/D/G/H/J (svarta).
- **Övre oktaven C5–B5:** Q/W/E/R/T/Y/U (vita) och 2/3/5/6/7 (svarta).
- En tangent ger en kort, **monofon förhandslyssning** med instrumentet på valt spår. Den använder befintlig PortAudio och påverkar inte uppspelningsmarkören. Saknas ljudenhet kan MIDI ändå spelas in.
- Klicka **Spela in MIDI**, spela tangenterna och välj **Stoppa MIDI-inspelning** eller transportens **Stoppa**. Noternas längder mäts från anslag till släpp med monoton klocka; flera samtidiga toner registreras och resultatet sätts in med 1/16-delskvantisering av starttiden.
- Inspelningen läggs på det spår som var valt vid start. **Ångra** kan återställa hela tagningen. Programmet hindrar projektbyte/stängning och sparning medan MIDI-inspelningen pågår; stoppa tagningen först.

**Begränsningar:** Högst 4 096 registrerade noter per tagning och 50 000 per spår i befintliga modellen. Ljudförhandsvisningen öppnar än så länge ett kortlivat PortAudio-flöde per ton och behöver speltestas för låg latens. Extern USB-/ALSA-MIDI har ett separat utkast i PR #28; licensgranskade inspelade instrument ingår **inte**. Se `docs/KEYBOARD_MIDI_0.9.md`, `docs/FRIA_SAMPLINGAR.md` och [ärende #26](https://github.com/danielwoxne/wenuyos-program/issues/26). Inget av detta finns ännu i den publicerade versionen 0.8.1.

## Extern MIDI-inmatning (utkast PR #28)

Musikstudion kan använda ett **externt USB-MIDI-keyboard** via Linux **ALSA Sequencer**, utan att starta en bakgrundstjänst eller ansluta automatiskt. Under **Pianorulle**, nedanför det virtuella keyboardet:

1. Anslut ett MIDI-keyboard till datorn och välj **Sök MIDI-enheter**. En lista över lokala ALSA-källor visas.
2. Välj rätt källa och klicka på **Anslut MIDI**. Anslutningen görs endast efter detta val. **Koppla från MIDI** avslutar prenumerationen och släpper toner som fortfarande hålls nere.
3. Spela. Toner från C4–B5 visas också på det virtuella keyboardet; övriga MIDI-toner (0–127) registreras utan att grafik utanför keyboardet krävs.
4. Starta **Spela in MIDI** innan du spelar om du vill behålla tagningen. Anslagets velocity (1–127) och verkliga notlängd följer med. Tryck **Stoppa MIDI-inspelning** och spara projektet.

**Viktigt:** Linux ALSA Sequencer (utvecklingspaketet `libasound2-dev` för bygge, körbiblioteket `libasound2t64` på Debian 13) krävs för den externa funktionen. Om MIDI-stödet inte kan byggas visas ett förklarande meddelande, medan det virtuella keyboardet och ljudredigeringen fungerar utan ALSA. MIDI-händelser behandlas händelsestyrt i GTK:s huvudloop utan nätverk. Källorna listas bara efter användarens klick.

Stödet omfattar MIDI Note On/Off, velocity och Controller 120/123 (*All Sound Off / All Notes Off*). Sustainpedal, pitchbend, flera röster samtidigt i förhandslyssningen och kanalbaserad separation är ännu inte implementerat. Inspelning kan behålla flera samtidiga toner, men förhandslyssningen är fortfarande monofon. Ingen fysisk USB-MIDI-enhet har provats i denna utvecklingsmiljö; se `docs/EXTERN_MIDI_0.9.md`.

## Pianorullen (utvecklingsversion 0.9.0)

1. Välj ett spår genom att klicka på dess rad i **Arrangemang**. Gå till fliken **Pianorulle**.
2. **Vänsterklick på en tom ruta:** lägg till en ton på den tonhöjden och tiden. Den anpassas till 1/16-noters rutnät utifrån projektets BPM. Tonhöjderna C2–C6 visas.
3. **Vänsterklick på en befintlig ton:** markera den. **Högerklick på tonen** eller **Delete/Backspace** på markerad ton tar bort den.
4. **Piltangenterna:** flyttar den markerade tonen ett halvt tonsteg upp/ner eller ett rutsteg vänster/höger. Klick i tidslinjens övre linjal flyttar endast uppspelningsmarkören.
5. **Ctrl+Z / Ctrl+Shift+Z:** ångra/gör om. `+ C4-ton` och **Kvantisera** finns kvar.

Det finns ännu ingen drag-redigering av tonlängd eller noter över/under synligt tonomfång. Pianorullen är under utveckling.

## Bygg och testa i WenuyOS / Debian 13

För utveckling: installera WenuyOS SDK (`wenuyos install dev`) eller använd Debian 13 med CMake, g++, pkg-config, GTK4/libadwaita-utvecklingsbibliotek och PortAudio-körbibliotek. Från programmappen:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
WENUYOS_PERF=low WENUYOS_GFX=soft wenuyos verify
```

`wenuyos verify` är avsett att köras på WenuyOS och provar även fönsterstart. Byggda filer och `.wapp` läggs aldrig i Git. Fysisk uppspelning och mikrofon måste kontrolleras på riktig dator innan en ny release.

**Kommandoradsverktyg (`wenuy-studio-cli`):**

```text
wenuy-studio-cli demo demo.wstudio
wenuy-studio-cli info demo.wstudio
wenuy-studio-cli render demo.wstudio mix.wav
wenuy-studio-cli midi-import input.mid song.wstudio
wenuy-studio-cli midi-export song.wstudio song.mid
wenuy-studio-cli drums song.wstudio with-drums.wstudio beat.wav 4
```

## Nuvarande begränsningar

- Fysiskt ljud in/ut och långtidsprestanda vid 2 GB med många spår är inte fullt provade.
- Vissa feltexter från den äldre ljud- och projektkärnan kommer ännu bara på engelska.
- I utvecklings-PR #19 frågar Musikstudion vid osparad musik när du väljer **Nytt**, **Öppna** (även via Filer) eller **Stäng**: **Avbryt**, **Kasta ändringar** eller **Spara**. Valet **Spara** fortsätter först efter lyckad lagring. Om den namnlösa låten kräver **Spara som** går det att avbryta utan att stänga eller byta projekt. Skyddet ingår ännu **inte** i publicerade 0.8.1.
- För effekter kan ändringar under pågående uppspelning kräva omstart av uppspelningen.
- Inget LV2/VST-hoststöd eller instrumentpaket från tredje part ingår.

Se `docs/STARTFIX_0.8.1.md`, `docs/PLAN_0.9.0.md` och `docs/PLAN_PROJEKTSKYDD.md` för bakgrund och verifieringskrav.
