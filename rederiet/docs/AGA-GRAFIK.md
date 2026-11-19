# AGA-inspirerad grafik för Rederiet (test 0.5.1)

Originalstil inspirerad av 1990-talets 32-bitars Amiga/AGA-spel utan att kopiera grafiska resurser från befintliga spel.

- Behåll nuvarande högupplösta världskarta som förvalt läge.
- Lägg till växlingsbart **AGA-inspirerat (256 färger)** i kartverktygsraden; svenska och engelska.
- 960×540 indexerad 256-färgers PNG (färgerna har 24-bitars RGB-värden); cirka 376 kB; nearest-neighbor-skalning. Inga dyra shaders, ingen nätanslutning.
- Pixelbaserad fartygssiluett och tydlig retroestetik med temats accent/förgrundsfärger för interaktiva kontroller. Klickbara hamnar, fartyg, rutter och sparfil opåverkade.
- Karta och fartyg förblir två separata lager så att de fungerar i samtliga fyra utsnitt.
- Äldre kartläge fungerar som reserv. Ingen automatisk omvandling av sparfiler eller tvång på retrostilen.

**Lokalt tillgänglig testkandidat:** `rederiet-0.5.1-aga-test-src.zip` och osignerad `.wapp` skapade i ChatGPT. Nya filer `map_widget.py`, `main.py`, `tests/test_aga_graphics.py`, `share/world-map-aga.png`, README och manifest. Alla **58/58** Python-testfall och syntaxkontroll godkända. Separat GTK4/labwc-start, verklig low/soft, CI och installationsprov inte utförda. Källkoden/bildresursen från testkandidaten måste synkas till PR innan granskning. Inget släpp utan Daniel.
