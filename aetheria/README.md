# Aetheria — Elementens värld

**Version 0.5.0 (fjärde större utvecklingsetappen, 10 november 2026).**
Ett självständigt, resurssnålt fantasy-actionrollspel för WenuyOS, byggt i C++17.
Ett eget spel, inte Genshin Impact eller en kopia av skyddade spelresurser.

## Vad som fungerar i den här versionen

- **Perspektivprojekterad 3D-vy** med trianglar, lågpoly-landskap, skog,
  ruiner, fiender, kristaller och karaktärer med tredimensionella kroppsdelar.
- **Steg 3:** gång-/springanimationer beroende på faktisk förflyttning, klädkappa
  som rör sig, 3D-svärdattack, nya fiendeformer och glidflygningsvingar.
- Delad deterministisk vegetation för spelkollision och grafiken: man kan inte
  gå genom träd. Brantare lutningar hindrar gång uppför sluttningar.
- Dag-/nattcykel på fyra minuter, sol/måne, stjärnhimmel, skiftande färgljus,
  dimma, små fjärilar, eldflugor och två dekorativa vandrande bybor.
- Alla grafiska resurser är procedurformer (inga nedladdade modellpaket).
- Tredjepersonskamera med rotation, kamerarelativ rörelse och frivilligt 2D-läge.
- Hopp och prototyp för glidning. Glidning förbrukar ork när man faller.
- Grundläggande strider med fyra element: eld, is, vind och åska.
- Quest: samla fem kristaller, besegra fyra väktare, aktivera altaret med E.
- Karta, hjälpruta, paus, helskärm, sparning/laddning och svenska/engelska texter.
- **0.5.0:** väska/inventarium med kristallskärvor och elementessens; tillverka
  och utrusta ett runsvärd (ökad närstridsskada) och en skogsamulet (mer ork,
  mindre inkommande skada). Utrustningen syns även på huvudkaraktären.
- **0.5.0:** dialogval med tangent 1/2 (eller E för det första alternativet);
  det går att avstå från uppdrag och läkningen.
- **0.5.0:** valfria syntetiserade ljudeffekter för strid, magi, fynd,
  dialog, tillverkning och skada, utan nedladdade ljudfiler. M stänger av ljudet.
- Programmet har ingen nätverksfunktion, ingen telemetri och inga kontokrav.

**Teknisk ärlighet:** Detta är en tidig lågpoly-3D-prototyp. Det är *inte* en stor
öppen AAA-värld. Den har ännu inga texturer, musik, skelettbaserade animationriggar,
klättring, utförliga berättelseuppdrag eller spelkontrollstöd. Ljudet är för närvarande bara korta effekter.

## Styrning

| Tangent | Funktion |
|---|---|
| WASD / piltangenter | Gå (kamerarelativt i 3D) |
| Shift | Spring (förbrukar ork) |
| Z / C | Rotera 3D-kameran |
| V | Växla mellan 3D och föregående 2D-vy |
| F2 | Växla mellan låg och hög geometri-/detaljnivå |
| F | Hoppa |
| Håll G | Glid långsammare under fall (förbrukar ork) |
| J / blanksteg / vänsterklick | Attack |
| Q / K | Byt element / använd vald elementkraft |
| R | Hälsodryck |
| I | Öppna/stäng väska; 1/2 tillverkar utrustning i väskan |
| 1 / 2 i samtal | Välj dialogalternativ (E väljer alternativ 1) |
| M | Ljud på/av |
| E | Aktivera altaret när uppdraget är klart |
| Tab / H | Stor karta / hjälp |
| Esc / F11 | Paus / helskärm |
| F5 / F9 | Spara / ladda |
| N | Nytt spel efter vinst/förlust |

## Installation på WenuyOS

Öppna **`aetheria-0.5.0.wapp`** med WenuyOS-installatören, eller prova:

```sh
wenuyos install aetheria-0.5.0.wapp
```

Manifestet anger beroendet **`libsdl2-2.0-0`**. WenuyOS behöver kunna
installera eller tillhandahålla detta. Paketet har byggts för **Linux amd64**.
**Vi har inte verifierat en verklig installation på WenuyOS; testa på en separat
maskin innan bred spridning.**

