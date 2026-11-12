# Wenuy Studio – Musikstudio

**Senaste godkända släpp:** [0.8.1](https://github.com/danielwoxne/wenuyos-program/releases/tag/musikstudio-v0.8.1) (signerad `musikstudio.wapp`, 12 november 2026).
**Nästa version under utveckling:** 0.9.0 i [PR #18](https://github.com/danielwoxne/wenuyos-program/pull/18) – **inte publicerad**. All källkod finns i `musikstudio/` i `danielwoxne/wenuyos-program`. En ny version gäller först när Daniel godkänt och den slagits samman med `main`.

C++17, GTK4/libadwaita (native Wayland), PortAudio, licens GPL-3.0-or-later. Programmet använder inga nätverkstjänster eller nedladdningar. Kräver inte XWayland. Målet är WenuyOS på Debian 13, även på 2 GB utan grafikdrivrutin.

## Det som finns

- Arrangemang med ljudklipp och MIDI-noter på högst 128 spår.
- Pianorulle med klick för toninmatning, och i utvecklingsversion 0.9 även val, borttagning och flytt direkt i rutnätet.
- Fyra inbyggda syntklanger (sinus, triangel, orgel och varm synt).
- Mixer med volym, mute, solo, EQ/kompressor och tillhörande kärnfunktioner för panorering.
- Import/export av standard-MIDI (SMF 0/1; toner och tempo) och PCM-WAV, WAV-mixdown och mikrofoninspelning med PortAudio.
- Projektfiler `.wstudio` version 5. Äldre v2–v4 kan läsas. Gamla versioner kan inte läsa v5 – **spara en kopia innan migrering**. Vid sparande skapas en `.bak`-kopia av befintlig projektfil.

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
- `Nytt`, `Öppna` och stängning skyddar inte ännu osparat arbete med en särskild bekräftelseruta; det är nästa steg i [ärende #17](https://github.com/danielwoxne/wenuyos-program/issues/17). Spara innan byte av projekt.
- För effekter kan ändringar under pågående uppspelning kräva omstart av uppspelningen.
- Inget LV2/VST-hoststöd eller instrumentpaket från tredje part ingår.

Se `docs/STARTFIX_0.8.1.md` och `docs/PLAN_0.9.0.md` för bakgrund och verifieringskrav.
