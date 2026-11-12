# Aetheria 0.3 — teknisk arkitektur

## Källstruktur

- `src/Environment.h` — delad deterministisk trädsampling utan tillgångar;
  samma koordinater används för kollisioner och 3D-vegetation.
- `src/Model.h/.cpp` — deterministisk värld 80 × 80 celler, uppdrag,
  rörelse, AI-grund, elementkrafter, fysiska hopp/glidning, sparning.
- `src/Scene3D.h/.cpp` — ortonormal tredjepersonskamera, kontinuerliga
  procedurgenererade höjder, 3D-mesh, low-poly-objekt och painter-sortering.
- `src/main.cpp` — fönster, input, 2D/3D-växling, HUD, framerate,
  sparmigrering och skärmbild för värdprov.
- `src/Font.cpp` — liten bitmap-ritning av text utan extern fonttillgång.
- `src/MiniSDL.h` — deklarationsbaserad SDL2-kompatibilitet om utvecklingsheaders
  saknas; föredra alltid riktiga headers för andra arkitekturer.
- `tests/test_model.cpp` — headless tester av uppdrags-, fysik- och sparlogik.

## Renderer: avvägningar för 2 GB RAM

1. Terrängens höjd bestäms av matematisk funktion, inte av stora bilder.
2. 3D-punkter projiceras med perspektiv genom en virtuell kamera.
3. Trianglar och lågpolygrafik sorteras bakifrån och fram (painter's algorithm).
4. Hela scenen lämnas i ett enda `SDL_RenderGeometry`-anrop per bild.
5. SDL2 väljer maskinvarurendering där det fungerar; annars mjukvara.
6. En begränsad radie för landskapet (17/25 celler) håller draw-mängden låg.
7. FPS begränsas till 30. 2D-läget ligger kvar för extra resurssvaga system.
8. En liten dagsljusfaktor påverkar belysning per polygon och himmelsgradient;
   inga dynamiska skuggkartor eller GPU-shaders används.
9. Figurer består av procedurbyggda prismor och kristallformer. Ben och armar
   flyttas individuellt; detta är inte ett skelett-/skinning-system.
10. Terrängkollision kontrollerar kontinuerlig höjdgradient och trädstammar;
    spelvärlden simuleras fortfarande i 2D + hopphöjd (inte riktig 3D-fysik).

**Begränsning:** Sortering på triangeldjup ger ibland fel vid överlappande
geometri eller genomskinliga objekt. Det finns ännu ingen z-buffer, skuggsystem
eller skinning. Uppdragsvärlden simuleras i 2D (x/y) medan geografin ritas i 3D;
exakt meshsamordnad kollisionsfysik och riktig glidflykt över hinder återstår.

## Minnes- och säkerhetsprinciper

- Ingen nätverksstack, plugin-inläsning, shellanrop eller användarspårning.
- Endast användarens XDG-sparmapp skrivs till.
- Inläsning av sparfil är begränsad och fältvaliderad före tillståndsbyte.
- Bevara föregående sparfil vid migration. En krasch mellan temporär sparfil
  och byte får inte avsiktligt ersätta en tidigare fungerande sparfil.
- CMake separerar modelldelen från UI så tester kan köras utan skärmmiljö.

## Nästa tekniska etapp

- Renderbuffer på låg upplösning + bilinjär/smart uppskalning som valbar fallback.
- Tydligare shader/ljus, material, markdetaljer och återanvändbara 3D-modeller.
- Kollision med höjdkarta och terrängkanter, riggade figurer, kontroller.
- Ljudsystem och fullt tillgängliga inställningar med stöd för handkontroll.
- Spelvärld med regional streamning, NPC-dialog och uppdragsgenerator.
- Separata prestandaprov på ThinkPad 4 GB respektive 2 GB system.


### 0.4: samtals- och fiendetillstånd

`World::npcs` och `World::herbs` är det enda positionsunderlaget för
interaktion, kartmarkering, kollision och renderad vy. De uppdateras av
modellen, aldrig av renderaren. Dialog har ett tillfälligt `activeNpc` som
stannar rörelsen och håller texten läsbar. Fiender använder en enkel
tillståndsmaskin: Patrol → Chase → Windup → Recover → Return.
Varningen syns under en halv sekund eller mer före skada; inget dold
attack-timing utanför modellen. `herbsCollected`, `sideQuest` och
`healerRested` lagras i en opt-in 0.4-extension; äldre sparfiler accepteras.
Inga slumpmässiga dynamiska allokeringar behövs för NPC-tillstånden.

## 0.5: inventarium och valfri ljudmotor

`Model.cpp` äger både material, utrustning och samtalsalternativ. Användargränssnittet
anropar `craft(recipe)` och `chooseDialogue(choice)`; renderaren kan inte
kopiera, tillverka eller konsumera föremål av misstag. `AETHERIA_EXTRA_5` kommer
efter den oförändrade 0.4-posten; gammal progress krediteras material.

`Sound.cpp` producerar korta signerade 16-bitars mono-sampel (22050 Hz) och
köar högst begränsad mängd ljud via SDL2. `SDL_InitSubSystem(SDL_INIT_AUDIO)`
kan misslyckas utan att grafikkoden påverkas. Ingen musik eller extern ljuddata.
Ingen ny `libwenuy`-API behövs för denna etapp.

Utrustning är för närvarande endast två fasta recept. Ett mer utvecklat
föremålssystem (staplar, utrustningsval, hållbarhet, plundringsfönster) återstår.
