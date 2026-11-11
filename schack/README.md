# Wenuy Schack

Ett offline-schackspel för WenuyOS, byggt i C++17. Spela mot datorn på fem nivåer eller mot en vän, använd 15 lektioner och få dragförslag.

## Bygg och testa
I WenuyOS SDK / Debian 13 med GTK4:s utvecklingspaket:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
wenuyos verify
```

Det grafiska programmet byggs från `src/RuntimeGtk.cpp` (0.3.0-klienten). Det använder GTK4/Cairo och sparar lokalt; det gör inga nätverksanrop.

## Installera
När Daniel godkänt PR och den signerade utgåvan har släppts:

```sh
wenuyos install github.com/danielwoxne/wenuyos-program#schack
```

Spelet har `se.wenuyos.app.schack.desktop` med `Categories=Game;BoardGame;`, svensk/engelsk metadata och en SVG-programikon.

## Status
Detta är import av **0.3.0** till GitHub, inte ett godkänt butikssläpp. Schackmotorns tester ska köras i CI och WenuyOS-fönstret måste verifieras i 2 GB/soft-profilen innan Daniel godkänner en tagg. Inga byggda `.wapp`-filer hör hemma i Git.
