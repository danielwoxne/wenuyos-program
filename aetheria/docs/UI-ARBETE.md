# Aetheria – användargränssnitt, säker överföring

**Syfte:** En tydligare, resurssnål HUD och läsbarare dialoger/menyer i spelet. Ändringarna avser enbart Aetheria, inte WenuyGFX eller WenuyOS.

**Källstatus (2026-11-12):** Senast direkt tillgängliga verkliga källpaket är **aetheria-0.5.0-src.zip** i Dropbox (NYA WENUYOS/Programvara/Aetheria). Aetheria **0.6.0 har redan skapats i en tidigare chatt**, med navigering, musklick, kamerazoom och autosparning, men dess källfil är inte tillgänglig för den här GitHub-grenen. Den tidigare 0.6.0 måste jämföras och återinföras innan denna gren får godkännas/släppas. Inget användardata/sparformat ändras.

## Den här PR:ens avgränsning
- Ta in 0.5.0-källan som **referensbas**, inte som uppdatering som ska ersätta 0.6.0.
- Förenkla informationshierarkin: karaktärens status, aktuellt uppdrag och minimap överst, färre textrader över spelvärlden.
- Visa tydliga knappar med tangentbordsnamn längst ned; ge paus, karta och väska läsbara stängningshänvisningar.
- Förbättra kontrast och justera placering för 960×600-logisk yta/skalning.
- Lägg till isolerade regressionsprov av layout/knappområden. Befintliga tre modellsviter fortsätter att köras.
- Båda språk, offline, ingen ny tjänst, inget nytt SDL/Wayland-beroende, soft/low som standard.

## Leveranskontroll
1. Bygge/test på Debian 13 i CI enligt Program-kontrollen.
2. Skärmbild och manuell visuell kontroll i low/soft, inventarium och dialog.
3. Källjämförelse/återbasering på verklig 0.6.0 innan merge.
4. Granskning av Claude, godkännande av Daniel. Ingen release, tagg eller merge i detta steg.

**Felsäkerhet:** Om äldre källkod/utdragsformat inte kan byggas korrekt, håll PR som utkast och använd ändringarna som patchförslag, inte som ny version av spelet.

## Genomfört i utkast-PR #13
- Aetheria-källan för 0.5.0 har lagts in **enbart i utvecklingsgrenen**. GitHub `main` är oförändrad. Det är en referensimport, inte en version som ersätter 0.6.0.
- `src/main.cpp`: kompakt HUD med hälsa/ork/nivå, huvud- och sidouppdrag, minimap, tydligare actionbar, tillfälliga meddelanden och direktklick för väska/karta/hjälp/strider.
- Dialog- och inventariepanelerna har nu tydliga klickbara val och Stäng, pausdialogen Fortsätt. Bakgrundsklick i modala vyer triggar inte attacker.
- `src/UiLayout.h`: en enda uppsättning hitbox-koordinater för 960×600, med ordningsregel för modala vyer och explicit avgränsning.
- `tests/test_ui_layout.cpp`: 21 rektanglar och kontroll av alla handlingar, modala klick och koordinater utanför skärmen. CTest-target i CMake.
- En enkel temporär SVG-ikon har använts eftersom Dropbox-textutdraget av 0.5.0-källzippen inte innehöll SVG-markup. Den ursprungliga ikonen ska återföras före granskning/release.

## Faktisk provning och begränsningar
- [x] **Lokal headless C++17-provkörning** av `UiLayout.h` och motsvarande hitbox/regressionsfall: `g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -UNDEBUG`; alla kontroller passerar. Det kördes utanför WenuyOS och utan SDL.
- [ ] **GitHub Program CI:** körning [#43323843517](https://github.com/danielwoxne/wenuyos-program/actions/runs/43323843517) misslyckades **före C++-bygget** eftersom Debian 13-containern för programförrådet inte installerar SDL2-körbiblioteket (CMake: `SDL2 runtime missing`). Därför är programkompilering, modellsviter, wenuyos verify, GUI-start och paketering **inte provade** i denna PR.
- [ ] Full spelprovsrunda i Debian 13 med installerad `libsdl2-dev` och faktiskt SDL2-fönster.
- [ ] Skärmbildstest för svenska/engelska, 960×600, 1024×600 och lägre upplösningar.
- [ ] Fysisk 2 GB WenuyOS utan GPU-drivrutin, input/fps/RSS/30 minuter.
- [x] Återskapat funktionerna i ny C++17-kod på GitHub: mål/riktning/avstånd/kartmarkör, högerdrag/mushjul, ~90 s autosparning och klickbara UI-kontroller.
- [ ] **Före merge:** verifiera samma beteende mot den tidigare 0.6.0-specifikationen i riktig spelkörning och försäkra att ingen befintlig funktion gått förlorad. Den försvunna zip-filen behöver inte vara en blockerare när beteendet är provat, men får inte påstås vara identisk källa.

**Avsiktlig avgränsning:** inga ändringar i WenuyGFX, inget nätverk i spelet, ingen ny persistent spelinställning eller ändring av sparfilsformat. CI-blockeringen i det gemensamma arbetsflödet åtgärdas av den som ansvarar för Program-CI, inte genom att Aetheria laddar paket eller nätverksresurser vid körning.

### Återuppbyggd 0.6-etapp (13 nov 2026)

- `src/GameplayAssist.h`: `nearestQuestTarget` väljer faktisk målposition utan scenallokering; `directionToTarget` är kamera-relativ. `CameraControl` begränsar zoom/högerdrag. `AutoSaveClock` räknar **bara aktiv** speltid.
- `src/main.cpp`: målpanel och målmarkering i liten/stor karta, höger musknapp + mushjul, svenska/engelska ledtexter, autosparning via befintlig `World::save` och separat manuellt F5/F9.
- `src/Scene3D.cpp`: kameraavstånd kan ändras utan att 2D-renderaren påverkas. `src/MiniSDL.h` deklarerar SDL2:s mus- och hjulevent i fallbackläget.
- `tests/test_stage6.cpp`: regressionsfall för uppdragsprecision, vinkel, zoomgränser, 90 aktiva sekunder, paus, felsparningsretry och kompatibel `World::save/load`.
- **Utfört utanför GitHub:** en isolerad C++17/ASan/UBSan-smokekörning med en förenklad testmodell för navigerings-/zoom-/autosparningsalgoritmerna passerade. Detta **är inte** ett bygge av den riktiga Aetheria-modellen.
- **Ej verifierat:** ny Stage6-testsvit mot verklig `Model.cpp`, komplett `wenuyos verify` och 30 minuter på WenuyOS, eftersom GitHubs Program-CI saknar SDL2 (#16).
- **Före merge:** granska och återbasera det stackade WenuyGFX-/Datatyper-PR:et #15 så att 0.6-funktionerna inte försvinner där.
