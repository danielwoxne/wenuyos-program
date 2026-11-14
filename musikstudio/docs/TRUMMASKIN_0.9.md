# Musikstudions trummaskin – etapp 0.9 (arbetsutkast)

Arbetet fortsätter ovanpå mixer-PR #30 och dess beroenden; ännu ingen publicerad funktion.

Mål: tre slagverk (bastrumma, virvel, hi-hat), 16-stegsrutnät, swing, 1–8 takter och tillägg av MIDI-toner i ett eget trumspår, så att Ångra och .wstudio v5 fortsätter fungera. Syntetiserat ljud utan externa samplerfiler och utan nätverk.

Provkrav: 2 GB/soft i Debian 13, minnes- och eventgränser, exporterbar/uppspelbar stereomix och tillgänglig GTK4-vy på svenska och engelska. Fysisk högtalare/MIDI återstår.

## Implementering
- Ny flik **Trummaskin / Drum machine** i GTK4 med tre rader (bastrumma 36, virvel 38, hi-hat 42), 16 tillgängliga växlingsknappar och horisontell rullning på små skärmar.
- 1–8 takter och swing 0–45 %; rytmen läggs till med **Lägg till trumspår** vid uppspelningsmarkören.
- Mönstret är en tillfällig inmatningsmall; efter tillägg är varje slag en vanlig MIDI-ton i ett separat spår med syntinstrument **Trumset (ID 16)**. Ångra, MIDI-redigering, uppspelning och projektfilerna förblir samma format.
- Ljudet består av syntetisk fallande bastrumma, kort virvelbrus och hi-hat; inga samplebanker eller nätverkskällor.
- Note/spår-tak, tempo, taktlängd, swing och tomt mönster kontrolleras före projektändring. Ingen skrivning till disk sker när rytmen ritas.
- Nya prov för swing, de tre ljuden, projektfil/Ångra, nekad felinmatning och CPU-mix. Full fysisk hörbarhet/latens är ännu inte provad.

## Direktförhandslyssning (steg 2)
- En ny Play pattern / Spela rytm-knapp ska loopa 16-stegsmönstret i projektets BPM, swing och antal takter, utan att ändra projektet.
- Trumsteg ska höras när de aktiveras även utan ett tillagt spår, via befintliga 16-rösters LivePreview och samma PortAudio-ström.
- Ny `DrumPreviewClock` planerar beat via monotona absoluta tider i stället för ackumulerade väntetider och hoppar över gamla steg om GTK-kön fördröjs. Inga nya trådar eller nättjänster.
- CTest provar timing, swing, två takters loop, liveändringar av steg, gränsvärden samt utebliven ljudkaskad efter fördröjt UI.
- Begränsningar: GLib-timer är inte sample-exakt; hörbar latens, jitter, 1024×600, Orca och 30 minuter på en fysisk 2 GB-dator behöver fortfarande provas.
