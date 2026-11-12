# Musikstudio 0.9.0 – föreslagna versionsnyheter (inte släppt)

Version 0.8.1 finns redan som signerad `.wapp`; detta dokument gäller **utvecklings-PR #18** och innebär inget godkännande att släppa 0.9.0.

## Ändringar i etapp 1

- Gemensam pianogeometri för GTK och huvudlösa prov.
- Sätt ut toner direkt på rätt tid/tonhöjd med vänsterklick och 1/16-delsanpassning.
- Klick på befintlig ton väljer den; synlig markering och tangentbordsfokus i pianorullen.
- Högerklick eller Delete/Backspace tar bort markerad ton, pilar flyttar vald ton.
- Ångra och gör om fungerar genom befintlig minnesbegränsad historik.
- Musgest för att flytta uppspelningsmarkören ritar nu bara om tidslinjerna i stället för att återskapa alla mixerreglage – viktigt på 2 GB.
- Svenska och engelska instruktioner under pianorullen.

## Föreslagna prov före merge

- C++17-regressionsprov av pixlar→ton, randvärden, dubblettskydd, markerad ton, förflyttning, ångra/borttagning.
- Program-CI: befintliga CTest, `wenuyos verify` GTK4 på 2 GB/soft och WAPP-paketering.
- Manuell GTK4-interaktion på 1024×600 och 1280×720, mus/högerklick/tangentbord.
- PortAudio med riktig högtalare/mikrofon och längre 8-spårs-pass återstår; ingår inte i detta första steg.

**Release, signatur och Programbutik hanteras endast efter Daniels godkännande.**
