# Polyfon förhandslyssning – Musikstudio 0.9
Etappen fortsätter i PR #28 ovanpå #27 → #24 → #23 → #21.

- `LivePreview` håller en PortAudio-ström igång under spelpasset i en ljudtråd, inte vid varje tangent. GTK skickar NoteOn/NoteOff via en SPSC-kö med 256 platser.
- `LiveSynth` använder 16 begränsade röster, med oberoende notsläpp, 125 ms release och avslutande/äldre röst som stjäls vid behov.
- Vid överfull kö skickas panik/AllNotesOff för att inte lämna fastnade toner.
- Ordinarie `Playback` för projekt/WAV-export är oförändrad; MIDI-tagningsformatet ändras inte, inget nätverk eller samplingsdata tillkommer.
- Automatisk CTest provar ackord, oberoende släpp, dubblering, 16-rösttak, ogiltiga värden och ändligt PCM.
- Ej provat i denna kodetapp: verklig PortAudio-högtalare, fysisk MIDI, verklig upplevd latens och 30 min på fysisk 2 GB-dator.
