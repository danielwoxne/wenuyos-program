# Matte – etapp B: lerikoner, sparat räknepapper och gränssnitt

**Status:** genomförandeplan (ej implementerat). **Ägare:** ChatGPT, Matte-fönstret. **Förråd:** `danielwoxne/wenuyos-program`, mappen `matte/`, befintligt utkast-PR #10 och ärende #9. **Datum:** 13 november 2026.

## Målbild och regler

Matte ska kännas som en pedagogisk digital arbetsbok: frågan syns tydligt, visuellt exempel går att förstora, och eleven kan skriva med Wacom/mus/tangentbord på ett räknepapper som sparas lokalt per försök. Programmet ska vara tillgängligt för barn och ungdomar från förskola till gymnasium. Illustrerade lerobjekt används som läromedel, medan knappar, bakgrunder, fokus och statusfärger hämtas från GTK/libwenuy-temat.

- Plattform: Debian 13, C++17/GTK4/libadwaita, libwenuy; **`WENUYOS_PERF=low`, `WENUYOS_GFX=soft` och 2 GB RAM** är krav. Ingen obligatorisk GPU, ingen nätverkstjänst eller automatisk AI-/molnöverföring.
- All användarsynlig text på svenska **och** engelska via befintligt `wenuy::tr(en, sv)`. Elevdata ligger under `datamapp()` (XDG-data), aldrig i programmappen.
- **Källkoden** går enbart via befintliga `ai/chatgpt/matte` och PR #10. `main`, släpp, programkatalog, nycklar och WenuyOS-bibliotek ändras inte i denna etapp. En annan AI granskar; Daniel godkänner alltid publicering/tagg.

## Utgångsläge (verifierat i koden)

- `src/Papper.cpp` har `GtkGestureStylus`, `GDK_AXIS_PRESSURE`, penna/mus, sudd, ångra senaste streck och rutnät. Pennpunkter lagras redan i **rutkoordinater**, oberoende av zoom. Max 10 000 punkter per **stylusstreck** finns, men en global gräns och varaktig lagring saknas.
- `src/Fonster.cpp::nyUppgift()` anropar `raknepapper_rensa(papper)` inför varje uppgift. Därför försvinner handskrift när eleven går vidare; någon återladdning efter omstart finns ännu inte.
- `src/Framsteg.cpp` lagrar elevprofil/framsteg i JSON. `src/Flytt.cpp` exporterar profilen, logg och PNG-teckningar; sparat räknepapper ingår **inte** i exportformatet.
- `verktyg/generera-saknade-ikoner.py` kan skapa reserv-PNG men ersätter inte de pedagogiska leroriginalen. `verktyg/expandera.py` ger original före reserv. `src/Data.cpp::ikon()` hämtar `<namn>.png` ur delade data. Originalmappen i det uppackade Dropbox-underlaget från 1.2 inventerades som tom; 1.4-källarkivet har inte innehållsjämförts.
- PR #10 hade 13/11 gröna `Program` och `Matte software renderer screenshots` på commit `259bbf4`: 2/2 CTest och 4/4 bilder. Det är **inte** hårdvaruprovning av Wacom eller långtidsminne. Senaste `main` ska synkas igen före slutgranskning.

## B0 – inventera och lås in befintlig funktionalitet (innan ny kod)

1. Jämför innehållet i Dropbox `matte-cpp-1.4.0-kallkod.zip` mot `matte/` på PR #10 innan enskilda filer importeras. Ta inga binärer/ZIP till GitHub.
2. Dokumentera en testmatris för 1.2-elever: profil, uppgiftsstatus, ritningar, logg, lättläst, återupptagning. Prova alltid med **syntetiska** elevprofiler i CI.
3. Spara baslinjen för GTK 1024×600 och 1600×900, samt nya prov 1280×720. Kontrollera verkliga skärmbilder, inte bara att PNG kan skapas.
4. Kontrollera licens och ursprung för varje bild och `common/`; duplicerad libwenuy måste godkännas/separat samordnas innan släpp.

**Klar när:** nulägets test/asset-inventering är spårbar och inga elevdata går förlorade när vi bygger vidare.

