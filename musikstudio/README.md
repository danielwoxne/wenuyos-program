# Wenuy Studio 0.4.1 – utvecklingsversion för WenuyOS

**Datum: 10 november 2026.** En fristående musikstudio i C++17 med mål att fungera på 2 GB RAM. **Detta är källkod och ett CLI-testprogram, inte ännu en verifierad installerbar `.wapp`.** GTK4/libadwaita finns inte i den använda byggmiljön, så GUI och fysisk mikrofon/högtalare har ännu inte testats på WenuyOS. Först när grafikbygge och hårdvaruprov är godkända kan vi tala om en verifierad WenuyOS-version.

## Nyheter i version 0.4.1

1. **Buffrad PortAudio-uppspelning:** en producer-arbetstråd läser och blandar WAV/MIDI, medan en separat ljudcallback hämtar redan färdiga ljudbilder från en fast SPSC-ringbuffert. Callbacken gör ingen fil-I/O, ingen allokering och använder inga mutex-lås. Om bufferten underfylls genereras tystnad för de saknade ljudbilderna, och avbrottet räknas.
2. **Tre lägen:** Låg latens (128 ljudbilder/callback; 4096 ljudbilder i ringen), Balanserad (512/8192) och Stabil (1024/16384). Låg latens ger kortare begärd blocktid men större risk för avbrott. Faktisk enhetslatens kan vara högre.
3. **Diagnostik under uppspelning:** GUI visar L/R-toppnivå, buffertnivå i ms och antalet underfyllda callbackanrop. CLI har `play ... --buffer=low|balanced|safe` och visar avbrotten när uppspelningen stoppas.
4. **Robustare testning:** simulerad PortAudio-callback, avsiktligt framkallad buffertbrist, wraparound-/ordningstest och flertrådstest för ringbufferten.

Sedan 0.4.0 finns blockbaserad mixer med mute, solo, pan, huvudvolym, toppmätare och exakt vågformsanalys i bakgrunden. Äldre funktioner som WAV-inspelning/-import/-export, MIDI-tonsynt, ångra/gör om, automatisk återställning, mediekontroller och GTK/libwenuy-GUI-kod finns kvar. `.wstudio`-formatet är fortfarande version 3; version 2 kan läsas men inte skrivas.

**Avgränsning:** En simulerad ljudenhet och C++-kärna är testade. GTK-fönstret är ännu inte kompilerat i denna byggmiljö, och riktig mikrofon/högtalare och 2 GB RAM för hela systemet är inte verifierade. Det är **inte** en färdig installerbar `.wapp`.

