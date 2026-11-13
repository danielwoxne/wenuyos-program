# Matte

Ett lärande matematikprogram från förskola till gymnasium – ett **WenuyOS-program**
i Python på libwenuy: samma fönstergrund, titelrader, sidopanel, statusrad, toast,
⋯-knapp och tema som Filer och Inställningar. Allt på engelska och svenska (`tr`),
data i textfiler, går på 2 GB-datorer.

**Installera:** dubbelklicka på `matte-<version>.wapp` (WenuyOS programpaket: kontrollsummor,
manifest, ev. signatur) – eller på zip-filen med källkoden – i Filer → *Installera programmet*
(eller `wenuyos install matte-0.4.0.wapp`). Bygg .wapp själv i projektmappen: `wenuyos app pack`,
signerat: `wenuyos app key "Ditt namn"` en gång, sedan `wenuyos app pack --sign`. WenuyOS packar upp den i `~/Program/Matte/`
och lägger Matte i Start. Kontrollera utan att installera: `wenuyos-get --check matte-0.4.0.wapp`.

Plattform: WenuyOS 0.4 (Debian 13, Python 3.13, GTK 4.18, libadwaita 1.7, libwenuy).
Programmet använder bara standardbiblioteket, `gi` och `cairo` – inga pip-paket. Det enda
extra är `espeak-ng` för uppläsningen, som står under `requires` i `wenuyos-app.toml`
och installeras av WenuyOS. Elevdata sparas i `~/.local/share/se.wenuyos.app.matte/`
(aldrig i programmappen), arbetsblad och regelböcker i `~/Dokument`.

```
python3 main.py                 # fönstret
python3 main.py kort            # korten och läroplanens mål
python3 main.py regelbok ut.html   # hela regelboken som HTML (utskrift)
python3 main.py regelbok --elev    # elevens egen regelbok
python3 main.py ova brak-delar  # öva i terminalen
python3 main.py check           # kontrollerar kort, läroplan, figurer, ikoner
python3 -m unittest discover -s tests
./pack.sh                        # (utanför WenuyOS) bygger ett .deb för vanlig Debian/Ubuntu
```

Saknas libwenuy (vanlig Debian/Ubuntu) ritar `matte/skal.py` samma fönster med libadwaita,
så programmet går att utveckla där – men det är WenuyOS det är gjort för. Motorn,
korten och regelboken fungerar helt utan GTK (ren Python 3.11+).

Program-id: `se.wenuyos.app.matte` – samma i `.desktop`-filnamnet, `StartupWMClass` och
`application_id`. Struktur enligt WenuyOS programregler: en mapp `matte/` överst i zip-filen
med `main.py`, `data/<id>.desktop`, `data/<id>.svg`, `wenuyos-app.toml` och `tests/`.

## Idén: regelboken

Kärnan är **regelkort** – som de inklistrade korten i en skolregelbok. Varje
kort har rubrik, en kärnmening, rader med *man säger – bild – man skriver*,
ett eller flera exempel, **när regeln gäller** (och när den slutar gälla),
samt uppgiftsmallar. Korten ligger i `data/kort/*.toml`. Läroplanen i
`data/laroplan/` pekar på korten per stadium och område; korten själva vet
inte vilken årskurs de hör till, så samma kort kan återkomma i flera stadier.

Eleven möter korten i läroplanens ordning. Varje kort hon sett hamnar i
*hennes* regelbok, i den ordningen, och kan skrivas ut.

## Pedagogisk grund