## Bygga själv (Debian 13 eller motsvarande)

```sh
sudo apt-get install cmake g++ libsdl2-dev python3
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/aetheria
```

`--2d` startar det äldre 2D-läget. `--high-detail` väljer längre ritavstånd.
`--frames 60` avslutar efter 60 bilder för värdprov.
`--preview-time 120` flyttar scenklockan till midnatt i ett utvecklarprov.
`--screenshot fil.ppm` tar en riktig bild från spelvyn utan dialogruta.
SDL2-version **2.0.18 eller senare** krävs på grund av `SDL_RenderGeometry`.

För att bygga `.wapp`:

```sh
cmake --install build --prefix "$PWD/stage"
cp -r data docs wenuyos-app.toml LICENSE README.md THIRD_PARTY.md stage/
python3 make-wapp.py stage -o aetheria-0.5.0.wapp
```

System med SDL2-körbibliotek men utan SDL2-headers kan byggas via projektets
begränsade `MiniSDL.h`-adapter (x86-64/Linux). För normal utveckling
rekommenderas alltid officiella SDL2-utvecklingsheaders.

## Sparfiler

Sparning sker under `$XDG_DATA_HOME/aetheria/save1.txt` eller
`~/.local/share/aetheria/save1.txt`. Vid första start försöker programmet läsa
föregående prototyps `wenuy-legends/save1.txt` **om ny sparfil inte finns**,
och skriver då en kopia till Aetherias sparfil. Det gamla originalet lämnas kvar. Om en äldre sparning placerar figuren
precis i ett nytt träd försöker 0.4.0 hitta en säker plats inom två meter;
originalsparfilen ändras inte. **0.5.0 lagrar tillverkade föremål och
återstående material i en separat, validerad `AETHERIA_EXTRA_5`-post.**
Tidigare versioners sparfiler kan laddas, och material krediteras för redan
plockade kristaller och besegrade fiender. Äldre versioner av Aetheria kan inte
läsa sparfiler skrivna av 0.5.0, så ta gärna säkerhetskopia före uppgradering.
Dagens tid sparas ännu inte.
Sparningen är textbaserad och valideras före inläsning.

## Resursmål och licens

Designmålet är att fungera på **2 GB RAM**, även med mjukvarurendering.
Detta är inte liktydigt med verifierad framerate på äldre verklig hårdvara.
En nedbantad 3D-vy är standard, och äldre 2D-läget finns kvar för jämförelse.

Egen källkod och procedurgenererad grafik: MIT (se LICENSE). SDL2 länkas
mot systemets runtime, men distribueras inte i `.wapp` (se THIRD_PARTY.md).

**Namnvarning:** Namnet *Aetheria* används redan av andra spel. Det är endast
ett **arbetsnamn** tills en varumärkes-/namnkontroll har genomförts.

Se `docs/ROADMAP.md`, `docs/ARCHITECTURE.md`, `docs/TESTPLAN.md` och
`docs/PERFORMANCE.md` för fortsatt utveckling.

## Nytt i 0.5.0 — väska, utrustning och ljud

- Väska: **I** öppnar en stor tydlig översikt. **1** bygger runsvärd av
  2 kristallskärvor + 2 essens. **2** bygger skogsamulet av
  1 kristallskärva + 3 essens. Varje föremål kan tillverkas en gång.
- Skärvor tillkommer när en kristall plockas upp; essens vid besegrad fiende.
  Redan intjänade material migreras från äldre sparfiler.
- Runvärdet ger +18 skada per träff, och skogsamulet ökar orkens återhämtning
  från 20 till 27 per sekund och reducerar inkommande fiendeskada med 20 %.
- Tillverkade föremål visas även på 3D-hjälten. Både 2D och 3D används.
- Två dialogval visas när man pratar med Arin eller Sera; välj **1** eller **2**,
  eller tryck **E** för första valet. Avböjande fungerar utan att uppdrag/läkning startar.
- Ljudmotor använder SDL2:s ljudkön och genererar korta toner matematiskt.
  Den fungerar offline och kan stängas av med **M**. Om en ljudenhet inte kan
  öppnas fortsätter spelet helt utan ljud. `--no-audio` kan användas vid värdprov.
