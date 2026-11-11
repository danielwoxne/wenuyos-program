# Wenuy Studio – utvecklingsplan från 0.4.1

**0.4.0 implementerat/testat i kärnan**: BlockMixer, PortAudio-simulering av direktuppspelning, mixerlogik (solo/pan/master), 2.0-projektläsning och 3.0-projektsparning, GUI-kod för reglage/meter och asynkrona vågformer. 9/9 automatiska tester inklusive sanitering.

**Omedelbar kvalitetsspärr:** Bygg GTK och valfri libwenuy mot aktuell WenuyOS; prova riktiga ljudenheter. 2 GB RAM-målet återstår att mäta.

**0.4.1 implementerat/testat i simulerad drivrutin:** SPSC-ringbuffert mellan producer-workern och PortAudio-callback, inga lås/allokeringar/filoperationer i callbacken, underfyllnadsmätare, stopp och förfyllning, GUI-val för låg/balanserad/stabil, CLI-status. **Återstår:** verklig enhetslatens, felhändelser från flera backends, sample-rate-fallback och seek under spelning.

**0.5:** Pianorulle och MIDI-editorn, metronom, snap till takt, klipptrimning, loop, markerade områden, val av inspelningsenhet och inspelningslatenskompensation.

**0.6:** Mixerns bussar och skick/retur, enkla EQ/kompressor/reverb, automationskurvor och utförligt prestandatest för 2 GB RAM.

**0.7–1.0:** LV2-host, säker pluginhantering och sandbox-policy, tillgänglighet, projektutbyte, återställning över uppdateringar, WenuyOS-native paketering och komplett dokumentation. Licensprövning görs för varje tillagd plugin/sampling.