## B1 – riktiga lerikoner och bildstöd

### Asset-kontrakt och första leverans

- Behåll befintliga **semantiska filnamn** och `Data::ikon()` så att TOML-kort inte behöver skrivas om.
- Först skapas/återfinns **24 prioriterade motiv**: `plus`, `minus`, `lika-med`, `tallinje`, `kulram`, `kuber-gula`, `fjardedelar`, `halvor`, `procent`, `vag`, `pusselbit`, `apple`, `apelsin`, `jordgubbe`, `kaka`, `triangel`, `kvadrat`, `cirkel`, `linjal`, `tarning`, `koordinatsystem`, `passare`, `stjarna`, `bok`.
- Bygg därefter övriga **refererade** ikoner (ungefär ett hundratal kärnmotiv). Kartlägg **faktiskt refererade** namn från kort, `ikoner.py` och GUI i ett manifest, i stället för att förlita sig på generatorns cirka 283 råa strängträffar, som även kan innehålla vanliga ord.
- Bildspråk: mjukt 3D-lerutseende, neutral och transparent bakgrund, centrerat motiv, tydlig siluett, ingen text inuti bilderna, tydlig igenkänning även vid 32 px. Skapa arbetsoriginal i 256×256 eller högre, exportera optimerad PNG i 96×96 och valfri HiDPI-variant efter mätning. **Hårdkoda inte UI-temats färger i widgetkod**.
- Beskriv upphov/AI-verktyg, licens/återanvändning och eventuella externa tillgångar. Granska varje ikon visuellt; etiketter ska inte försvinna för användare som inte kan tolka bilden.

### Filer

| Fil/mapp | Åtgärd |
| --- | --- |
| `innehall/data/ikoner/*.png` | **Nya** verkliga lerikoner, semantiska namn |
| `innehall/data/ikoner/MANIFEST.md` | **Ny** motivlista, upphov, licensstatus, storlek, tillgänglighet |
| `verktyg/generera-saknade-ikoner.py` | Behåll enbart teknisk fallback i byggmappen |
| `verktyg/expandera.py` | Kontrollera korrekt kopiering/originalprioritet |
| `verktyg/test-ikoner.py` | Utöka till saknade kärnnamn, PNG-signatur, filgränser och fallback |
| `src/Data.cpp` / `src/Rita.cpp` | Begränsa minnescache och prova bildstorlekar; använd Datatyper när detta är verifierat med systemets C-API |

**Acceptans:** inga bokstavsplatshållare i de 24 första motiven, original PNG tar alltid företräde, samtliga referenser valideras, transparens och tillgänglighet är visuellt granskade på både små och stora skärmar. Inga byggen får skriva i källmappen.

## B2 – autosparat räknepapper (utan handskrifts-AI)

### Datamodell (förslag, **inte** ett existerande API)

Skilj ritmodellen från GTK-widgeten, använd ett versionsmärkt lokalt JSON-format. Punkternas `x` och `y` anges i **rutkoordinater**, inte skärmpixlar, så att zoomning och fönsterstorlek inte ändrar uträkningen. Färg väljs med semantiska pennroller (exempelvis `ink`), och faktisk ritfärg hämtas från aktuellt tema.

```json
{
  "format": "se.wenuyos.matte.raknepapper",
  "version": 1,
  "kort": "addition-tiotalsovergang",
  "mall_index": 0,
  "variant_id": "sha256-av-stabilt-uppgiftsinnehall",
  "forsok_id": "lokalt-genererat-slump-id",
  "rutor": [{"rad": 1, "kol": 2, "text": "4", "liten": ""}],
  "summastreck": [{"rad": 2, "fran": 2, "till": 4}],
  "pennstreck": [
    {"roll": "ink", "basbredd": 1.0, "punkter": [[2.12, 3.08, 0.5], [2.22, 3.28, 0.7]]}
  ]
}
```

Lagra i `datamapp()/raknepapper/<sanerat-elev-id>/<forsok-id>.json`. Ett litet, separat index håller reda på **aktuell uppgift**, dess stabila variant-ID och återupptagningspekare. Dela inte filen mellan olika elever. Vid byte till en ny uppgift: spara och markera föregående försök först; öppna nytt papper först därefter. Om samma aktiva uppgift återupptas efter omstart ska pappret återställas.

