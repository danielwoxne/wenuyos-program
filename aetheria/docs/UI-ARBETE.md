# Aetheria – användargränssnitt, säker överföring

**Syfte:** En tydligare, resurssnål HUD och läsbarare dialoger/menyer i spelet. Ändringarna avser enbart Aetheria, inte WenuyGFX eller WenuyOS.

**Källstatus (2026-11-12):** Senast direkt tillgängliga verkliga källpaket är **aetheria-0.5.0-src.zip** i Dropbox (NYA WENUYOS/Programvara/Aetheria). Aetheria **0.6.0 har redan skapats i en tidigare chatt**, med navigering, musklick, kamerazoom och autosparning, men dess källfil är inte tillgänglig för den här GitHub-grenen. Den tidigare 0.6.0 måste jämföras och återinföras innan denna gren får godkännas/släppas. Inget användardata/sparformat ändras.

## Den här PR:ens avgränsning
- Ta in 0.5.0-källan som **referensbas**, inte som uppdatering som ska ersätta 0.6.0.
- Förenkla informationshierarkin: karaktärens status, aktuellt uppdrag och minimap överst, färre textrader över spelvärlden.
- Visa tydliga knappar med tangentbordsnamn längst ned; ge paus, karta och väska läsbara stängningshänvisningar.
- Förbättra kontrast och justera placering för 960×600-logisk yta/skalning.
- Lägg till isolerade regressionsprov av layout/knappområden. Befintliga tre modellsviter fortsätter att köras.
- Båda språk, offline, ingen ny tjänst, inget nytt SDL/Wayland-beroende, soft/low som standard.

## Leveranskontroll
1. Bygge/test på Debian 13 i CI enligt Program-kontrollen.
2. Skärmbild och manuell visuell kontroll i low/soft, inventarium och dialog.
3. Källjämförelse/återbasering på verklig 0.6.0 innan merge.
4. Granskning av Claude, godkännande av Daniel. Ingen release, tagg eller merge i detta steg.

**Felsäkerhet:** Om äldre källkod/utdragsformat inte kan byggas korrekt, håll PR som utkast och använd ändringarna som patchförslag, inte som ny version av spelet.
