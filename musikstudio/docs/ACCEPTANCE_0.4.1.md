# Manuell provplan för WenuyOS 0.4.1

## 1. Bygg på WenuyOS

Installera/aktivera `cmake`, `g++`, `pkg-config`, `libgtk-4-dev`, `libadwaita-1-dev`, `libportaudio2`. Bygg med `-DWENUY_STUDIO_REQUIRE_GUI=ON`; kontrollera att `wenuy-studio` faktiskt skapas. Kör även med `-DWENUY_STUDIO_USE_LIBWENUY=ON` om de nödvändiga utvecklingsbiblioteken finns och SDK matchar operativsystemet. Inget GUI-bygge har verifierats i den här utvecklingsmiljön.

## 2. Grafisk start och ljudenheter

Öppna musikstudion, skapa testprojekt, se att Ljudbuffert-menyn har tre val, tryck Ljudenheter och Testa ljudenheter. Använd riktiga hörlurar/högtalare och mikrofon. Se till att inget gammalt viktigt ljud skrivs över.

## 3. Spela / stoppa / diagnostisera

Lägg till en WAV och en MIDI-ton. Prova Balanserad, Låg latens, Stabil. Hör efter knaster, korta luckor eller tappade noter. Läs `Ljudavbrott` och `Buffert [ms]` i status. Låt programmet spela klart, och prova också att avbryta när många spår är aktiva. Anteckna antal avbrott, CPU-last och faktisk enhetsfrekvens.

## 4. Öppna gamla projekt / inspelning / export

Öppna en *kopia* av äldre v2/v3-projekt. Spela upp, spela in en ny WAV, exportera mix och jämför. Kontrollera återställning och att originalinspelningar inte skrivs över.

## 5. Test på 2 GB RAM

Under verklig WenuyOS-session: kontrollera hela systemets minnesanvändning och om OOM-/swap-problem uppstår. Prova 2/8/16 ljudspår med lokal WAV-lagring under minst 10 min, i alla tre profiler. Logga median/p99 callback-längd om det går och faktiska underruns. **En lyckad kort CLI-export räcker inte som minnes- eller latensbevis.**

## Stoppkriterier

Ingen `.wapp` annonseras som verifierad förrän GUI-bygge, fysisk ljudutgång, inspelning, korrekt stopp, rimlig 2 GB-användning och felrapportering passerat på målmaskin. Spara testprotokoll som underlag för 0.4.2/0.5.
