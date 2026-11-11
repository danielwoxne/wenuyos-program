# Arkitektur – Wenuy Studio 0.4.1

```text
GTK4 + libadwaita + valfri WenuyTimeline (Apache-2.0 SDK)
    | Projektvy, tidslinje, mute/solo/pan/master, peak-meter
    | GTask -> waveformAccurate() i bakgrund -> GTK-uppdatering
    v
Project / Track / AudioClip / MidiNote
    |  ProjectIO .wstudio v3 (läser även v2)
    |  History / Recovery / MediaCheck
    v
BlockMixer (max 1024 stereobilder per anrop, inga allokeringar efter init)
    | WAV-ström i små läsblock (samma öppna reader vid delad källa)
    | MIDI sinuston + attack/release + mute/solo/pan/gain/master
    +-> WAV-export (WaveWriter, separat .part, worker)
    +-> Playback producer worker -> SPSC AudioBuffer (4k/8k/16k frames)
                                     -> PortAudio output callback (128/512/1024)
                                        -> Linux ljudstack/ljudkort
Recording worker <- PortAudio blocking ReadStream <- mikrofon
       | unik .part-XXXXXX -> no-replace WAV-fil -> klipp
```

## Säkerhets- och prestandagränser

- `BlockMixer` skapar en förberedd kopia av spårmodellen, med WAV-läsare och en ca 192 KiB gemensam resamplingsbuffert. Inga dynamiska allokeringar görs inne i `process()`. Den **läser ändå från filsystemet** för varje aktivt klipp, och är därför inte realtidssäker i en callback.
- `Playback` blandar och förbereder ljud på en *producer worker*, och PortAudio-callbacken hämtar ljudet från `AudioBuffer`. Callbacken gör ingen disk-I/O, minnesallokering, låsning eller loggning. Bufferkontroll använder acquire/release på atomiska 64-bitarsindex. Enheten kan ändå underrunna, och positionsvärdet är skickade ljudbilder – inte uppmätt DAC-uppspelningstid.
- Varje WAV-klipp använder streaming och upp till 4096 topp-/bottenvärden kan returneras av waveform-API:t. GTK begär 2048 toppar per unik källa, beräknat i GTask; detta är en exakt filskanning och kan ta tid på stora filer. Grafiktråden väntar inte på skanningen.
- Master och mixreglar kopieras från projektet vid uppspelningsstart och ändras inte medan samma snapshot spelas. En senare version kan införa en enkel kontrollkö till ljudmotorn.
- GPU, PipeWire och AVX krävs inte för C++-kärnan. GTK4/libadwaita och PortAudio behövs för körande fönster och ljud. Det är ännu inte bevisat att WenuyOS med 2 GB RAM har tillräcklig marginal med GUI.
- Ringbufferten är begränsad till högst 16384 stereobilder (131072 byte i standardinställning Stabil), plus 1024 bilder i producer-scratch. Ingen obegränsad kö byggs upp.
- Långa projekt, många separata WAV-filer, tung CPU eller långsam disk kan fortfarande skapa ljudavbrott. Underfyllda PortAudio-callbackanrop räknas, och återstående del nollfylls utan att försöka upprepa bortfallet. Enhetsfel stoppas med feltext.
- `Recording` använder fortsatt blockerande PortAudio-läsning med skyddad temporär fil. Välj av inspelningsenhet, inspelningslatenskompensation och stabil realtidsprofil återstår.

## WenuyOS / libwenuy

Inga ändringar i `libwenuy` krävs för denna version. Ett framtida valfritt gemensamt API bör kunna rapportera tillgängliga enheter, behörigheter, förväntad latens, förvalda sample rates och över-/underrun-statistik; DSP och pluginhost förblir lokala för Studio.

## Versionskontrakt

- Wenuy Studio 0.4 kan **läsa projektversion 2 och 3**, men sparar 3.
- Wenuy Studio 0.3.x kan **bara läsa version 2**, och kan därför inte öppna en fil som sparats i 0.4 utan konvertering.
- Sparning gör fortfarande `.bak`-kopia av föregående projektfil. Ljudklippen förblir externa filer och kräver egen säkerhetskopiering.
