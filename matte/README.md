# Matte 1.3 (C++ – utvecklingsversion)

Ett lärande matematikprogram från förskola till gymnasium för **WenuyOS** – nu i C++
(GTK 4.18, libadwaita 1.7, libwenuy statiskt länkat). Samma fönstergrund, sidopanel, statusrad
och tema som Filer och Rita. Allt på engelska och svenska, går på 2 GB-datorer.

**Installera:** dubbelklicka på `matte-1.3.0.wapp` i Filer → *Installera programmet*
(eller `wenuyos install matte-1.3.0.wapp`). Kontrollera utan att installera:
`wenuyos-get --check matte-1.3.0.wapp`. Programmet hamnar i `~/Program/Matte/` och startas med
`bin/matte`; det hittar sina data i `share/se.wenuyos.app.matte/` bredvid.

Elevdata sparas i `~/.local/share/se.wenuyos.app.matte/<elev>.json` – **samma format som
Python-versionen 0.4**, så elevernas framsteg följer med.

> **Utveckling:** 1.3.0 finns i utkast-PR #10 och är inte godkänd för släpp.
> CI måste verifiera byggnad, innehåll, 2 GB-programprofil och eventuell grafikstart
> innan installation rekommenderas. Elevdata från äldre versioner ska bevaras.

## Kursanpassat gränssnitt (etapp B, utvecklingsgren)

Det visuella uttrycket anpassas automatiskt efter valt stadium: lekfullt för förskola och
åk 1–3, lugnare för åk 4–6, avskalat för åk 7–9 och professionellt för gymnasiet.
Inställningar → Utseende → **Visuell stil** kan överstyra valet per elev:
**Automatisk**, **Illustrerad** eller **Avskalad**. Stilvalet lagras som
`visuell_stil` i befintliga elevprofilen (gamla 1.2-profiler får `automatisk`).
Det påverkar enbart UI-täthet och dekoration – aldrig elevens svårighetsnivå,
lättläst, uppläsning, tillgång till förklarande bilder eller penninmatning.

Kurslogiken har GTK-fria C++-prov i `matte-test` och skärmbildskontrollen
provar dessutom gymnasiet och högstadiet med `WENUYOS_PERF=low` och
`WENUYOS_GFX=soft`. Se `docs/ETAPP-B-GENOMFORANDEPLAN.md`. Verkliga
lerikoner är ännu inte införda. **Sparning av räknepapper finns däremot i PR #10:**
penndrag och siffror sparas lokalt per uppgiftsförsök och samma pågående
uppgift kan återställas efter omstart. Tidigare papper lagras och kan
exporteras med eleven; en särskild översikt över avslutade papper återstår.
Sparningen bygger på `src/PapperLagring.cpp` (GTK-fri JSON-modell, max 2 MiB
per fil), `src/Papper.cpp`, `src/Framsteg.cpp` och `src/Flytt.cpp`.
Se `docs/RITPLATTA.md` för begränsningar, integritet och provning.

## Ren källkod och prov

En ny checkout kan byggas med:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

CMake förbereder uppgiftsdata och **reservikoner enbart under `build/`**.
Källans `innehall/data/ikoner/` skrivs aldrig över. Finns en riktig lerikon
med samma namn kopieras alltid den, annars används en tillfällig reservbild.
Regressionstestet `matte-icon-assets` kontrollerar att byggdata inte förorenar
källmappen och att original har företräde. Testerna täcker också bråk/procent,
stavfel, omkastade siffror, facit, figurer och utskrifter.

**Viktigt före släpp:** originalpaketet med lerikoner saknas i nuvarande
GitHub-källa, så reservbilderna är inte en godkänd visuell ersättning.
Vi behöver hitta originalen och prova gränssnittet med dem. De 22 JPG-bilderna
och lerikonerna är skapade av Daniel Woxnerud med ChatGPT enligt uppgifter
i PR #10; bildlicenser och publiceringsvillkor granskas separat.
`common/` innehåller en äldre kopia av delar av libwenuy, vars publicering
och uppdateringsmodell behöver granskas innan sammanslagning.
Praktiskt prov på WenuyOS-hårdvara, särskilt Wacom Intuos, återstår.

## Hur det hänger ihop

