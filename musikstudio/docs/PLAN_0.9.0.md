# Musikstudio 0.9 – etapp 1: interaktiv pianorulle

Kopplat till [ärende #17](https://github.com/danielwoxne/wenuyos-program/issues/17). Föregående signerade 0.8.1 i `main` lämnas orörd tills Daniel godkänner den här PR:en.

## Användarfall

- Klicka i en tom pianorulle för att infoga en ton där pekaren står. Rytm: 1/16-noter (fyra steg per fjärdedel), tonhöjd: C2–C6 enligt befintlig skala.
- Klicka i en ton för att välja den; högerklick eller Delete tar bort vald ton; piltangenter flyttar vald ton i semiton/rutsteg.
- Ångra och Gör om ska fungera för varje ändring och inga dubbletter skapas när man klickar igen på samma rutsteg.
- Utanför rutnätet ska inget infogas. Tidslinjeklick flyttar endast markören.
- Synliga förklaringar på svenska/engelska; inget extra beroende eller nätverk.

## Verifiering som krävs

- C++17-regressionstest: koordinat→ton/tid, randvärden, markera befintlig ton, ingen dubblett, borttagning, förflyttning, ångra/gör om.
- Befintliga kärnprov och ikon/MIME-prov i Program-CI utan regression.
- `wenuyos verify` under `WENUYOS_PERF=low WENUYOS_GFX=soft`; faktisk pek-/musinteraktion och mikrofon på fysisk maskin anges separat.
- Kontrollera export/import av MIDI efter noter lagts in.

## Ej del av denna PR

Osparat-arbete-dialog vid Nytt/Öppna/Stäng, längre PortAudio-prestandaprov, LV2-instrument och publicering.
