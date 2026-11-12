# Ritplatta i Matte 1.3 (PR #10)

## För eleven
Välj **Penna** i panelen **Räkna här** och skriv uträkningen eller svaret på
det rutade papperet med ritplatta, pekskärm eller mus. När pennan sätts mot
Wacom-ritplattan växlar programmet automatiskt från **Skriv** till **Penna**.
Välj **Sudd** för att ta bort streck eller **Ångra pennstreck** för att ta bort
det senast ritade strecket. Tangentbordet och **Ställ upp** fungerar som förut.

## Teknik
- GTK4 `GtkGestureStylus` för pennans ned-/rörelse-/upp-händelser.
- `GDK_AXIS_PRESSURE` används där den finns; annars konstant tjocklek.
- Ritpunkterna använder samma koordinater även efter zoomning.
- Dubbelinmatning undviks genom att stylushändelser inte samtidigt
  registreras som vanliga `GtkGestureDrag`-rörelser.
- Mus/pekning fungerar som tidigare. Ingen ny drivrutin, AI-modell eller
  nätverkskoppling i programmet.
- Högst 10 000 punkter per pennstreck för att begränsa minnesförbrukning.
- Ritplattans egen systemdrivrutin/libinput och stöd i fönsterhanteraren
  krävs för tryckdata; en ritplatta kan fungera som mus utan tryckdata.

## Kända begränsningar och testplan
- Pennans tryck/bredd och fallback provas i `matte-test`.
- GitHub provar C++-bygge, CTest och GTK-start på 2 GB-profil.
- **En fysisk Wacom Intuos och dess tryckvärden har inte provats.**
  Testa penna, sudd, ångra, zoom och växling till tangentbord/mus på
  WenuyOS, inklusive `WENUYOS_GFX=soft`.
- Handskrivna anteckningar finns i räknepappret under den aktuella
  uppgiften men **sparas ännu inte automatiskt mellan uppgifter eller
  omstarter**. Denna etapp omfattar endast fungerande penninmatning.
- Programmet **tolkar inte handskrivna siffror automatiskt** och rättar
  inte pennstrecken; eleven svarar via uppgiftens vanliga svarskontroll.

Granska i samma utkast-PR #10; ingen merge eller release utan Daniels
separata godkännande.
