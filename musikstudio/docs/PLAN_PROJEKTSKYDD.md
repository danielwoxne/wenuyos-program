# Musikstudio 0.9 – steg 2: skydd för osparad musik

Kopplat till [ärende #17](https://github.com/danielwoxne/wenuyos-program/issues/17) och bygger på `main` efter godkänd/sammanslagen pianorulle PR #18. Endast `musikstudio/` ändras.

## Användarfall
- Nytt projekt, öppning via meny, dubbelklick från Filer/CLI, och fönsterstängning visar vid osparat arbete en modal fråga: **Avbryt / Kasta ändringar / Spara**.
- Avbryt lämnar projektet orört. Kasta ändringar utför valet. Spara utför valet först efter lyckat sparande (inklusive namnlösa projekt som kräver filväljare).
- Avbrutet eller misslyckat sparande innebär att det osparade projektet finns kvar, utan att växla projekt eller avsluta programmet.
- Befintliga projekt ska inte skrivas över tyst via `Spara som`; att spara till nuvarande fil ska behålla befintlig backupsfunktion.
- Under mikrofoninspelning måste användaren först avsluta inspelningen; stängning får inte tappa pågående ljud.
- Även ett trasigt projekt som öppnas ska lämna befintlig musik kvar. Filer öppnas först till temporärt projekt, sedan ersätts det aktiva.
- Samma flöde och användartexter på svenska och engelska.

## Prov
- Enhetsprov av beslutsgång, avbrutna och felande sparningar.
- Regression av projektladdning som inte ersätter minnet förrän filen godkänts.
- `wenuyos verify` i Debian 13, `WENUYOS_PERF=low` och `WENUYOS_GFX=soft`, övriga CTest.
- Manuell provning av Adwaita-dialoger vid 1024×600 och fysiskt ljud på WenuyOS redovisas separat.

Ingen .wapp i Git, ingen merge/release utan Daniel.
