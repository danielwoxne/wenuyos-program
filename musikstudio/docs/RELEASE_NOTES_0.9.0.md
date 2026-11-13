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


## Etapp 2: skydd för osparat arbete (PR #19)

- Nytt projekt, projektöppning från meny/Filer/kommandorad och fönsterstängning går genom samma skydd för osparad musik.
- Dialog med **Avbryt / Kasta ändringar / Spara**, på svenska och engelska.
- **Spara** genomför den begärda åtgärden först efter lyckat projektfilssparande; vid fel eller avbruten Spara som behålls musiken.
- Ett trasigt `.wstudio` läses/valideras innan det aktiva projektet byts; vid läsfel finns musiken kvar.
- Mikrofoninspelning måste avslutas innan projektet kan bytas eller programmet stängas.
- Nytt test `unsaved_project_guard_090` och en GTK-kontroll att alla ingångar använder skyddet.

Inget av detta ingår i signerade 0.8.1 förrän ny release godkänts och publicerats.

## Keyboard och MIDI-tagningar (stackad PR #27)

- Tvåoktavers temaanpassat virtuellt keyboard för C4–B5. Spel med mus eller datortangentbord på svenska/engelska.
- Kort förhandslyssning av valt spårs syntinstrument via redan befintlig PortAudio-motor.
- **Spela in MIDI / Stoppa MIDI-inspelning:** note-on/off, samtidiga tangenter, monotona tidsstämplar, uppmätt tonlängd, kvantiserad start, allting på valt spår. Ångra återställer hela tagningen.
- Blockering av sparning/stängning/projektbyte medan MIDI-tagningen pågår.
- Tak 4 096 händelser per tagning; inget nät eller ny audio-drivrutin. MIDI via fysisk USB-enhet och samplingsbank är framtida etapper, inte levererade här.
- Dokumenterad separat licens- och RAM-budget för framtida fria samplingar, inga ljudfiler tillagda.

## Föreslagna prov före merge

- C++17-regressionsprov av pixlar→ton, randvärden, dubblettskydd, markerad ton, förflyttning, ångra/borttagning.
- Program-CI: befintliga CTest, `wenuyos verify` GTK4 på 2 GB/soft och WAPP-paketering.
- Manuell GTK4-interaktion på 1024×600 och 1280×720, mus/högerklick/tangentbord.
- PortAudio med riktig högtalare/mikrofon och längre 8-spårs-pass återstår; ingår inte i detta första steg.

**Release, signatur och Programbutik hanteras endast efter Daniels godkännande.**
