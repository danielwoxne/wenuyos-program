# Testplan Aetheria 0.3.0

1. Bygg i ren Debian 13, även på riktiga WenuyOS om möjligt.
2. Kör `ctest --test-dir build --output-on-failure`. Kontrollera uppdrag,
   sparning, ogiltig sparning, kamera-relaterad rörelse och hopp.
3. Starta med `LANG=sv_SE.UTF-8`; UI ska visa svenska när `LANGUAGE`/`LANG`
   börjar med `sv`.
4. Starta med `--frames 60`, `--2d --frames 60` och `--high-detail --frames 60`.
5. Starta `--screenshot /tmp/aetheria.ppm` och kontrollera 960 × 600 bild
   samt att ruiner, spelare, växter och HUD syns.
6. Prova Z/C för kamerarotation och WASD; kontrollera att riktningsmappningen
   följer kameran och inte tappar kollision.
7. Tryck F, håll G under nedgång; orken ska sjunka. Släpp och landa.
8. Tryck F2 medan spelet körs; logisk FPS/musrespons ska inte försvinna.
9. Gör färdigt kristall- och fiendeuppdragen. Aktivera altaret med E.
10. Testa F5, avbryt start, F9; behåll XP, kristaller och position.
11. Migrera från 0.1.x: lägg giltig `wenuy-legends/save1.txt` i gamla
    sparfilskatalogen och radera nya filen; starta och bekräfta kopian.
12. Öppna `.wapp` med WenuyOS-installationsprogrammet, kontrollera startmenyikon,
    beroendeinstallation, avinstallation och skrivbehörigheter.
13. Låt körningen stå i 30 min på CPU utan GPU; kontrollera RSS, CPU, värme,
    pekarfördröjning, kraschlogg och valfri 2D-fallback.

**Ej genomfört i detta värdbygge:** punkterna som kräver en fysisk WenuyOS-enhet,
handkontroller, temperaturövervakning och långtidsstabilitet. Den syntetiska
skärmbilden är däremot tagen från den körande SDL2-renderaren.

## 0.3.0 — tilläggstestning

Automatiska prov i `tests/test_model.cpp` omfattar nu:
- Samtliga genererade träds position är blockerande, uppförslutningsspärr.
- Rörelseanimation drivs bara av faktisk förflyttning.
- Dagsljuskurvans gränser och återkomst, samt äldre sparfil inne i nytt träd.

Manuellt speltest på WenuyOS (ännu inte utfört):
1. Gå/spring mot träd och branta sluttningar; försök gå runt.
2. Byt riktning och observera ben och kappa; hoppa, glid och attackera.
3. Låt spelet vara igång 4 minuter: dag, skymning, natt, gryning.
4. Kontrollera att eldflugor visas nattetid, fjärilar dagtid och NPC:er
   är synliga (de ska inte uppfattas som färdiga samtalspartners).
5. Växla F2 och V, ändra fönsterstorlek och helskärm.
6. Öppna en äldre sparfil och provspela hela kristall-/väktaruppdraget.
7. Mät CPU-belastning, FPS, RAM och inmatningsfördröjning under 30 minuter.


## Etapp 4 regressionsprov

- [x] `ctest` — både den gamla modellsviten och scen-4-sviten passerar.
- [x] Testa öppna/stäng dialog, pausa spelarlogik medan dialog visas.
- [x] Testa sidouppdragets alla stadier, belöning och läkarens engångsläkning.
- [x] Läs gammal sparfil; läs ny sparfil; avvisa ogiltig extension.
- [x] Kontrollera varningar innan träff, undvikbar träff och återvändande fiender.
- [x] Prova eldskada över tid.
- [x] Kontrollera NPC- och örternas koordinater mot fysisk kollisionsmodell.
- [x] Starta 60 bildrutor i låg/hög detalj med SDL2.
- [ ] Riktigt tangentbordsprov med 30+ minuter spelande på WenuyOS.
- [ ] Tillgänglighet med stor text, handkontroll och varierad skärmstorlek.

## Etapp 5 — regressioner och praktiskt prov

- [x] Första två äldre testsviter samt en tredje testsvit för inventarium.
- [x] Dialogval 1/2, avstå från uppdrag och läkare, sparning.
- [x] Tillverkning, förbrukning av material, förbättrad svärdsskada och ork.
- [x] Läs v4-sparning utan v5-extension; förhindra ändring vid korrupt fil.
- [x] Starta riktiga SDL-spelvyer i virtuell display och spara skärmbilder.
- [ ] Provspela hela progressionen med tangentbord på WenuyOS-hårdvara.
- [ ] Mäta 30 min CPU, fläkt och RAM på 2 GB RAM / gammal processor.
- [ ] Testa ljudkort, maskin utan ljudkort, samt M/ljud av när spel kör.
- [ ] Kontrollera faktisk `.wapp`-installation i WenuyOS.
