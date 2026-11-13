# Räknepapper och ritplatta i Matte 1.3 – utvecklingsgren PR #10

## För eleven

Öppna **Räknepapper** i en uppgift. Välj **Skriv** (tangentbord),
**Penna** (Wacom, mus eller pekskärm) eller **Sudd**. Med **Ställ upp**
kan du skriva uppställningen från en enkel räkneuppgift. **Ångra pennstreck**
tar bort det senaste pennstrecket. **Rensa** frågar om bekräftelse innan
pappret suddas.

Efter ett avslutat pennstreck, tangentändring eller radering sparas
uträkningen automatiskt **på den här datorn**, tillsammans med elevens
pågående uppgift. Texten **Sparat** visas efter lyckad skrivning.
Om skivan är full visas **Kunde inte spara** och arbetet finns kvar på
skärmen. Programmet ska inte byta uppgift eller stänga medan den
aktuella uträkningen inte går att spara.

Nästa gång samma pågående lektion öppnas får eleven tillbaka samma
uppgiftsvariant, ruttext, minnessiffror, summastreck och handskrivna
pennstreck. Vid en uttrycklig ny uppgift får nästa försök ett nytt
lagrings-ID och skriver inte över den tidigare filen.

## Teknik

- GTK4 `GtkGestureStylus` med `GDK_AXIS_PRESSURE` där drivrutinen stöder
  tryckdata; annars konstant streckbredd. Mus/pek fungerar också.
- `Papper.cpp`: samma rutkoordinater oavsett zoom, separata stylus-
  och musgester. Sparning sker efter avslutad inmatning, **inte under
  varje enskild pennrörelse**.
- `PapperLagring.h/.cpp`: GTK-fri JSON-modell med formatet
  `se.wenuyos.matte.raknepapper`, version 1. Filerna ligger i
  `datamapp()/raknepapper/<elev>/<uuid>.json`. Inga fjärranrop.
- Elevprofilen lagrar endast **pekaren till pågående försök**, inklusive
  kort-ID, mall-/variantindex och frågetext. Detta är ett valfritt fält;
  äldre elevprofiler fortsätter att läsas.
- Skrivning sker med GLibs säkra filersättning; filen är privat
  (`0600`). Felaktig JSON, farligt filnamn, överstora eller icke-finit
  pennpunkter stoppas utan att befintligt fungerande papper ersätts.
- Räknepappret har ett tak på **16 000 punkter under normal inmatning**,
  maximalt 250 handskrivna streck och maximalt 2 MiB per sparad fil.
  Vid punktgränsen visas en varning i stället för att innehåll tyst
  försvinner. Formatläsaren har en separat hård gräns på 50 000 punkter.
- `Flytt.cpp`: elevens export innehåller även valfria räknepapper.
  Gamla exportfiler utan sådana data importeras oförändrat. Import
  validerar varje filnamn och JSON **innan profilen skrivs**.

## Prov som krävs före ett släpp

- C++-regression: JSON-rundgång, penntryck, rutor och linjer,
  sparning/återläsning, separata elever, korrupt JSON,
  filstorleksgränser och path traversal vid import/export.
- GTK4 i Debian 13: skärmbildsprovet startar programmet två gånger
  under `WENUYOS_PERF=low` och `WENUYOS_GFX=soft`.
  Första processen skriver syntetiskt räknepapper, den andra ska
  återställa samma pågående uppgift och rutor.
- Manuell WenuyOS/ThinkPad och fysisk Wacom Intuos: prova
  tryckkänslighet, mus, touch, ångra, sudd, zoom, diskfullt,
  elevbyte, omstart och export/import på 2 GB.

## Avgränsningar

Detta är **inte** handskriftsigenkänning och ändrar inte rättningen.
Bilder/anteckningar skickas aldrig till nätet. Tidigare
avslutade försök lagras i var sin fil och kan exporteras, men någon
fullständig elevöversikt där man bläddrar bland **alla gamla**
räknepapper finns ännu inte. Den behövs i en följande UI-etapp.

Implementationen ligger i öppna utkast-PR #10. Ingen sammanslagning
eller release utan granskning och Daniels godkännande av släppet.
