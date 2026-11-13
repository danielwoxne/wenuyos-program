# Matte 1.3 – granskningsunderlag

Detta dokument gäller **utkast-PR #10** och ersätter inte Daniels godkännande.
All källkod ligger i GitHub. Byggresultat/PNG-skärmbilder ska enbart ligga som
GitHub Actions-artefakter eller i ett av Daniel godkänt släpp.

## Teststegen

1. **Debian 13 + 2 GB:** Standardflödet `Program` kör `wenuyos verify`,
   bygger C++17/GTK4, kör CTest och provar fönsterstart under låg minnesprofil.
2. **Separat mjukvarurendering:** `Matte software renderer screenshots` kör
   `WENUYOS_PERF=low`, `WENUYOS_GFX=soft`, `GSK_RENDERER=cairo`,
   `LIBGL_ALWAYS_SOFTWARE=1` med Xvfb vid 1024×600 och 1600×900. Den sparar
   fyra PNG: hem, lättläst övning, kurskarta och bråk/procent. Detta är
   skärmbildsprov, **inte** ett fullständigt 2 GB-långtidsprov i mjukvaruläge.
3. **Elevprofiler:** CTest innehåller ett reproducerbart prov som läser och
   sparar om en profil med Matte 1.2-nycklar. Varken verkliga elevdata eller
   användarens hemkatalog används i provet.
4. **Manuell genomgång:** Kontrollera att allt får plats på 1024×600,
   tangentbordsfokus, skärmläsning, uppläsningsmarkeringar och elevens
   befintliga logg/ritningar efter uppgradering.

## Ursprung och licenser – kräver beslut före släpp

| Resurs | Nuvarande uppgift | Krävd granskning |
|---|---|---|
| `src/` och `innehall/` | Matte README anger GPL-3.0-or-later, Daniel Woxnerud | Bekräfta att samtliga filer får distribueras under angiven licens |
| `common/` | WenuyOS/libwenuy-källkod, kopierad från äldre utvecklingsunderlag | Jämför mot senaste WenuyOS `tools/common/`; verifiera Apache-2.0-notiser och ev. tredjepartsdelar |
| `innehall/data/bilder/*.jpg` | 22 bildscener i originalprojektet | Säkerställ upphov, samtycke/överföringsvillkor och dokumenterad licens för varje bild |
| `innehall/data/ikoner/*.png` | Saknade ikoner genereras av repoets Python-generator | Kontrollera begriplighet och kontrast; ersätt viktiga former med semantisk grafik om nödvändigt |
| `data/se.wenuyos.app.matte.svg` | Programikon från originalprojektet | Bekräfta upphov och licens |

**Tills ovanstående är verifierat ska PR #10 förbli ett utkast och ingen
signerad eller osignerad release göras.** En grön CI bekräftar inte
automatiskt rättigheterna till bilder.

## Teknisk rättelse av källkodshygien (13 nov)

- `generera-saknade-ikoner.py` skriver enbart till byggmappen och `expandera.py` väljer verkliga originalikoner före reserven.
- `MATTE_IKONER_DIR` pekar Python-kontrollen på de färdiga byggikonerna; inga genererade PNG-filer läggs i `innehall/data/ikoner/`.
- `matte-icon-assets` är ett eget regressionsprov för originalprioritet, fallback, ombyggning och skrivskydd för källmappen.
- `verktyg/bygg-wapp.sh` använder nu `ctest` och allt källdata genereras via CMake under `build/`.
- Bildpaketet med de riktiga lerikonerna **saknas alltjämt**; bokstavsreserv är inte tillräcklig för barn som behöver bildstöd. Vänta med släpp.
- Dropbox innehåller `matte-cpp-1.4.0-kallkod.zip`, men zip-innehållet är ännu inte bitvis jämfört med det öppna GitHub-PR:et. Ändra inte versionsnummer eller påstå 1.4-paritet utan kontroll.
- Kontrollera ny grön Program-kontroll och skärmbildsprov på **senaste commit**, inte en äldre commit. Testresultat fylls i PR.

## Öppna kontroller

- Visuell granskning av alla fyra PNG-skärmbilder.
- Uppgraderingstest med *en säkerhetskopia* av verklig 1.2-profil,
  först på separat testanvändare.
- Fysisk ThinkPad på 2 GB/utan GPU: flöden, scroll, fokus och minnesmätning.
- Granskning av Claude i samma PR. Korrigeringar görs med commits där.
