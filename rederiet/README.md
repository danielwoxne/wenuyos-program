# Rederiet 0.2.0 – världskarta

Ett eget, litet turbaserat rederispel för WenuyOS. Inspirationen kommer från klassiska handelsspel för hemdatorer; inga originalbilder, namn på programdelar eller spelresurser från *Ports of Call* används.

## Nytt i 0.2.0

- **Världskarta** med schematiska kustlinjer, 15 hamnar på flera kontinenter och klickbara hamnmarkörer.
- **Världen**, **Europa**, **Atlanten** och **Asien–Stilla havet** som valbara kartutsnitt.
- **Sjövägar** genom bland annat Suezkanalen och Panamakanalen – renderade som streckade rutter.
- **Fartygsposition** längs rutten, som flyttas när en speldag går.
- Fraktuppdrag som blandar korta och långa resor. Anpassad bränsleförbrukning för oceanresor.
- Äldre sparfiler från 0.1.0 kan läsas.

Kartan är *schematisk* och får **inte** användas som navigationshjälp. Kustlinjerna är handritade, förenklade former – inte riktiga sjökort.

## Spela

1. Starta i Göteborg med ett fartyg och 22 000 kr.
2. Klicka på en hamn på kartan eller välj den från listan. Streckad linje visar den föreslagna rutten.
3. Acceptera ett fraktuppdrag och segla mot destinationshamnen.
4. Tryck **Nästa dag** för att se fartyget röra sig över världskartan.
5. Vid ankomst lossas rätt last automatiskt och du får betalt. Tanka/reparera i hamn och välj nästa uppdrag.
6. Spelet sparas under din användardatakatalog. **Nytt rederi** ersätter det pågående spelet.

## Kör och testa

På WenuyOS/Debian 13 med Python 3, GTK 4 och PyGObject:

```sh
cd rederiet
python3 main.py
python3 -m unittest discover -s tests -v
```

Resursprofil: ritning via GTK 4/Cairo med små vektorformer, ingen SDL/OpenGL/GPU, inga karttexturer, inga timerloopar, inget nätverk eller externa API-anrop. Tema- och accentfärg kommer från aktiv GTK-färgpalett. Spelmotorn använder en liten graf över sjövägar och beräknar allt offline. För målmaskiner med **2 GB RAM** och `WENUYOS_GFX=soft`, `WENUYOS_PERF=low`.

### Teststatus

Logik- och geometri-enhetstester körs utan GTK. GTK-fönster, WenuyOS `verify`, installationspaket och verklig ThinkPad/Chromebook måste fortfarande testas på målsystemet.
