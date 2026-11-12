# Wenuy Schack

Ett offline-schackspel för WenuyOS, byggt i C++17. Spela mot datorn på fem nivåer eller mot en vän, använd 20 interaktiva lektioner och få dragförslag.

0.5.0 introducerar fokusläge (Ctrl+L), rullbar sidopanel och ett högkontrasttema. Spelregler och sparfiler är oförändrade.

## Spela mot en annan WenuyOS-dator (0.5.0-test)
I **Nätverksparti / LAN Game** väljer du uttryckligen *Bjud in* på en dator och *Anslut* på en annan. Ange värdens privata IPv4-adress och använd TCP-port 47777. Endast på **betrodda lokala nätverk**: trafiken är inte krypterad eller autentiserad, och ingen anslutning startar utan knapptryck. Läs [LAN-instruktionerna](docs/LAN-050.md). Två fysiska WenuyOS-datorer behöver fortfarande provas innan funktionen kan bedömas färdig.

## Språk och systemtema
Schack använder nu WenuyOS:s gemensamma `wenuy::i18nInit("se.wenuyos.app.schack")` och `wenuy::tr()`. Svenska och engelska finns inbyggda. För ytterligare språk behövs motsvarande översättningar för Schacks engelska texter i `se.wenuyos.app.schack.mo` enligt WenuyOS:s språkdomän; bara ett språkpaket för själva operativsystemet ger **inte** automatiskt översatta schacktexter.

På en WenuyOS-utvecklingsmaskin kan `wenuyos app translate de` användas för att skapa ett tyskt `.po`-underlag som sedan översätts och installeras enligt SDK:s rutiner. Saknas en översättning visas engelska. Vid nästa programstart väljs systemspråket. Dialoger och kontroller får WenuyOS gemensamma GTK-tema genom `wenuy::loadStyle()`, medan schackbrädets fyra särskilda paletter finns kvar. Schack använder också WenuyOS valda systemljud för drag och avslutade partier. Ljuden följer systemets ljudinställningar.

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