```
innehall/        regelkorten (data/kort/*.toml), läroplanen, bilder, ordlistan (data/ordlista.toml)
verktyg/
  expandera.py   kontrollerar korten och räknar fram färdiga uppgifter → share/…/uppgifter/*.json
  testfall.py    facit för tolkningen av elevsvar (Python-motorn) → tester/fall.json
  bygg-wapp.sh   bygger, testar och packar matte-<version>.wapp (körs i Debian 13)
  skarmbild.sh   fönstret som PNG utan skärm (felsökning)
src/
  — wenuy-lara (ämnesoberoende) —
  Lara, Text     programmets id, texthjälp
  Json, Data     JSON; kort, uppgifter och ordlista
  Framsteg       Lär/Övar/Behärskar, repetition, anpassning, placering, stjärnor, Dagens tre
  Logg, Flytt    händelseloggen; exportera och importera elever
  Uppl           uppläsning (espeak-ng/Piper) med ord-för-ord-markering
  — matematik —
  Tal, Algebra   bråk, tal och uttryck – tolkar elevsvar exakt som Python-versionen
  Bedom          rättningen (feltankar, stavfel, omkastade siffror)
  Rita           alla figurer (cairo/pango) – ritas på vitt papper, syns i ljust och mörkt tema
  Svar           svarsytorna: skriva, välja, klicka, pricka in, dra linjer i koordinatsystem, dra brickor
  SvarNy         kryssa i rutor, para ihop (dra linjer), måla ytor, fri rityta
  Utskrift       arbetsblad och regelbok som PDF (cairo) – utan GTK
  Papper         räknepappret (rutat papper, ställ upp, penna, sudd, autospar)
  PapperLagring  privat versionsmärkt JSON, återställning och elevexport
  Fonster        fönstret och kommandoraden
  Test           enhetstester: alla 13 159 uppgifter rättas mot sitt facit, PDF, ordlistan
```

## Svarstyper som ritas (0.6)

| svarstyp | eleven … | i kortet |
|---|---|---|
| `kryssa` | kryssar i alla som stämmer (flera kan vara rätt) | `alternativ = [...]`, `svar = ["a", "b"]` |
| `para`   | drar linjer mellan saker som hör ihop | `par = [["3 + 4", "7"], ...]` |
| `mala`   | målar ytor med färgpennor (rättas) | figur `malrutor`/`scen`/`stapel` …, `svar = "röd\|blå\|\|röd"` eller `"röd:2\|blå:1"` |
| `rita`   | ritar fritt (rättas inte) | valfri bakgrund, t.ex. `rutpapper` eller `koordinatsystem` |

En sak kan vara text, `form:triangel`, `ikon:apple` eller `prickar:4` (en tärning).
Feltankar: kryssa per fel ruta (`"{a+1}" = "…"`), para per fel linje (`"3 + 4 → 8" = "…"`),
mala för hela svaret. Teckningarna sparas i `~/.local/share/se.wenuyos.app.matte/ritningar/<elev>/`
och syns under *Teckningar* i Översikt. Hela beskrivningen står överst i `innehall/matte/uppgifter.py`.

Nya uppgifter skrivs i korten (`innehall/data/kort/*.toml`). Kör sedan `verktyg/bygg-wapp.sh`.
Programmet räknar aldrig Python – mallarna är förberäknade (upp till 40 varianter per mall, 13 159 uppgifter).

## Bygga

```
sudo apt install cmake g++ pkg-config libadwaita-1-dev libsqlite3-dev liblua5.4-dev libsodium-dev
verktyg/bygg-wapp.sh            # → ../matte-1.3.0.wapp
cmake -S . -B build && cmake --build build && MATTE_DATA=$PWD/share/se.wenuyos.app.matte ./build/matte
```

Byggs RelWithDebInfo och strippas bara med `--strip-debug`, så `wenuyos prestanda matte` visar
funktionsnamn.

## 1.2: WenuyOS utseende, hela skärmen, räknepappret

- **Samma utseende som resten av WenuyOS.** Inga svävande "papperskort": sidorna fyller fönstret som Filer och
  Inställningar. Inställningarna är en vanlig inställningssida (rader med brytare, namn, textstorlek, röst). Menyn
  har systemets färgikoner, Hem och kartan använder systemets kortstil, lärarvyn systemets diagram, tomma sidor
  systemets tomma vy, och färgerna kommer från temat (accent, grönt för rätt, rött för fel).
- **Verktygsraden** under titelraden på kortsidan: *Regeln, Läs upp, Ledtråd, Räknepapper, Arbetsblad, A− 100 % A+,
  Fokus, Helskärm*. Den radbryts i ett smalt fönster. **Statusraden**: *Uppgift 3 av 10 · Nivå 2 av 3 · 4 rätt i rad*
  och kortets stjärnor.
- **Hela skärmen används.** Sidan mäter sin bredd: under 1000 px en kolumn, 1000–1500 uppgiften och räknepappret
  bredvid varandra, över 1500 **regeln | uppgiften | räknepappret** i tre kolumner som går att dra bredare och
  smalare. *Regeln* i verktygsraden fäller ihop regelkolumnen.
