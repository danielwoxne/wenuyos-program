# Versionshistorik

## 0.5.0 (under utveckling)
- Etapp 14: Två nya flerstegsövningar utökar kursen till 28. Spelaren planerar två egna utvecklingsdrag med ett förutbestämt lagligt svar från svart däremellan. Endast båda korrekta spelardragen ger slutförande och sparad progression. Skriptade drag kontrolleras på varje mellanposition; de tidigare 26 enstegsövningarna och sparfilens index är oförändrade. Kör helt offline utan extra sökbelastning.

- Etapp 13: Tre nya kombinationsövningar (torngaffel, dubbelhot med dam och springargaffel) utökar schackskolan till 26 tvåspråkiga övningar. Varje lösning får ett regel- och hotprov.
- Kunskapsprofil visar fyra områdens avklarade övningar, lyckade repetitioner och fel. Rekommendationerna prioriterar fortfarande oavklarade övningar, men för avslutade moment vägs tidigare misstag mot lyckade repetitioner.
- `progress.txt` lagrar lyckade repetitioner på en egen tredje rad. Läsaren hanterar också äldre en- eller tvåradiga sparfiler och äldre övningsantal genom att läsa varje rad separat. Programmet arbetar helt lokalt, utan nya tjänster, extra grafikytor eller nätverksanrop.

- Etapp 12: Schackskolan delar in 23 övningar (20 befintliga och tre nya) i Grunder, Taktik och Fördjupning. Fel per övning sparas efter den äldre avslutsmasken i `progress.txt` och gamla sparfiler kan fortfarande läsas. Rekommenderad övning väljer först olösta moment med flest misstag; när alla klarats repeteras svagaste momentet. Efter två misstag visas en uppgiftsspecifik ledtråd. Tre taktiska övningar tillkommer: avdragsschack, bindning och spetstaktik, med uppgiftsbeskrivning, ledtråd och återkoppling på båda språken. Inga nya grafik- eller nätverksberoenden.

- Etapp 11: ny knapp för att granska det senast spelade draget. En bakgrundstråd söker maximalt 8 000 positioner från föregående ställning; resultatet visar dragets observerbara egenskaper och eventuellt ett annat alternativ, utan att kalla draget ett bevisat misstag. Svenska och engelska.
- Nybörjarnivån väljer slumpmässigt bland högst fyra drag som föredras av en billig positionsbedömning, i stället för bland samtliga lagliga drag. Övriga nivåer och deras sökbudgetar oförändrade.
- Rättat regressionstest för bondeförvandling: provet använder nu löparförvandling eftersom damförvandlingen i testställningen samtidigt gav schack.

### Tidigare etapper
- Etapp 10: knappen för dragförslag ger en kort tvåspråkig pedagogisk förklaring av observerbara egenskaper (schack, matt, slag, rockad, bondeförvandling, pjäsutveckling och centrum). Förklaringen är ingen garanti för att draget är optimalt. Schackmotorns sökbudget och fem nivåer är oförändrade.

- Etapp 9: pjäserna har ytterligare läsbara Cairo-vektordetaljer, och drag kan glida 160 ms utan extra bildytor. Rörelse stängs av vid WenuyOS `motion=off`, lågprestandaprofil eller `WENUYOS_PERF=low`; animation omfattar också LAN-drag och avbryts vid återställning/ångra.
- Etapp 8: dynamiskt tillgänglighetsnamn för brädet (ruta, pjäs, färg, vald ruta), tydlig GTK-beskrivning av tangentbordet samt tillgängliga LAN-fältnamn.

- Språkstöd flyttat till WenuyOS egna libwenuy/gettext-domän och standardiserat ljust/mörkt tema via `wenuy::loadStyle()`.
- Nya CTest-kontroller av svenska/engelska och engelskt reservspråk när översättningsfil saknas.
- Drag och avslutade partier använder WenuyOS valda systemljud även vid LAN-spel.
- Ny sexsiffrig parningskod i LAN-läget: slumpas på värddatorn och måste anges av motspelaren; okrypterat och inte kryptografiskt säkert.
- Första spelbara LAN-läget: två WenuyOS-datorer på samma betrodda nät, värd vit och ansluten svart. Inget nätverk aktiveras utan knapptryck.
- TCP/IPv4 med privat adressfilter, protokollversion, lagliga drag och åtgärder för att avbryta eller koppla från; okrypterad testfunktion.
- Automatiskt test av två nätverksparter över loopback och separata instruktioner i `docs/LAN-050.md`.
- Nytt fokusläge för brädet med knapp och Ctrl+L, utan att dölja viktiga spelkontroller.
- Sidopanelen rullbar på små skärmar; flyttade sekundära verktyg till egen rad.
- Fjärde brädtema med hög kontrast.
- Tydligare markeringsram kring vald pjäs och separat tangentbordsmarkör.
- Statusfältet berättar vilken pjäs som är vald och antalet lagliga drag, på svenska eller engelska.
- Förfinade sex vektorritade pjästyper med konturlinjer, detaljer och en tydligare bas, utan rasterbilder eller oskärpa.
- Mindre minsta bräd- och sidopanelstorlek, mer kompakt standardfönster och verktygsmarginaler.
- Responsiv layout som staplar bräde och rullbar sidopanel under 780 px fönsterbredd; kompakt verktygsrad under 520 px.
- Färger för programfönster och panel följer GTK/WenuyOS temat; brädfärgerna behåller användarens val.
- Kort brädstatus visas även när sidopanelen är dold, med text som kan läsas av hjälpmedel.
- Nya regressionstester för GUI-resurser och installationsmetadata.


## 0.4.0 (under utveckling)
- Utökad schackskola till 20 interaktiva övningar, med nya taktiska och grundläggande slutspelsmoment.
- Förbättrad språkhantering i spelklienten (svenska och engelska).
- Förnyade regressionstester och verifierad 2 GB-profil före godkännande.

## 0.3.0 (2026-11-11)
- Nytt tema för den GTK4-baserade klienten: nordiskt blått, ljust och trä.
- Stöd för att dra pjäser och markera rekommenderat drag.
- 15 interaktiva lektioner och permanent lagrade övningsframsteg.
- Begränsad, iterativ AI-sökning med återgång till senaste helt sökta nivå.
- Tydligare status och materialbalans, fönstergenvägar och atomisk autosparning.
- Samma app-id som 0.2.1 för uppgradering i WenuyOS.

## 0.2.1 (2026-11-10)
- Första körbara GTK4-runtime-kompilerade .wapp-klienten.

## 0.2.0 (2026-11-10)
- Interaktiv schackskola med 9 övningar.