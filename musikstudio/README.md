# Wenuy Studio – Musikstudio 0.8.0

**Utvecklings-/provversion, komplett fristående källkodsbygge med körbart `.wapp`-testpaket för x86-64.**

Datum: 11 november 2026. Koden är C++17 och GPL-3.0-or-later. Det finns inga låsta samplingar, proprietära tillägg eller nätverkskrav.

## Funktioner

- Arrangemang med upp till 128 ljud- och MIDI-spår, vågklipp och tonblock.
- Pianorulle för att klicka in, välja och radera toner. Tangenterna `A` (ton), `Q` (kvantisera), `Del` och pilar används i redigeraren.
- Mixer med volym, panorering, mute, solo och per-spårs EQ (bas/mellan/diskant) och kompressor (tröskel/ratio).
- Fyra syntljud: sinus, triangel, orgel och varm lead.
- Standard MIDI: import av SMF 0/1 och export; begränsat till noter och tempo, utan controller/ljudbanker.
- WAV-import och WAV-export med streaming och begränsat arbetsminne.
- Mikrofoninspelning via PortAudio till en ny WAV-fil, utan att skriva över befintlig fil. Inspelningen infogas som ett nytt ljudspår.
- Loop av uppspelningsområde, icke-förstörande trimning/split/duplicering av klipp (kärn-API; GUI erbjuder split/duplicering), WAV-export och automatisk återställningskopia vid osparade ändringar.
- Projektformat `.wstudio` **v5**. Läser v2, v3 och v4. Version 0.4–0.6 kan **inte** läsa v5-filer; behåll äldre projektkopior.
- Allt ljud genereras lokalt med C++17. Inget internet krävs.

## Nytt i 0.8.0

- Minnesbegränsad ångra-/gör om-historik med Ctrl+Z, Ctrl+Y och synliga knappar.
- Kvantisering och transponering samt icke-förstörande klippoperationer i redigeringskärnan.
- Svenska och engelska i huvudkontrollerna, med språkknapp. Vissa dynamiska meddelanden är fortfarande på engelska.
- Nya 0.8-regressionstester.

## Gränssnittets status och beroenden

**Viktigt:** Den körbara 0.7-versionens grafiska fönster använder X11. WenuyOS är Waylandbaserat, så **XWayland måste vara installerat och aktiverat** för att starta den här `.wapp`-versionen. Detta är ett **kompatibilitetsbygge**, inte den slutliga GTK4/libadwaita-versionen. Det grafiska fönstret kan startas i Xvfb i Debian, men är ännu inte testat på en faktisk WenuyOS-dator eller med en verklig mikrofon/högtalare. 2 GB RAM är ett mål och ingen uppmätt garanti.

`.wapp` är packad enligt den bifogade WenuyOS-SDK:ns format med `wenuyos-app.toml`, skrivbordsfil, ikon, binär och `SHA256SUMS`. En fungerande installation kräver XWayland och PortAudio samt en WenuyOS-version som accepterar detta paketformat.

## Bygg från källkoden

På Debian 13 eller WenuyOS med utvecklingsverktyg och X11-dev:

```sh
sudo apt-get install cmake g++ make libx11-dev libportaudio2 xwayland
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/wenuy-studio examples/Demo.wstudio
```

Paketera själv med medföljande byggskript `tools/build-wapp.sh`. För ren C++-motor och MIDI utan X11 kan `wenuy-studio-cli` byggas även om X11 saknas.

## Använda appen

1. Öppna Wenuy Studio. Demomelodin finns på skrivbordet i programmet och kan spelas upp.
2. Klicka `Play`. Om du vill importera en WAV: välj `WAV in` och skriv den fullständiga sökvägen.
3. I `Piano roll` kan du skapa och redigera toner. Byt till `Mixer + FX` och klicka på instrumentnamnet för att välja synt.
4. `Record` spelar in från standardmikrofonen och sparar en separat WAV. Klicka igen för att avsluta inspelningen.
5. Spara projektet med `Ctrl+S` och exportera färdig mix via `WAV out`.

**Begränsningar:** Inbyggt filval är ännu en sökvägsruta, inte en GTK-fildialog; ljuddrivrutin kan neka enhetens samplingsfrekvens; effekter uppdateras från en projektsnapshot när uppspelningen startar; plugin-host för LV2 är inte inbyggd och måste säkerhetsgranskas separat.

## Medföljande CLI

```
wenuy-studio-cli demo demo.wstudio
wenuy-studio-cli info demo.wstudio
wenuy-studio-cli render demo.wstudio mix.wav
wenuy-studio-cli midi-import input.mid song.wstudio
wenuy-studio-cli midi-export song.wstudio song.mid
wenuy-studio-cli drums song.wstudio with-drums.wstudio beat.wav 4
```

## Licenser och proveniens

- Ursprungliga Wenuy Studio 0.5/0.6-kärnmoduler `Sequencer`, `MidiFile`, `Arrangement` och `TrackFX` har integrerats från tidigare utvecklingspaket, licens GPL-3.0-or-later.
- Resterande kärna och det fristående X11-gränssnittet för 0.7 har byggts om för att kunna kompileras och testas utan originalarkivet från Dropbox. Detta är **inte** en fil-för-fil-uppgradering av Wenuy Studio 0.4.1:s GTK/libwenuy-gränssnitt och ska därför inte utan vidare ersätta den tidigare utvecklingsgrenen.
- WenuyOS-SDK:s packningsskript medföljer som separat verktyg. Inget libwenuy-bibliotek eller tredjepartskod länkas in i denna binär.

Se `docs/TEST_REPORT_0.8.0.md` för faktisk teststatus och `docs/RELEASE_NOTES_0.8.0.md` för detaljer.

## GitHub-prövningssläpp

Den här 0.8.0-versionen är **osignerad, experimentell XWayland-kompatibilitet** för att möjliggöra hårdvaruprov. Den är inte den slutliga GTK4/Wayland-designen. Användaren behöver godkänna nätverksåtkomst för eventuell beroendeinstallation. Spara äldre projekt innan konvertering; `.wstudio` v5 öppnas inte av tidigare Studio-versioner.

Test vid release: CMake och fyra CTest samt `wenuyos verify`; fysisk mikrofon/högtalare och långvarig 2 GB-last är ännu inte testade.