- **Räknepappret**: rutat papper (vitt med ljusblå rutor, som ett mattehäfte). Skriv – en siffra per ruta, piltangenter,
  Enter = nästa rad, `_` = streck under raden; *Minnessiffra* (eller Ctrl+siffra) skriver en liten röd siffra i hörnet;
  *← Åt vänster* för att skriva svaret från höger; *Penna* och *Sudd* för att rita och sudda; *Ställ upp* skriver talen
  i uppgiften (48 + 15, 7,7 · 2) i kolumner med streck under och markören på entalens plats. Ett nytt papper till varje
  uppgift. Rättas inte – loggen visar hur många rader eleven skrev. Sifferknapparna skriver i pappret.
- **Allt växer med skärmen**: över 1500 px blir text och figurer 15 % större, över 2100 px 30 %. Själv: A−/A+,
  Ctrl+plus/minus/0 eller *Textstorlek* i Inställningar (80–160 %, sparas per elev). Figurerna krymper till sin
  kolumn i stället för att tränga ut fönstret.
- **Hem** är rutor som fyller skärmen: *Var ska jag börja?*, *Härnäst*, *Dagens tre*, *Att repetera*, *Din kurs*
  (staplar per område), *Din samling* (stjärnor, senaste korten), *Matteorden*.
- **Kursens karta** (i menyn): alla kort i ett stadium som rutor område för område – grönt = sitter, blå ram = övar,
  *Nästa*, *Ny*. Stadierna som knappar överst.
- **Helskärm** (F11 eller knappen): bara uppgiften och räknepappret. Esc går ur.
- **Lärarvyn**: nyckeltalen som kort och ett diagram över uppgifter per dag (14 dagar).

## 1.1: regel och uppgift på samma sida, uträkningen, ny meny

- **Ett kort = en sida.** Klicka på ett kort så står regeln överst och första uppgiften direkt under –
  ingen *Öva*-knapp och ingen annan sida att hoppa till. Är fönstret brett (sidan minst 1300 px, t.ex. en
  1920-skärm) står regeln till vänster och uppgiften till höger, med var sin rullning.
  I ett smalare fönster visas **hela regeln** när kortet är nytt, och bara **kärnan** (rubrik, meningen,
  *Gäller*) när eleven redan övar på det. *Visa hela regeln* / *Visa mindre av regeln* och *Till uppgiften*.
  Samma kort igen mitt i ett pass ger samma uppgift. Att bara öppna ett kort loggas inte som avbrott.
- **Uträkningen.** Ett `?` i stegen blir en ruta att fylla i; Enter går till nästa ruta och sist till svaret.
  Är sista raden själva svaret (`= ?`) skrivs det också i svarsrutan, så man skriver bara en gång.
  **Räkna här**: egna rader (Enter = ny rad) för uppgifter man räknar ut – rättas inte, men loggen visar
  om eleven räknade (`rader`, `luckor`). Sifferknapparna skriver i rutan man står i.
- **Uppgiften** har en egen rubrik: *Uppgift 3 av 10*, prickar, uppläsning och **Fokus** (göm menyn medan
  man övar; *Visa menyn* eller Esc tar tillbaka den). *Ditt svar* står över svarsrutan.
- **Sidomenyn**: eleven överst (byt elev, ny elev), *Hem*, *Min regelbok*, *Matteorden*; **Kursen** med ett
  stadium i taget (väljare) och områden som fälls ut och ihop, med hur många kort som sitter (2/5), stjärnor
  och *Nästa* på kortet att göra härnäst. Längst ned *Inställningar* och *För läraren*.
- **Hem**: vad man ska göra härnäst, *Vad heter du?* första gången, och en stapel per område i kursen.
  **Inställningar** (lättläst, sifferknappar, lekläge, rösten) och **Min regelbok** är egna sidor.

## 1.0: lärarvy, logg, Dagens tre, sifferknappar, export

- **Händelseloggen** (`~/.local/share/se.wenuyos.app.matte/logg/<elev>.jsonl`): svar (tid, försök, feltanke),
  uppgifter som lämnas ofärdiga, uppslagna ord, ledtrådar, uppläsning, pass. Bara på datorn; högst 2 MB per elev.
- **Elevens sida** (klicka på namnet i Översikt): dagar, pass, rätt första gången, tid, avbrott; *Värt att lägga
  märke till* (t.ex. mycket bättre på bilduppgifter än skrivuppgifter, omkastade siffror, många avbrott);
  *Där det tar stopp* med arbetsblad och förkunskapskort; feltankar som kommer tillbaka; ord eleven slår upp;
  *Rensa loggen*; *Exportera elev…*.
- **Uppläsning med markering** även i regelkorten och i ordlistan.
- **Sifferknappar** (per elev): 0–9, + − x / % ( ) , och Kolla på skärmen – svara utan tangentbord.
  Inställningarna visar också vilken röst som används.
