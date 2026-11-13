# Ett nytt ämne på samma grund (wenuy-lara)

Matte är byggt i två lager. Det mesta är inte matematik och kan användas för fler ämnen –
Svenska är det naturliga nästa, eftersom uppläsningen, ordlistan och lättläst gör mest nytta där.

## Vad som är gemensamt och vad som är matte

| Del | Fil | Gemensamt (wenuy-lara) | Matematik |
|---|---|---|---|
| Programmets id, datamapp | `Lara` | ✓ `laraInit("se.wenuyos.app.svenska", "SVENSKA_DATA")` | |
| Texthjälp | `Text` | ✓ | |
| JSON | `Json` | ✓ | |
| Kort, läroplan, uppgifter, ordlista | `Data` | ✓ samma format | |
| Framsteg, repetition 1–120 d, anpassning, placering | `Framsteg` | ✓ | |
| Händelseloggen, lärarvyn | `Logg` (+ `visaElevvy`) | ✓ | |
| Uppläsning med markering | `Uppl` | ✓ | `somOrd` gör om matematiska tecken till ord – ett nytt ämne kan låta bli |
| Svarstyperna kryssa, para, måla, rita, välja | `Bedom` (nyNyckel), `SvarNy` | ✓ | |
| Tal, bråk, procent, uttryck | `Tal`, `Algebra` | | ✓ |
| Rättning av skrivna svar | `Bedom` (svarsnyckel) | ord jämförs redan som text (`o:`) med stavfelstolerans | tal (`t:`) och uttryck (`p:`) |
| Figurerna | `Rita` | sak, bild, malrutor, rutpapper, tabell, scen | staplar, tallinje, koordinatsystem, bråk … |
| Fönstret | `Fonster` | nästan allt | korttexterna "Man säger / Man skriver" |
| Utskrift (PDF) | `Utskrift` | ✓ | |
| Författarverktygen | `innehall/matte/*.py` | kort.py, kontroll.py, expandera.py | uppgifter.py (taltolkning), rita.py (figurerna) |

`libwenuy-lara.a` byggs för sig (se CMakeLists.txt) och har inga beroenden till matematiken –
kontrollerat med `nm`: alla odefinierade symboler finns i biblioteket självt.

## Så här skulle Svenska byggas

1. **Ny mapp** bredvid `matte-cpp`, t.ex. `svenska/`, med samma struktur: `innehall/`, `src/`, `verktyg/`.
2. **Länka mot wenuy-lara** och anropa `laraInit("se.wenuyos.app.svenska", "SVENSKA_DATA")` först i `main`.
   Elevdata hamnar då i `~/.local/share/se.wenuyos.app.svenska/`, helt skilt från Matte.
3. **Korten** skrivs i samma TOML-format: rader (*man säger – bild – man skriver*), lösta exempel,
   gäller/slutar gälla, uppgiftsmallar. Exempel på kort: *Vokaler och konsonanter*, *Stor bokstav och punkt*,
   *Substantiv*, *Verb i dåtid*, *Sj-ljudet*, *Läsförståelse – vem, var, när*.
4. **Svarstyperna** som redan fungerar utan ändring: `val`, `kryssa` (vilka ord är verb?), `para` (ord och bild,
   singular och plural), `mala` (måla alla vokaler), `rita`, `text` (ett ord – stavfel kan räknas eller förlåtas
   per uppgift).
5. **Det som behöver skrivas nytt:**
   - rättning för meningar (jämför ord för ord, visa vilket ord som är fel),
   - figurer för ord och bokstäver (bokstavsrutor, stavelsebågar, ordbilder),
   - en egen ordlista (`ordlista.toml`) med grammatikorden: substantiv, verb, adjektiv, mening …,
   - läroplanen: Lgr22 Svenska åk 1–3, 4–6, 7–9.
6. **Fönstret**: kopiera `Fonster.cpp` och byt namn och texter. Pass, paus, Dagens tre, lättläst, ordlistan,
   lärarvyn, utskriften och exporten fungerar som de är.

Ett rimligt första steg är *Svenska åk 1–3* med 8–10 kort, eftersom läsinlärningen är där stödet för
dyslexi gör mest nytta.