| Princip | Var i programmet |
|---|---|
| **Konkret → bild → symbol** (CRA; Bruner, Witzel) | Varje rad på ett kort: ord, figur, symbol sida vid sida. Figurerna ritas av programmet och kan göras om med andra tal. |
| **Flera representationer** (Lesh; Dehaene) | Uttrycksmotorn räknar exakt med bråk, så 1/2, 0,5, 50 % och 2/4 bedöms som *samma tal*. Uppgifter där man växlar form, inte bara räknar. |
| **Variationsteori** (Marton; Learning Study) | `var` i mallarna styr exakt vad som varierar. Bråkstaplarna: samma helhet, bara antalet delar ändras. |
| **Regler som slutar gälla** (Karp, Bush & Dougherty) | `galler` och `galler_inte` på kortet. "Multiplikation gör talet större" gäller för heltal – kortet säger det, och programmet tar upp regeln igen när gränsen passeras. |
| **Lösta exempel som tunnas ut** (Sweller; Renkl; kognitiv belastning) | Varje kort har minst två `[[exempel]]` som räknas steg för steg. Mallar med `steg` är exempel med en lucka: stegen visas, eleven fyller i det som saknas. `niva`: 1 = nästan löst, 2 = vanlig, 3 = svårare/överföring. Nivån stiger med rätt i rad. |
| **Förklarande återkoppling** (Shute; Hattie & Timperley) | Första felet ger feltankens förklaring eller ledtråden – eleven får försöka själv. Andra felet visar `losning`: så här kan man tänka, steg för steg. Bara första svaret räknas mot Behärskar. |
| **Många olika uppgifter per kort** (variation; interleaving) | Minst 8 mallar per kort: samma idé från olika håll – räkna, hitta det som saknas, välj rimligt svar, hitta felet, en vardaglig situation. `matte check` säger till om ett kort blir tunt. |
| **Feltankar i stället för rött kryss** (Swan; diagnostisk undervisning) | `feltankar`: kända fel svar har en egen förklaring ("du har vänt på bråket"). Okänt fel ger ledtråden, aldrig bara facit. |
| **Hämta ur minnet, glesare med tiden** (Roediger & Karpicke; Cepeda) | `framsteg.py`: Lär → Övar → Behärskar; behärskade kort repeteras efter 1, 3, 7, 14, 30 … dagar; fel kortar intervallet men nollställer inte. |

## Scenbilder

`data/bilder/` – Daniels målade scener (stationsklockan, pizzan i åttondelar, verkstaden,
brickorna). Ett kort visar sin scen överst med `bild = "pizza-attondelar.jpg"` och
`bildtext = "…"`: verkligheten före staplarna och symbolerna – det första steget i CRA.
JPEG ≤ 1280 px, ~200 kB per bild.

Målningar som uppgiftsbild: `figur = { typ = "scen", bild = "…", omraden = [...], namn = [...] }`.
Klickytorna sätts med `matte omraden <bild>` – en sida där man drar rutor över målningen.
Vilka målningar som behövs, med exakta antal: `docs/bildbestallning.md`.

## Ikoner

`data/ikoner/` – Daniels 100 lerikoner (siffror, räknetecken, former, kroppar,
diagram, vardagsföremål). Används i sidopanelen (`matte/ikoner.py` knyter område
och stadium till ikon), på korten (`ikon = "tarning"`), som räknebilder
(`figur = { typ = "ikoner", namn = "apple", antal = 7 }`) och för rätt/fel.

## Placeringstest

"Hur mycket kan du redan?" – per område börjar testet i mitten av kortföljden
(åk 1–3 → 4–6 → 7–9), en uppgift på nivå 2, uppåt vid rätt, nedåt vid fel.
Efter högst ~5 frågor per område vet programmet var det blir nytt. Allt före
markeras som *Behärskar* och repeteras om några dagar, så det som ändå inte
satt fångas upp. Ingen väljer årskurs.

## Regler som slutar gälla

`galler_inte` på ett kort beskriver var regeln slutar gälla. `ateruppta` på ett
senare kort listar vilka regler som ska tas upp igen där: när eleven kommer
till *Multiplikation med decimaltal* visas "Minns du Multiplikation med 10,
100 och 1 000? Regeln 'multiplikation gör talet större' …" – men bara om hon
har mött det kortet.

## Lekläget (förskola och åk 1–3)

En brytare på Hem, sparad per elev. Stora knappar och figurer (1,5×), bara förskolans och
lågstadiets kort i sidopanelen, och klicka/dra/välj i stället för att skriva. Frågorna
**läses upp** med en fri svensk röst på datorn – Piper om den finns, annars espeak-ng
(espeak-ng installeras av WenuyOS tillsammans med Matte). Allt lokalt. 🔊-knappen finns på alla kort och uppgifter,
även utan lekläge (för den som läser långsamt).

Förskolekorten följer Lpfö 18: hur många, flest/färst, siffrorna, formerna och mönster –
med lerikonerna som material och brickor att dra till en tioram.