- **Stjärnor** per kort (★ ett rätt svar, ★★ behärskar, ★★★ det sitter) och *Din samling* på Hem.
- **Dagens tre**: tre uppgifter från tre kort eleven mött – blandad repetition varje dag, med dagar i rad.
- **Fler uppgifter**: 19 nya mallar i bråk, procent, ekvationer, problemlösning, mönster, tallinjen, flest/färst.
- **Exportera/importera elev**: allt om eleven i en fil (framsteg, logg, teckningar), t.ex. via USB-minne.
  `matte --exportera NAMN [fil]`, `matte --importera fil [--ersatt]`. Importen stoppar farliga sökvägar.
- **wenuy-lara**: den ämnesoberoende grunden som eget bibliotek – se `docs/NYTT-AMNE.md`.

## Matteorden och uppläsning med markering (0.9)

- **Ordlistan** (`innehall/data/ordlista.toml`, 54 ord): matteord som täljare, omkrets, median och
  ekvation, med en kort förklaring, ett exempel och en bild. Orden blir klickbara (understrukna) i
  frågor, exempel, lösningar och återkoppling – i en tredjedel av alla uppgifter. Klick ger en ruta med
  förklaringen, bilden, *Läs upp* och *Se också*. Alla ord: knappen *Matteorden* på Hem (med sökfält).
  Nya ord: lägg till i filen med alla böjningsformer; `matte check` kontrollerar att ingen form finns två gånger.
- **Uppläsning med markering**: rösten skapas först i minnet (espeak-ng eller Piper), längden mäts och
  ordet som läses markeras med gul bakgrund i frågan och exemplet. Spelas med `aplay` (alsa-utils, finns i
  WenuyOS); utan aplay läser rösten själv och markeringen följer en uppskattad takt.
- Uppläsningen säger inte längre "vad" för frågetecknet i slutet av en fråga – bara för en lucka (`3 + ? = 10`).

## Fokus och lättläst (0.8)

- **Hem** visar en sak att göra: *Hur mycket kan jag redan?*, *Repetera* eller *Fortsätt: <kort>* som stora
  knappar. Byta elev, lekläge, lättläst och lärarens översikt ligger längst ned under
  *Elev, inställningar och översikt* (ihopfällt).
- **Sidopanelen** visar bara stadierna eleven är på; *Visa alla stadier* längst ned visar resten.
- **Övningen är ett pass** på 10 uppgifter (6 i lekläget). Under passet visar sidopanelen bara kortet,
  prickar för hur långt man kommit, *Titta på regeln* och *Avsluta*. Efter passet: en paus med vad man
  gjort, sedan *Fem till* eller *Sluta för i dag*. Frågan står överst med en uppläsningsknapp bredvid,
  själva frågan (meningen med ?) i fetstil, och *Nästa* är huvudknappen.
- **Lättläst** (per elev, stöd vid läs- och skrivsvårigheter som dyslexi): större text, mer luft mellan
  bokstäver och rader, kortare rader, varm bakgrund, en mening per rad, frågor och svar läses upp
  långsammare, omkastade siffror (21 för 12) räknas inte som fel utan ger ett nytt försök. Arbetsbladen
  blir större med 6 uppgifter (`--lattlast` på kommandoraden).
- För alla: ett ord med ett litet stavfel räknas som rätt ("Rätt! Det stavas kvadrat.") – det är matte,
  inte stavning. Ord med färre än fem bokstäver måste stämma.

## Utskrift (0.7)

PDF-filerna hamnar i `~/Dokument/Matte/` (datorns dokumentmapp). Inget skickas någonstans.

- **Arbetsblad** – knappen *Arbetsblad* på varje kort: 8 nya uppgifter från lätt till svår, namn- och
  datumrad, och facit med lösningar på en egen sida. Bladet har ett nummer: samma nummer ger samma
  uppgifter igen (dela ut blad 4711, rätta mot facit 4711). Skärmens uppgifter blir pappersuppgifter:
  klicka → ringa in, dra brickor → rita brickor, para → dra streck, måla → färglägg (pennorna står under),
  rita → en ruta (med rutpapper eller koordinatsystem om uppgiften har det).
- **Min regelbok** – i menyn: elevens kort i den ordning de blivit klara, med försättsblad och innehåll.
- **Hela regelboken** – i Översikt: alla kort i läroplanens ordning (förskola → gymnasium), ~110 sidor,
  med bokmärken. Målningarna bäddas in som de är (JPEG), så filen blir ~6 MB och skarp på papper.

Från kommandoraden (läraren, skript):

```
matte --kort                                         # alla kort och deras id
matte --arbetsblad brak-delar --blad 4711 ut.pdf     # ett blad
matte --arbetsblad forskola-former,forskola-rakna --antal 4 ut.pdf   # flera kort på samma blad
matte --regelbok --elev Ali ut.pdf                   # elevens regelbok (utan --elev: hela boken)
```

Licens: GPL-3.0-or-later. Upphov: Daniel Woxnerud.
