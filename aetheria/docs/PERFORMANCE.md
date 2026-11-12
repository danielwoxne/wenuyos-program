# Prestandaprov — Aetheria 0.3.0 (2026-11-10)

Körning: Linux x86-64 i isolerad värdmiljö med SDL2:s `dummy`-videodrivrutin,
programvarurendering och logisk upplösning 960 × 600. Byggt med `-O3`/CMake
Release och testat med `--frames 60`. SDL2:s 30 FPS-tak ingår i väggtiden.

| Prov | Väggtid / 60 bildrutor | Högsta residenta minne (RSS) | Utfall |
|---|---:|---:|---|
| Standard 3D, låg detalj | 1,97 s | 7 576 KiB | Godkänt |
| Hög detalj (`--high-detail`) | 1,98 s | 7 700 KiB | Godkänt |
| Natt 3D (`--preview-time 120`) | 1,96 s | 7 696 KiB | Godkänt |

Provet är ett kort funktionellt renderingsprov, **inte** en FPS-mätning på
WenuyOS. Då videodrivrutinen är en dummy mäter det inte verklig fönsterkompositor,
GPU-drivrutin eller faktisk latens, och innehåller nästan ingen användaraktivitet.
Minnet avser bara spelprocessen och inte SDL/system/skrivbordets totala RAM.
Därför kan vi inte garantera flytande 30 FPS eller 2 GB-stöd på en äldre dator.

SDL med virtuell X11-skärm (xvfb) kördes också i tolv bildrutor utan krasch.

## För nästa WenuyOS-prov

- Prova 2 GB RAM utan swap och 4 GB RAM med mjukvarurendering.
- Spela aktivt minst 30 minuter, med fiender, kamera och skymningsövergång.
- Mät p50/p95/p99 bildtid, CPU, total RAM, inputfördröjning, temperatur.
- Dokumentera renderers drivrutin, upplösning och använd detaljnivå.


## Etapp 4 (0.4.0), Debianvärd 2026-11-10

Prov: `--frames 60`, CMake Release, SDL2 via virtuell X11 och genom `dummy`
med mjukvarurendering. Resultaten är **inte prestanda på WenuyOS**.

| Prov | Väggtid | Max RSS (KiB) | Kommentar |
|---|---:|---:|---|
| `dummy` låg detalj | 1,96 s | 8 620 | Spelprocess, utan riktig fönsterkompositor |
| `dummy` hög detalj | 1,97 s | 8 844 | Spelprocess, utan riktig fönsterkompositor |
| Virtuell X11 låg detalj | 2,09 s | 84 948 | Xvfb-mätning inkluderar stödprocesser |
| Virtuell X11 hög detalj | 2,09 s | 85 620 | Xvfb-mätning inkluderar stödprocesser |

30 FPS-taket gör att tvåsekunderstesterna inte säger något säkert om uppnådd
maximal bildfrekvens eller tidsfördelning. Proverna simulerade nästan ingen
användarinmatning. Testa 30 minuters spel, kamera, dialog och strider direkt
på WenuyOS innan 2 GB RAM eller följsamhet kan anses verifierad.

## 0.5

Inga ljudfiler, texturer eller riggade modeller läggs i RAM.
Ljudmotorn skapar korta PCM-buffertar i samma process, utan nya beroenden
utöver redan länkade SDL2. Den går automatiskt i tyst läge utan ljudenhet.
Inventariets två paneler ritas endast när I hållit uppdateringen pausad.
Utrustningsrendering bidrar endast med ett fåtal extra polygoner.