## Bygg kärnan och kör tester (Debian 13 x86-64)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWENUY_STUDIO_GUI=OFF
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/wenuy-studio-cli info examples/Startprojekt.wstudio
./build/wenuy-studio-cli render examples/Startprojekt.wstudio /tmp/wenuy-mix.wav
./build/wenuy-studio-cli peaks-accurate examples/demo-fran-0.1.wav
```

**Viktigt:** den simulerade PortAudio-biblioteksfilen i CTest-katalogen är endast en testdubbel. Den ska inte installeras, kopieras med i `.wapp` eller användas för riktiga ljudenheter.

## Bygg grafiken på WenuyOS/Debian med utvecklingspaketen installerade

```sh
sudo apt-get install cmake g++ pkgconf libgtk-4-dev libadwaita-1-dev libportaudio2
cmake -S . -B build-gui -DWENUY_STUDIO_GUI=ON -DWENUY_STUDIO_REQUIRE_GUI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gui -j2
./build-gui/wenuy-studio
```

För integrerad `libwenuy`-tidslinje (det medföljande SDK:t) krävs bland annat `libsqlite3-dev`, `liblua5.4-dev` och `libsodium-dev` samt `-DWENUY_STUDIO_USE_LIBWENUY=ON`. Kontrollera SDK-motsvarigheten mot WenuyOS-versionen du kör. Kör `xvfb-run -a env GDK_BACKEND=x11 ./build-gui/wenuy-studio --smoke-test` för att startprova GUI på en testvärd som har GTK4. Det är **inte samma sak** som fysisk hårdvaruverifiering.

## Val av ljudbuffert (GUI och CLI)

I det grafiska fönstrets övre rad finns **Ljudbuffert**. Välj **Låg latens**, **Balanserad** (standard) eller **Stabil**. Valet träder i kraft nästa gång du trycker Spela. Under uppspelning visas antal ljudavbrott. Om värdet ökar, välj Stabil, minska antalet samtidiga spår eller välj en snabbare disk. Detta mäter inte inspelningslatens.

På kommandoraden:

```sh
./build/wenuy-studio-cli play examples/Startprojekt.wstudio --buffer=balanced
./build/wenuy-studio-cli play examples/Startprojekt.wstudio 0 --buffer=safe
```

Dessa exempel kräver **riktig** `libportaudio.so.2` och en fungerande ljudutgång. Den simulerade testdrivrutinen används bara i automatiska tester.

## Ljudprov via kommandoraden

```sh
./build/wenuy-studio-cli devices
./build/wenuy-studio-cli probe
./build/wenuy-studio-cli play examples/Startprojekt.wstudio
./build/wenuy-studio-cli record /tmp/ny-inspelning.wav 5
```

OBS: PortAudio laddas som `libportaudio.so.2` vid körning och måste vara installerat. Provet `probe` öppnar strömmarna men fångar inte mikrofonljud. Inspelning sparar först till en unik temporär fil och skriver aldrig över en befintlig målinspelning.

## Teststatus

- **10/10 CTest passerade** i Release-läge och med AddressSanitizer/UndefinedBehaviorSanitizer på den här utvecklingsvärden.
- De äldre regressionstesterna omfattar WAV-renderparitet, solo, panorering, huvudvolym, v2/v3-kompatibilitet och transienter; det nya testet omfattar SPSC, flertrådsordning, buffertbrist och simulerad PortAudio-callback.
- CLI-export av exempelprojekt gav giltig 48 kHz stereo-PCM16-WAV. En kort exportanrop uppmättes till ungefär 2,2 MB högsta RSS, **inte ett minnesprov av hela programmet**.
- **Ej verifierat:** GTK-kompilering, grafiskt startprov på WenuyOS, mikrofon/högtalare, subjektiv latens, ljudavbrott, samtidiga många spår, 2 GB RAM för hela installationen.

Se `docs/RELEASE_NOTES_0.4.md`, `docs/ARCHITECTURE.md`, `docs/TEST_REPORT_0.4.md`, `docs/ACCEPTANCE_0.4.md` och `docs/LICENSES.md`. Inga nya externa bibliotek, fonter, samplingar eller plugins har bundlats till denna version. Vår kod: GPL-3.0-or-later; medföljande libwenuy-SDK: Apache-2.0 (se `third_party/libwenuy/LICENSE`).

**Nästa arkitektursteg:** verifiera ljudcallbacken mot riktiga ljudenheter på WenuyOS, stabilisera grafiken, följt av MIDI-editor, klipptrimning, loop och inspelningssynk. Ändringar i `libwenuy` krävs inte för 0.4.1.

## Kända begränsningar 0.4.1

- Callbacken undviker allokering, väntan och disk-I/O, men operativsystemet och ljuddrivrutinen kan fortfarande ge underruns. Vi har inte låst minne eller begärt schemaläggning i realtidsklass; inga garantier om fysisk latens kan lämnas.
- Underliggande mixmotorn kan läsa från fil i arbetstråden. Stora sessioner kan därför fortfarande orsaka underfyllnad; uppspelningen hoppar inte tillbaka och spelar om bortfallna ljudbilder.
- Den begärda samplingsfrekvensen motsvarar projektets. Ljudenheter som inte stöder den kan neka uppspelningen. Automatisk omvandling till annan enhetsfrekvens får komma senare.
- Liveändring av spårens mixerreglage under uppspelning träder fortfarande i kraft först vid nästa Spela.
- `libwenuy`, GTK4 och den verkliga PortAudio-runtime måste finnas på målmaskinen eller byggmiljön; inga testdrivrutiner får paketeras för användare.

Se `docs/ACCEPTANCE_0.4.1.md`, `docs/AUDIO_BUFFERS_0.4.1.md` och `docs/TEST_REPORT_0.4.1.md`.
