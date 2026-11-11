# Status: Musikstudio 0.8 – integrering
Den här PR-grenen innehåller den verifierbara 0.4.1-basen från Dropbox samt ny 0.8-redigeringskärna: minnesbegränsad ångra/gör om, kvantisering, transponering, flytt och duplicering av ljudklipp.
**Viktig skillnad:** Det fristående 0.7/XWayland-paketet i ChatGPT är inte identiskt med 0.4.1/GTK4-grenen. 0.5–0.7-funktionerna måste sammanföras och testas innan versionsnumret kan höjas till 0.8 i GitHub.
Test i CI: CMake/CTest automatiskt. Riktigt WenuyOS, mikrofon, 2 GB och GTK4 kräver senare verifiering. Den här PR:n är inte ett godkänt släpp.
