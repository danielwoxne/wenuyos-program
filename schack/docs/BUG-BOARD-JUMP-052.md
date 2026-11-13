# Wenuy Schack 0.5.2 – brädet hoppar när en pjäs flyttas

**Rapporterat av Daniel, 13 november 2026:** Vid markering/flytt av en schackpjäs rör sig eller skakar hela spelbrädet.

## Trolig orsak
`refresh()` ändrar varje gång `boardSummary`-etikettens text (status, urval och drag). Etiketten kan slå om mellan en och flera rader och får därmed olika höjd. Den lodrätt expanderande GTK-ritningsytan delar samma `GtkBox`; varje höjdändring flyttar eller krymper brädet och dess centrerade geometri.

## Prov att utföra vid granskning
Vid 1024×600: starta parti, markera vit bonde, flytta e2–e4, invänta AI-drag, ångra, växla fokusläge och brädtema. Brädets ytterram får inte ändra koordinater vid statusbyte. Upprepa med `WENUYOS_PERF=low WENUYOS_GFX=soft` och systemets rörelse avstängd. Kontrollera att hela texten går att läsa via skärmläsare och tooltip. Prova även en längre draglista och schackövningar.

## Plan
- Behåll statusinformation och skärmläsarstöd men ge brädets sammanfattningsrad konsekvent en rad med ellips vid behov; fullständiga texten ska fortfarande vara tillgänglig.
- Avgränsa rättelsen till `schack/`. Inga spelregel-, sparfil- eller nätverksändringar.
- Lägg strukturellt regressionsprov och kör `wenuyos verify` i Debian 13/low/soft eller dokumentera vilka moment som inte gick att prova.
- Be Claude att granska och kontrollera manuellt med drag vid 1024×600, skärmläsare och reducerad rörelse.

**Inget släpp före Daniels uttryckliga godkännande.**
