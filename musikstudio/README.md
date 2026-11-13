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