## Anpassning efter feltankar

Programmet räknar *vilken* feltanke en elev gör (samma felsvar på samma uppgiftsmall,
oavsett vilka tal som slumpades). Har den kommit två gånger blir den uppgiften vanligare,
med ledtråden framme ("Tänk på: …"), tills eleven klarar den – varje rätt svar räknar ned.
Fastnar eleven (4 fel av 6) föreslås ett förkunskapskort. Se `matte/adaptiv.py`.

## Elever, översikt och arbetsblad

Flera elever på samma dator (väljare och "Ny" på Hem). **Översikt** visar per elev vad som
behärskas och ska repeteras, och gruppens vanligaste feltankar med programmets förklaring –
underlag för en genomgång. **Arbetsblad** på varje kort ger 8 nya uppgifter att skriva ut,
med facit på egen sida; bladnumret ger samma uppgifter igen. Allt ligger på datorn.

```
matte arbetsblad brak-delar gy-trigonometri --antal 6 --blad 4711
matte oversikt
```

## Gymnasiet (Gy25 nivå 1–2)

20 kort: procentenheter, ränta på ränta, potenslagar, grundpotensform, rötter, ekvationer,
räta linjen, funktioner, exponentialfunktioner, trigonometri, sannolikhet i flera steg,
spridning, likformighet, kvadreringsregler, andragradsekvationer och -funktioner,
ekvationssystem och logaritmer. Motorn räknar exakt med rötter och bråkexponenter, och
har sin/cos/tan (grader), lg, ln och π. Uppgifter kan ha `tolerans` ("svara med en decimal")
och `oordnad = true` (x₁ och x₂ i valfri ordning). Grafer: `funktioner = ["x^2 - 4x + 3"]`
och `linjer = [[k, m], …]` i koordinatsystemet.

## Rita, färga, pricka – och rätta

Allt eleven gör i en figur **snäpps till rutnätet**, så programmet behöver aldrig
tolka en skakig linje: ett klick på tallinjen blir närmaste streck (ett tal), ett
klick i koordinatsystemet blir en gitterpunkt (x, y), två klick blir en linje
som rättas mot k och m. Färgade rutor rättas på antal. Feltankar fungerar som för
skrivna svar – (y, x) i stället för (x, y) har sin egen förklaring. Skissrutan är
det enda som inte rättas: den är elevens eget papper vid problemlösning.

## Kortformatet

```toml
id = "brak-delar"
titel = "Bråk – delar av en hel"
omrade = "tal"              # tal · algebra · geometri · sannolikhet · samband · problem
sager = "Ett bråk säger hur många lika stora delar …"
galler = "När helheten är delad i LIKA stora delar."
galler_inte = ""            # regel som slutar gälla, om det finns en
forkunskap = []             # kort som bör sitta först
kalla = "CRA. Variationsteori."

[[rad]]                     # man säger – bild – man skriver
sager = "en fjärdedel"
figur = { typ = "stapel", delar = 4, fyllda = 1, farg = "gron" }
skriver = ["1/4"]

[[exempel]]
rubrik = "Exempel: 8,50 · 10"
figur = { typ = "positionstabell", tal = "8,50", skift = 1 }
steg = ["· 10 → ett steg åt vänster.", "Svar: 85,0 = 85."]

[[uppgift]]
niva = 2
fraga = "Vilket bråk visar bilden?"
figur = { typ = "stapel", delar = "{n}", fyllda = "{k}" }
svar = "{k}/{n}"
form = "brak"               # brak · brak-enklast · decimal · procent · auto · text
svarstyp = "text"           # text · uttryck (x, 2a+3) · val (alternativ = [...]) ·
                            # klick/farga (markera rutor i figuren; svaret är antalet) ·
                            # tabell (falt = [...], svar = [...]) ·
                            # punkt (pricka in på tallinje: svar = "0,7"; i koordinatsystem: "{x};{y}") ·
                            # linje (två gitterpunkter → k och m: svar = "{k};{m}") ·
                            # skiss (ritruta som inte rättas + textsvar) ·
                            # valj (klicka på EN sak i en målning/ikonrad; svar = dess namn) ·
                            # markera (klicka på ALLA rätt saker; svar = "gul,gul,gul") ·
                            # dra (dra brickor till en tioram; svar = antalet)
tolerans = 0.05             # godta svar så här nära facit
oordnad = false             # flera fält i valfri ordning
var = { n = "3..10", k = "1..n-1" }     # "2..10 != a", "[10,100,1000]"
ledtrad = "Nämnaren = alla delar. Täljaren = de färgade."
feltankar = { "{n}/{k}" = "Du har vänt på bråket – …" }
steg = ["Alla delar: {n} – nämnaren.", "Färgade: {k} – täljaren."]   # löst exempel med lucka (före frågan)
losning = ["Täljare {k}, nämnare {n}.", "{k}/{n}."]                   # så här tänker man (efter två fel, i facit)
```

