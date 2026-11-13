# Wenuy Schack 0.5.1 – provplan före godkännande

## Omfattning
0.5.1 är en begränsad GUI-kvalitetsuppdatering. Kontrollera att den **inte**
ändrar spelregler, de 31 schackövningarna, sparfilerna, valfritt LAN eller
WenuyOS tema-/ljudinställningar. Alla användartexter ska finnas på svenska
och engelska.

## Automatiska prov (i Debian 13)
1. `wenuyos verify --project schack` via **Program** i GitHub Actions.
2. `chess_rules`, `lan_loopback`, `ui_assets` och `system_i18n`.
3. GTK-fönstret ska visas med 2 GB-miljö och mjukvarurendering.
4. Dokumentera tid för fönsterstart och uppmätt start-RSS från aktuell CI
   och jämför med version 0.5.0 (55 MB / 193 ms i Claudes Debian-prov).
5. UI-resurstestet kontrollerar både GTK-policy för **ingen vågrät rullning**
   och radbrytning av svensk och engelsk hjälptext. Detta är inte en
   ersättning för en skärmbild eller praktiskt GUI-test.

## Manuell 1024×600 och 1280×720
1. Starta i `WENUYOS_PERF=low WENUYOS_GFX=soft` på en fysisk maskin med
   2 GB RAM om tillgänglig. Testa även smalare, omstorleksbara fönster.
2. Skrolla genom hela sidopanelen i två språk, alla fyra brädpaletter,
   både vanligt parti, övning och LAN-fält. **Ingen vågrät rullist**, och
   ingen text eller kontroll får vara oåtkomlig eller kapas.
3. Läs hela genvägshjälpen, inklusive **Ctrl+L fokus** / **Ctrl+L focus**.
   Jämför 1024×600, 1280×720 och ett fönster under 780 px bredd.
4. Kontrollera att draglistan radbryts, att knappar för Föregående,
   Ledtråd och Nästa kan aktiveras, och att felmeddelanden fungerar.
5. Prova tangentbord: Tab, pilar, Enter, Escape, Ctrl+L, Ctrl+N, Ctrl+Z
   och Ctrl+H. Kontrollera att pilar inte flyttar brädfokus medan text
   skrivs i LAN-fälten. Med Orca: läs brädets status och hjälptext.
6. Fokusera också på att föreslagna drag och parningskodens varningar
   verkligen kan läsas med skärmläsare; notera begränsningar.

## Längre prestandaprov
1. Kör minst **30 minuter** på fysisk 2 GB/mjukvarurenderande dator:
   datornivå 1–5, omkring 30–50 drag, flera sidopanelsscrollningar,
   övningar, dragförslag och gränssnittsbyte.
2. Mät RSS vid start, efter 10, 20 och 30 minuter samt max-RSS, CPU och
   märkbar fördröjning hos muspekare/inmatning. WenuyOS verktyg
   `wenuyos prestanda schack` kan användas när det är tillgängligt;
   dokumentera annars faktiskt använda process-/mätkommandon.
3. Spela också efter växling mellan mörkt och ljust tema; säkerställ
   att ingen gradvis minnesökning eller långa frysningar uppstår.
4. Prova LAN på två datorer om möjligt – det är **inte** verifierat
   av en ensam lokal loopback-körning.

## Resultat och överlämning
Skriv i PR #12 vad som verkligen provats och vilka punkter som återstår,
med skärmbilder eller mätning där det finns. Låt Claude göra en oberoende
granskning, rätta i samma PR, och lämna sammanslagning och signerat släpp
till Daniels uttryckliga godkännande.

Ingen .wapp eller hemlig signeringsnyckel får hamna i Git.
