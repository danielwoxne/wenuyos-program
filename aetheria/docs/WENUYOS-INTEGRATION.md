# Aetheria använder WenuyOS gemensamma grafik och Datatyper

**Status:** en första *utvecklingsgren*, PR #15 ovanpå UI-PR #13. Spelkällan på GitHub är fortfarande en 0.5.0-referens; **spelversion 0.6.0** (kamerazoom, uppdragsnavigering, automatisk sparning, musstyrning) måste återtas och jämföras **före** sammanslagning. Det är inte ett färdigt eller distribuerat Aetheria.

## 1. WenuyGFX – faktiskt integrerat, men valbart

- CMake söker `WenuyGFX 0.9 CONFIG` i **WenuyOS:s installerade SDK** och länkar `WenuyGFX::wenuygfx`. Inga motorfiler vendoreras, och inget ändras i WenuyOS.
- `Scene3D::collectWenuyGfx` använder **samma** funktioner `terrain()` och `objects()` som den befintliga SDL-renderaren: hjältens position, levande fiender, NPC:er, kristaller, örter och miljö. Ingen spelmekanik eller filskrivning i renderaren.
- F3 eller `--wenuygfx` väljer systemmotorns **SoftwareRenderer** (djupbuffert och grundbelysning) i stället för SDL:s sorterade trianglar. Även datorer med GLES kör **CPU i denna etapp**; full GLES3-fönsterintegration kräver separat SDL3/EGL-arbete.
- Ursprunglig SDL2-renderare är fortsatt **standard** och används automatiskt om den valbara WenuyGFX-vägen misslyckas. 2D, dialoger, väska och sparning finns kvar.
- Lågprofil ger 480×300 pixlar för WenuyGFX-bilden (uppskalad i SDL-fönstret); högre detalj 640×400. Scenexporten begränsas till 26 000 trianglar. Inga skuggkartor/texturer och inga nätverksanrop.
- VIKTIGT: även om WenuyGFX ritar på CPU så används här **SDL2:s befintliga fönster och texturer**. Det är **inte ännu** WenuyGFX:s wl_shm-presentering utan EGL, eftersom HUD och SDL-inmatning inte är migrerade. Utgå inte från att det sparar Mesa/LLVM-minne på Wayland före separat mätning.

## 2. Datatyper – anslutet till skärmbilder

- CMake söker `pkg-config wenuy-datatypes` och länkar bara systemets `libwenuy-datatypes` om det är installerat.
- Exempel: `aetheria --screenshot /tmp/aetheria.png --wenuygfx` eller `.../aetheria.iff` (WenuyOS:s installerade format avgör vad som kan sparas). De RGB-värden spelet ger omvandlas med ett storleksbegränsat ARGB→RGBA-hjälp-API till `GdkPixbuf`, varefter `wenuy_datatype_save_pixbuf` väljer rätt systemtillägg.
- Det tidigare `--screenshot /tmp/aetheria.ppm` är **oförändrat**. Om Datatyper inte finns fungerar PPM fortfarande; andra ändelser ger begripligt fel, ingen dold nätverkshämtning.
- Nästa datatypssteg är **inläsning** av egna porträtt/bilder i UI samt ljudformat via `wenuy_datatype_load_pixbuf/audio`, när vi har verkliga resurser att använda; inga stora data allokeras vid normal spelstart.

## 3. Portar och övrig spelintegration

- `WenuyPort` använder GApplication/D-Bus, medan Aetheria fortfarande kör SDL2:s huvudloop. Inga låtsasportar eller osäkra IPC-kommandon har införts.
- På sikt: spelbiblioteket via spelets `.desktop`, `.wapp` med systembibliotekens versionskrav, indata/handkontroll via libwenuy-game när den verkliga 0.6.0-koden kommit in.
- Inga ändringar av sparformat eller spelarens framsteg i denna etapp; kör aldrig programinstallatörer eller nätverkskod från spelet.

## 4. Prova och verifiera

Följande ska köras i en **WenuyOS/Debian 13-miljö med SDL2-dev och installerad WenuyGFX 0.9 samt Datatyper**:

```sh
cmake -S aetheria -B /tmp/aetheria-gfx-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/aetheria-gfx-build -j2
ctest --test-dir /tmp/aetheria-gfx-build --output-on-failure
WENUYOS_PERF=low WENUYOS_GFX=soft /tmp/aetheria-gfx-build/aetheria --wenuygfx --frames 120 --no-audio
WENUYOS_PERF=low WENUYOS_GFX=soft /tmp/aetheria-gfx-build/aetheria --screenshot /tmp/aetheria-gfx.png --wenuygfx --no-audio
```

**Genomfört:**
- [x] Kod för delad geometri, valbar WenuyGFX-CPU-väg och SDL-reserv incheckad i **PR #15**. Detta är kodgranskning, inte ett framgångsrikt fönsterprov.
- [x] `tests/test_wenuygfx.cpp` registrerat som CTest i byggsystemet (kräver det installerade SDK:t).
- [x] `tests/test_datatype_pixels.cpp`: samma isolerade ARGB→RGBA-konvertering har kompilierats och körts med GCC C++17, `-Wall -Wextra -Wpedantic -Werror`, AddressSanitizer och UBSan **utan fel**. Detta provar inte den dynamiska Datatyper-kodvägen.
- [ ] Fullt `wenuyos verify`, alla spelmodelltest, faktiskt SDL2-fönster och PNG/IFF-formatskrivning är **inte provade**.
- [ ] Program CI på GitHub: [#43327309729](https://github.com/danielwoxne/wenuyos-program/actions/runs/43327309729) stoppades **före bygge** av att `libsdl2-2.0-0`/`libsdl2-dev` saknas i CI-containern. Separat blockerare [#16](https://github.com/danielwoxne/wenuyos-program/issues/16).
- [ ] Skärmbilder före/efter, FPS/p95/p99, RSS och kontroll på fysisk 2 GB-dator med `WENUYOS_GFX=soft`.
- [ ] Jämför och återför huvudversion 0.6.0 samt den ursprungliga ikonen. **Ingen merge och inget släpp före detta samt Daniels godkännande**.

Denna etapp ändrar inte WenuyGFX:s eller WenuyOS:s källkod; spelet använder deras installerade API:er.