- Spara efter avslutat pennstreck/musdrag, radering, tangentändring samt innan uppgiftsbyte/avslut; samordna täta ändringar med exempelvis en kort GLib-idle/debounce, **ingen bakgrundstjänst**.
- Skriv till temporär fil och ersätt målfilen atomärt; varna användaren vid diskfullt/fel. Överskriv **inte** en fungerande kopia med trasigt JSON. Visa `Sparat`/`Saved` bara efter verifierad lyckad skrivning.
- Sätt uttryckliga, provade gränser: exempelvis max 50 000 punkter och 250 streck per papper, max 2 MiB per fil samt en dokumenterad lagringsbudget per elev. Varna vid gränsen – kasta **aldrig tyst** bort arbete. Komplettera med samplingsavstånd för mus och penntryck; nuvarande begränsning 10 000 per stylusstreck räcker inte ensam.
- Spara också rutans text, minnessiffror, summastreck och markör/läge där detta hjälper återupptagning. **Rätta inte handskrift automatiskt**: eleven använder ordinarie svarsyta.
- `Rensa` ska kräva bekräftelse om sparat innehåll finns; `Ångra` ska fungera också efter återladdning. Tangentbord, mus, pekskärm och Wacom får inte generera dubbla streck.
- Export/import: uppdatera `Flytt.cpp` till ett bakåtkompatibelt tillägg för räknepappersfiler med strikta filnamns- och storleksgränser. Gamla elevpaket ska fortfarande gå att läsa och ingen import får kunna skriva utanför elevmappen.

### Filer

| Fil | Åtgärd |
| --- | --- |
| `src/Papper.cpp`, `src/Papper.h` | Återanvänd ritningen, gör kontrollerat export/import av modell, signal för ändringar och återladdning |
| `src/PapperLagring.h/.cpp` | **Nya** GTK-fria serialiserings-/lagringsfunktioner (föreslagna namn) |
| `src/Fonster.cpp` | Koppla sparning till `nyUppgift()`, uppgiftsbyte, stängning och återupptagning |
| `src/Framsteg.cpp/.h` | Håll pekare till aktuell uppgift/försök bakåtkompatibelt, utan att ändra gamla nycklars betydelse |
| `src/Flytt.cpp/.h` | Exportera/importera räknepappersfiler säkert |
| `src/Test.cpp` | Roundtrip, version 1, skadad/trunkerad JSON, path traversal, storleksgränser, misslyckad skrivning, olika elever, 1.2-kompatibilitet |
| `CMakeLists.txt`, `docs/RITPLATTA.md` | Bygg/test för GTK-fri modell och uppdaterad hjälp |

**Acceptans:** samma handskrift, tryck, rutpositioner och skrivna siffror överlever byte av uppgift, omstart, zoom och export/import; programmet visar fel vid utebliven sparning utan att radera gammal fil. `WENUYOS_PERF=low` kräver ingen GPU eller tjänst.

## B3 – ett lugnare och mer användbart gränssnitt

### Layout och beteende

- **1024×600:** smal navigering kan döljas; övningen öppnar med fråga och svar synliga utan lång bläddring. Förklaringsbild visas som mindre förhandsbild och kan förstoras/fällas ihop. Räknepapper öppnas i en tydlig växling `Uppgift / Räknepapper`, inte som en alltför smal sidokolumn.
- **1280×720:** frågan och pappret kan ligga bredvid varandra, med minskbar sidomeny. Behåll `Till uppgiften` ovanför stor regelbild.
- **1600×900+:** valbara tre kolumner: regel/exempel | uppgift | räknepapper. Eleven får ändra bredden.
- På alla skärmar: uppgiftsrubrik, svarsfält, `Ledtråd`, `Läs upp`, `Penna`, `Sudd`, `Ångra`, `Sparat`, `Nästa uppgift`. Mindre verktyg läggs i en meny, inte flera långa verktygsrader.
- Fokuskontroll, tangenter, skärmläsar-etiketter, lättläst läge och tydlig kontrast. Inget blinkande eller visuellt överbelastande.
- Behåll lerikoner som komplement till **textetiketter** och stöd för svenska och engelska. Tema och läge (ljust/mörkt) via libwenuy.

