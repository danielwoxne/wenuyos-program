# Wenuy Schack

Ett offline-schackspel för WenuyOS, byggt i C++17. Spela mot datorn på fem nivåer eller mot en vän, använd 20 interaktiva lektioner och få dragförslag.

0.5.0 introducerar fokusläge (Ctrl+L), rullbar sidopanel och ett högkontrasttema. Spelregler och sparfiler är oförändrade.

## Bygg och testa
I WenuyOS SDK / Debian 13 med GTK4:s utvecklingspaket:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
wenuyos verify
```

Det grafiska programmet byggs från `src/RuntimeGtk.cpp` (0.4.0-klienten). Det använder GTK4/Cairo och sparar lokalt; det gör inga nätverksanrop.

## Installera
Först efter att Daniel godkänt PR och en ny utgåva publicerats:

```sh
wenuyos install github.com/danielwoxne/wenuyos-program#schack
```

Spelet har `se.wenuyos.app.schack.desktop` med `Categories=Game;BoardGame;`, svensk/engelsk metadata och en SVG-programikon.

## Status
**0.5.0 utvecklas i separat PR.** Den publicerade 0.3.0-versionen fortsätter att gälla tills Daniel godkänner den nya. Kontrollera `Program`-CI och `wenuyos verify` för varje revision och prova därefter på verklig WenuyOS-hårdvara med mjukvarurendering. Inga byggda `.wapp`-filer hör hemma i Git.