- Nya bildlägen för utvecklartest: `--preview-inventory`, `--preview-gear`.
  Dessa påverkar bara tillfällig skärmbildsdata och skriver inte till sparfil.

## Nytt i 0.4.0 — samtal, örter och fiender

- Två **interaktiva invånare**: Äldste Arin (ger sidouppdraget Läkeörter) och
  läkaren Sera (engångsläkning). När du står nära: tryck **E** för att öppna
  ett samtal, **E** för nästa dialograd eller val, **Esc** för att lämna.
- **Sidouppdrag:** prata med Arin, samla tre gröna läkeörter på ängarna och
  återvänd för två hälsodrycker och 35 erfarenhetspoäng. Örter kan hittas
  innan du accepterar uppdraget; de räknas retroaktivt.
- **Strid:** fiender patrullerar, jagar inom upptäcktsavstånd och återvänder
  till sitt ursprung när spelaren försvunnit. Röda varningsmarkeringar visar
  att fienden laddar ett anfall. Hopp eller undanmanöver kan undvika skada.
- Eldkraften kan orsaka tidsbegränsad brännskada. Närstrid prioriterar
  närliggande fiender framför spelaren. Rörelse och dialog är separerade.
- Kartan visar invånare (guld) och läkeörter (grönt). Nya visuella effekter
  finns i både 2D- och 3D-läge; svenska texter stöds.
- Sparfilerna från 0.1–0.3 går fortfarande att läsa. 0.4 lägger till
  `AETHERIA_EXTRA_4` efter det gamla sparformatet. Själva dialogrutan
  och temporära AI-tillstånd sparas inte.

Det finns **ingen AI-språkmodell** inbyggd i invånarna. Samtalen följer
förskrivna rader och spelregler så att spelet förblir förutsägbart och lättdrivet.

Spelet är fortfarande en liten prototyp. Grafik, ljud, större värld,
karaktärssystem och en riktig GPU-baserad renderingsmotor behöver fortsatt
utveckling. Versionsnumret avser spelet och garanterar inte stöd på fysisk
WenuyOS-hårdvara förrän det har provats där.

## Återskapade 0.6-funktioner i GitHub-PR #13 (utkast, inte släppt)

Det ursprungliga 0.6.0-arkivet saknas, men beteendet har rekonstruerats från den tidigare utvecklingsspecifikationen ovanpå den existerande 0.5.0-spelmodellen. **Versionsnumret behålls på 0.5.0 i utkastet tills full verifiering och beslut om släpp.**

- **Uppdragsmål:** en panel visar närmaste återstående kristall, levande vakt eller altare med avstånd och kamera-relativ riktning. Målet markeras också gult på den vanliga och stora kartan. Om huvudspelets uppdrag inte har ett tillgängligt mål kan läkeörtsuppdraget visas.
- **Kamera:** vrid med **höger musknapp och dra** eller **Z/C**; zooma med **mushjulet** (begränsat så att kameran inte hamnar i spelaren). Gäller 3D-vyn; **V** växlar som tidigare till 2D.
- **Mus:** klicka på Väska, Karta, Hjälp, stridsknappar, val i dialog och tillverkningsrecept. Klick genom öppna dialoger orsakar aldrig stridsanfall.
- **Autosparning:** ungefär **var 90:e sekund aktiv speltid**, inte under paus, dialog, avslutat spel eller skärmbildsförhandsvisning. Vid skrivfel försöker spelet igen tidigast efter 15 nya aktiva sekunder. F5/F9 fungerar som tidigare.
- **Sparkompatibilitet:** exakt samma `World::save/load` och befintliga `AETHERIA_EXTRA_4/5` används. Kamera-, mål- och autosparningsklocka är enbart tillfälliga tillstånd.

**Kontroll som återstår:** testsviten `aetheria-stage6-restored-controls`, full GUI/SDL2-kompilering på Debian 13, `wenuyos verify`, verkliga skärmbilder och 2 GB-hårdvaruprov. Kod finns på GitHub-grenen `ai/chatgpt/aetheria-ui`; inga nya `.wapp` har släppts. Beroende motorintegrering finns i separat PR #15.