### Filer

| Fil | Åtgärd |
| --- | --- |
| `src/Fonster.cpp` | Responsiv layout, toppfält, knappar, bildstorlek, vyn `Uppgift/Räknepapper`, sparindikator |
| `src/LektionsytaGtk.cpp` | Integrera övningshjälp utan att gömma svarsrutan |
| `src/Papper.cpp` | Anpassa verktygsrad, sparstatus och storlekar |
| `src/VisuellStil.h` och befintlig GTK/libwenuy-stil | Använd tema-tokens, inte hårdkodad UI-palett |
| `verktyg/prova-soft.sh` | Utöka skärmbildstester till 1024×600, 1280×720 och 1600×900 |
| `README.md`, `docs/RITPLATTA.md` | Kort användarvägledning i båda språken |

**Acceptans:** eleven kan se frågan och starta sitt arbete på 1024×600 utan att scrolla förbi en hel målning. Alla viktiga knappar nås med tangentbord och på pekskärm. Bilder får aldrig skjuta undan svarskontrollen permanent.

## Ordning, granskningsgrindar och ansvar

| Steg | Vem | Klart när |
| --- | --- | --- |
| B0 källa/licens/baslinje | ChatGPT Matte, ev. Daniel för originalfiler | 1.4-arkiv jämfört, licensbeslut synligt i PR |
| B1 lerikoner | ChatGPT Matte; Daniel bekräftar godkänt bildurval/licens | Kritiska 24 riktiga PNG finns och är visuellt testade |
| B2 autosparat räknepapper | ChatGPT Matte | Roundtrip och elevbyte/omstart/export fungerar i CI |
| B3 gränssnitt | ChatGPT Matte | GTK-skärmbilder + manuell 1024×600/1280×720/1600×900 kontrollerade |
| Oberoende granskning | Claude eller annan AI | Kod, tillgångar, minnesbudget och testresultat granskade; `Granskat och godkänt` |
| Släpp `matte-v…` / Programbutiken | **Daniel** | Uttryckligt släppgodkännande, signering och fungerande uppdateringsprov |

### Tester före release

1. `cmake -S matte -B matte/build -DCMAKE_BUILD_TYPE=RelWithDebInfo`, `cmake --build matte/build -j2` och `ctest --test-dir matte/build --output-on-failure`; kontrollera att nya tester är gröna.
2. `wenuyos verify` på Debian 13 med `WENUYOS_PERF=low WENUYOS_GFX=soft`; separat Cairo-skärmbildstest och minnesmätning. Som **planeringsbudget**, inte uppmätt resultat: högst cirka 16 MiB tillkommande ritmodell/spararbuffert.
3. Återuppta en syntetisk elevprofil efter flera omstarter, felaktiga/stora sparfiler och avbruten skrivning. Testa att gamla 1.2-data fortfarande kan importeras och exporteras.
4. Prova fysisk Wacom Intuos, mus, pekskärm, svensk/engelsk text, fokus och läsbarhet på lågupplöst hårdvara.
5. Utvärdera 1.4-källans eventuella nya funktioner och bibliotekets rättigheter före sista merge. Inga `.wapp` eller ZIP i Git. **Ingen release före Daniels godkännande.**

## Designmål – koncept, inte skärmbild från fungerande version

Ljust nordiskt WenuyOS-tema, mörkgrå text och stora väl avgränsade ytor. Till vänster en smal navigering med färgglada lerikoner och text; i mitten aktuell övning med visuella tiotal/ental och ledtråd; till höger ett kvadrerat räknepapper med naturtrogen handskrift och penna/sudd/ångra. Överst elevens kurs och framsteg, nederst tydligt svarsfält och Nästa uppgift. På 1024×600 ersätts högerspalten av växlingen `Uppgift / Räknepapper`. Ingen obegärd nättrafik eller handskriftsigenkänning.
