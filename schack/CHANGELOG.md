# Versionshistorik

## 0.5.0 (under utveckling)
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