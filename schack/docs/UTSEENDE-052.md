# Wenuy Schack 0.5.2 – visuell målbild

Målbilden är den av Daniel valda produktillustrationen från samtalet den 13 november 2026. Illustrationen är en **designreferens**, inte en skärmbild av byggd programvara. Den är inte en exakt FEN-beskrivning och får inte ersätta regelmotorns spelställningar.

## Mappning från målbild till GTK4-programmet

| Illustration | 0.5.2 (detta PR) |
| --- | --- |
| Tydlig titelrad med primärknapp | Titeln, Nytt parti (primär), Ångra och Vänd brädet; valfria symboliska ikoner från valt GTK-tema |
| Rena verktyg över brädet | Schackskolan, dragförslag, draganalys och fokusläge i en separat verktygsrad |
| Stort schackbräde till vänster | Befintlig Cairo-vektorgrafik, förbättrade pjäser, temastyrda dragmarkeringar och enradig status under brädet |
| Kortet Ditt parti | Partistatus, vems tur det är och materialbalans |
| Kortet Schackskolan med framsteg | Alltid synlig sammanfattning med riktig procentandel av avklarade övningar; Starta-knapp när träning är av; fördjupad övningsvy när den är på |
| Kortet Spelinställningar | Spellägen, nivåer, spelarens färg, bondeförvandling och brädtema |
| Kortet Draglista | Ordinarie spelnotation med vertikal rullning och PGN-export |
| Kortet Nätverksparti | Befintlig frivillig LAN-anslutning och säkerhetsvarning, sist i sidopanelen |

Ikonerna hämtas enbart ur valt tema. Om en symbolisk ikon saknas visas fortfarande läsbar knapptext. De GTK/Adwaita-styrda korten följer tema och mörkt/ljust läge i stället för påhittade fasta färger. När fönstret är för smalt för sidopanel vid sidan av brädet används den tidigare stående responsiva layouten och vertikal rullning. Mockupens skrivbord, wallpaper och docka hör till operativsystemet och ingår inte i spelprogrammet.

## Tekniska och pedagogiska gränser

- **2 GB RAM och mjukvarurendering** är golvet. Inget nya grafikbibliotek, ny process, nätverksanrop, stora bildresurser eller utökade AI-sökningar.
- Schackregler, FEN/PGN och LAN-kontroller ändras inte. Skolans procentandel bygger på `lessonMask` och `lessonCount()`, inte på ett påhittat exempel som i bilden.
- Under vanliga partier visas endast skolans rubrik, sparade framsteg, introduktion och startknapp. Övningsknappar, analys och ledtrådar visas först när Schackskolan verkligen är aktiv.
- Pjäser, drag och status uppdateras fortfarande i huvudprogrammets befintliga GTK4/Cairo-vy. Inga extra renderingstrådar används.

## Acceptansprov inför granskning och släpp

1. Ljust/mörkt systemtema: alla kort, ikoner och knappar följer tema; saknad ikon blir text, inte ett trasigt bildtecken.
2. Svenska/engelska: titel, skolans start-/avslutsknapp, framsteg och alla övriga kontroller är tydliga.
3. Spela e2–e4, låt datorn svara, välj en pjäs och ångra. Brädets kant står stilla; inga popup- eller etikettstorleksryck.
4. Schackskolan är synlig även i vanligt parti med verkligt antal avklarade övningar. Start öppnar träning och visar uppgiftsknappar. Avsluta återgår till förhandsvisning. Tidigare avklarade övningar och sparfiler påverkas inte.
5. Prov i 1024×600, 800×600 och stående fönster: inga överlapp, verktygen går att nå, sidan går att rulla och tangentbordet fungerar.
6. `WENUYOS_PERF=low WENUYOS_GFX=soft`: `wenuyos verify`, korrekt uppstart och inga nya minnes-/CPU- eller grafikberoenden. Hårdvaruprov och Orca dokumenteras separat.

Detta är ett **utkast**. En annan AI granskar med grönt PR-prov innan sammanslagning; Daniel godkänner varje släpp.
