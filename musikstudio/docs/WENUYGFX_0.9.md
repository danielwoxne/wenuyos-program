# WenuyGFX 0.9 i Musikstudion – valbar 3D-notkarta

[Plan #20](https://github.com/danielwoxne/wenuyos-program/issues/20). Stackad på [PR #23](https://github.com/danielwoxne/wenuyos-program/pull/23), som i sin tur kräver [PR #21](https://github.com/danielwoxne/wenuyos-program/pull/21).

## Beslut
GTK4 fortsätter att rendera allt vanligt gränssnitt och pianorullen. `WenuyGFX` är en **3D-grafikmotor** och vore olämplig som ersättning för ett resurssnålt 2D-musikfönster. I stället införs ett separat, valfritt **verktyg för en 3D-karta över noter i ett sparat projekt**.

- Byggs **endast** om systemets `find_package(WenuyGFX 0.9 CONFIG QUIET)` hittar det versionsmärkta SDK:t.
- Länkar enbart det särskilda renderingsverktyget mot `WenuyGFX::wenuygfx`, inte musikens huvudprogram och inte ljudmotorns tråd.
- Kräver ingen skärm/GL-drivrutin: `wenuygfx::SoftwareRenderer` kan rita en bild direkt från projektets noter till PPM.
- Körs endast efter uttryckligt användarval. Ingen animering, nätaktivitet eller bakgrundstjänst.
- `WENUYOS_PERF=low`: högst 320×180 med högst 256 noter, högst ca 0,3 MB pixelbuffert plus liten z-buffert. Annars högst 640×360.
- Projektfiler och inspelningar påverkas inte. En visuell karta har inget med ljudkvalitet att göra.

## Prov
C++17/CTest som bygger/rasteriserar exempelprojekt utan GPU och skapar PPM. Valfri modul får aldrig göra huvudprogrammets start beroende av libwenuygfx. Testa om SDK saknas, på 2 GB/soft och på riktig GPU efter oberoende granskning.

**Ej en live-visualizer.** Den första etappen skapar en statisk 3D-karta med kommandot `wenuy-studio-gfx <projekt.wstudio> <bild.ppm> [--low]` för extern förhandsvisning. Integrering med GTK:s Bilder/Datatyper och live-ljudvisualisering är separata, mätbara framtidssteg.
