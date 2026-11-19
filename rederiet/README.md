# Rederiet 0.3.0 – bygg din egen fraktflotta

**Rederiet** är ett självständigt, turbaserat rederispel för WenuyOS (Debian 13, GTK 4), med känslan från klassiska sjöfarts- och handelsspel. Ingen originalkod, grafik, text eller musik från *Ports of Call* ingår.

## Nytt i 0.3.0

- **Flotta med upp till åtta fartyg** som kan segla samtidigt och väljas i listan eller direkt på kartan. Alla fartyg har egen rutt, bränsle, skick, position och last.
- **Fyra fartygstyper** med olika pris, hastighet, lastkapacitet, bränsletank och besättningskostnad: kustfraktare, lastfartyg, oceanfraktare och containerfartyg.
- **Köp och sälj fartyg** i hamn. Du kan inte sälja ett fartyg med last eller till sjöss, och du måste behålla minst ett fartyg.
- **Sex frakterbjudanden per hamn och dag**, där större fartyg kan ta flera frakter till samma destination på en resa. Leveranser ger pengar och bättre rykte; sena leveranser ger halverad betalning och sämre rykte.
- **Förbättrad ekonomi:** olika bränslepriser per hamn, besättningskostnad per fartyg, intäkter, utgifter, rederivärde och säljarvoden. Du kan nu tanka och reparera **delvis** om du inte har råd med fullt arbete.
- **Världskartan** visar flera samtidiga rutter med särskilt tydlig markering för valt fartyg. Fartyg i samma hamn visas med små förskjutna markörer så de är möjliga att välja.
- **Säker nystart:** två tryck krävs för att ersätta ett sparat rederi.
- **Sparfilformat v3:** sparar alla fartyg och frakter, stöder automatiskt 0.1/0.2-sparfiler utan att kräva omstart, och avvisar ogiltiga/orimligt stora sparfiler.

## Spela

1. Starta som kapten på **Nordstjärnan** i Göteborg med 22 000 kr. Välj en frakt som ryms ombord.
2. Klicka på önskad hamn på världskartan eller välj ur destinationlistan. Klicka **Segla**.
3. Klicka **Nästa dag** för att förflytta **alla** fartyg samtidigt. För levererad last kommer betalningen automatiskt.
4. **Tanka**, **reparera** eller använd **varvet** när valt fartyg ligger i hamn.
5. När du har råd: köp ett till fartyg, välj vilket du styr i flottlistan och planera en annan resa samtidigt.

Det går att segla utan last, men då kostar resan bränsle och besättningslöner utan fraktintäkter.

## Köra och prova

På WenuyOS/Debian 13 med Python 3.13, `python3-gi` och `gir1.2-gtk-4.0`:

```sh
cd rederiet
python3 main.py
python3 -m unittest discover -s tests -v
```

- **Offline**: inga nätverksanrop, inga externa API:er och ingen telemetry.
- **2 GB-målprofil**: statiska ruttpunkter och Cairo/GTK-vektorgrafik; inga karttexturer, stora bildfiler, OpenGL eller kontinuerliga timerloopar. `WENUYOS_PERF=low` och `WENUYOS_GFX=soft` kräver inget separat grafikspår.
- **Svenska/engelska**: språkval enligt WenuyOS locale (`T(en, sv)` i Python); färger och fartygsmarkörer följer GTK-temat.
- **Sparfil**: `GLib.get_user_data_dir()/se.wenuyos.app.rederiet/save.json`, utanför programinstallationen.
- **Kartan** är en handritad schematisk spelkarta. Rutter, bränslepriser, fartyg och ekonomi är fiktiva och ska aldrig användas för navigering eller verkliga affärsbeslut.

## Teststatus för 0.3.0

- Python-logik, gamla sparfiler, ekonomi och flera fartyg testas via `tests/test_engine.py`, `tests/test_world.py`, `tests/test_fleet.py`.
- `tests/test_map_widget.py` ritar **riktiga Cairo-anrop** men använder en **GTK-stubb**. Det är **inte** ett prov av ett verkligt GTK-fönster.
- `python3 -m compileall` kontrollerar syntax; lokalt test körs även med `WENUYOS_PERF=low WENUYOS_GFX=soft`.
- Återstår: `wenuyos verify`, UI-interaktion i GTK4/labwc, Debian 13-installation, faktisk minnesmätning på 2 GB-dator, signerad `.wapp`, oberoende granskning och godkännande före släpp.

Utveckling sker i `danielwoxne/wenuyos-program`, mappen `rederiet/`, utkast-PR #33. Ingen ändring i WenuyOS main eller programsläpp utan godkänd process.