Platshållarna `{…}` är små Python-uttryck i variablerna: `{a*b}`, `{max(a,b)}`,
`{t(m)}` (”+ 3”/”− 3”), `{ts(n)}` (”56 000”), listuttryck som
`{', '.join(str(i) for i in range(1, n))}`, och för gymnasiet `sqrt`, `lg`, `sin`/`cos`/`tan`
(grader) och `atand`/`asind`/`acosd`. Går ett uttryck inte att räkna ut stoppar `matte check`.

Figurer (`matte/rita.py`): `stapel`, `staende`, `tiostaplar`, `hundraruta`,
`positionstabell`, `tallinje` (även negativa tal och decimaler), `brak`,
`rektangel`, `triangel`, `stapeldiagram`, `koordinatsystem`, `tiobas`, `prickar`,
`talkamrater`, `klocka`, `tabell`, `ikoner`. Samma ritkod används i fönstret (cairo)
och i regelboken (SVG), så korten ser likadana ut på skärm och papper.

## Filer

```
main.py               start (wenuyos run)
matte/uttryck.py      uttrycksmotorn (tolka, likvardiga, formatera)
matte/algebra.py      uttryck med variabler, likvärdighet, linjära ekvationer
matte/placering.py    placeringstestet
matte/adaptiv.py      anpassning efter feltankar
matte/tal.py          uppläsning (Piper / espeak-ng)
matte/oversikt.py     översikt för lärare/förälder
matte/arbetsblad.py   arbetsblad med facit
matte/omraden.py      verktyg för klickytor på målningar
matte/kontroll.py     matte check
matte/ikoner.py       ikonerna
matte/kort.py         kort och läroplan (TOML)
matte/uppgifter.py    uppgifter ur mallar, bedömning med feltankar
matte/framsteg.py     Lär/Övar/Behärskar, repetition, ~/.local/share/se.wenuyos.app.matte
matte/rita.py         figurer, SVG- och cairo-duk
matte/regelbok.py     regelboken som HTML
matte/fonster.py      fönstret (Mattes vyer)
matte/skal.py         WenuyOS fönstergrund via libwenuy (reserv: libadwaita)
matte/tema.py         färger (WenuyOS + färgpennorna)
data/kort/            regelkorten
data/laroplan/        Lpfö 18, Lgr22 åk 1–3/4–6/7–9, Gy25 nivå 1–2 (47 mål, 45 med kort)
data/ikoner/          100 lerikoner
data/bilder/          Daniels målningar (scener)
docs/                 bildbeställning
pack.sh               .deb-paket
tests/                enhetstester
```

## Etapper

0.1 motorn + regelkort + tre kort · 0.2 hela Lgr22: 37 kort över alla sex
områden i tre stadier, algebra med variabler, nya figurer och svarstyper,
placeringstest, regler som slutar gälla, `matte check`, .deb ·
0.3 klickbara målningar, lekläge med uppläsning och förskolekort, anpassning efter
feltankar, Gy25 nivå 1–2, flera elever, översikt, arbetsblad, firande när ett kort sitter ·
**0.4: fler uppgifter och tydligare förklaringar – 62 kort, ~600 uppgiftsmallar (8–12 per kort),
~120 lösta exempel, exempel med lucka, ”så här kan man tänka” vid fel, längre förklaringar på
gymnasiekorten, nya kort Dubbelt och hälften (åk 1–3) och Algoritmer (åk 7–9) – alla 47 mål har kort (här)** ·
0.5 Matematik – fortsättning (derivata, integraler) och fler målningar · 0.6 specialisering och spets.
