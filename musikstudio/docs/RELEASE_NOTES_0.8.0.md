# Wenuy Studio 0.8.0 – osignerad förhandsversion

## Nya funktioner jämfört med det fristående 0.7-bygget

- Ctrl+Z / Ctrl+Y och två synliga knappar för att ångra och göra om ändringar i sessionen.
- C++-modul EditHistory med begränsning av historikens minnesanvändning (12 MiB, högst 32 steg som standard).
- Testbara icke-förstörande redigeringsfunktioner för kvantisering, transponering, flytt och kopiering av klipp.
- Språkväxling mellan engelska och svenska för huvudkontrollerna. Alla dynamiska feltexter är ännu inte lokaliserade.
- GTK4-grenen i WenuyOS-programförrådet har importerats och fått EditModel-koden, men 0.7/XWayland-GUI:s samtliga funktioner har ännu inte förenats med den.

## Hårdvaru- och plattformsbegränsningar

- Det bifogade programmet använder X11/XWayland och inte ett färdigt GTK4-fönster.
- Windows har inte fått lokala filer direkt, eftersom ChatGPT saknar åtkomst till Windowsprojektmappen i denna session.
- Mikrofon, högtalare och verklig latens är inte provade. Att CLI kan exportera WAV ersätter inte ett ljudtest.
- Ångra/gör om har provats i C++-kärnan; knappfunktionerna har kompilerats och fönstret startats, men ännu inte provats manuellt med mus på WenuyOS.
- Projektformat är fortsatt version 5; äldre .wstudio v2–v4 läses. Behåll säkerhetskopior av äldre projekt.

## Publicering

Varken GitHub Release eller katalogpublicering är genomförda. Paketet är osignerat och är endast en förhandsversion. Endast Daniel kan godkänna PR/merge/släpp.
