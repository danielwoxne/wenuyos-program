# Wenuy Schack

Ett schackspel för WenuyOS, byggt i C++17. Offlinespel är standard; nätverksspel aktiveras bara genom användarens uttryckliga val. Spela mot datorn på fem nivåer eller mot en vän, använd 31 interaktiva övningar i utvecklingsversion 0.5.0 och få dragförslag.

0.5.0 omfattar även flerstegsövningar med två alternativa försvar, stegspecifika svenska/engelska ledtrådar och en längre femhalvdragsövning. Motståndarens övningsdrag är förutbestämda, inte AI-genererade. 0.5.0 introducerar fokusläge (Ctrl+L), rullbar sidopanel och ett högkontrasttema. Spelregler och sparfiler är oförändrade.

## 0.5.1 – publicerad
De två kvarvarande GUI-observationerna från 0.5.0 åtgärdades: sidopanelens vågräta rullist och en tangentbordshjälp som kapades på små skärmar. Se [provplan 0.5.1](docs/PROV-051.md).

## 0.5.2 – föreslagen rättelse för skakande bräde
Rapporterat fel: hela schackbrädet kan flytta sig när statusraden ändras efter ett drag eller när en pjäs markeras. En enradsstatus med ellips föreslås så brädets höjd inte varierar. Hela texten finns fortfarande tillgänglig. Rättelsen behöver provas visuellt före släpp.

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

Det grafiska programmet byggs från `src/RuntimeGtk.cpp` (0.4.0-klienten). Det använder GTK4/Cairo och sparar lokalt. Endast när användaren klickar på Bjud in eller Anslut används det lokala nätverket (okrypterat, enbart betrodda LAN).

## Installera
Först efter att Daniel godkänt PR och en ny utgåva publicerats:

```sh
wenuyos install github.com/danielwoxne/wenuyos-program#schack
```

Spelet har `se.wenuyos.app.schack.desktop` med `Categories=Game;BoardGame;`, svensk/engelsk metadata och en SVG-programikon.

## Status
**0.5.1 är publicerad. Version 0.5.2 förbereds i utkast-PR #29** och får inte släppas förrän Daniel godkänt en ny signerad version. Kontrollera `Program`-CI och `wenuyos verify` för varje revision och prova därefter på verklig WenuyOS-hårdvara med mjukvarurendering. Inga byggda `.wapp`-filer hör hemma i Git.
