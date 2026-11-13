# Mixerns nästa etapp – Wenuy Studio 0.9

**Utvecklingsordning:** bygger på polyfon realtidsuppspelning i PR #28 (som bygger på #27 → #24 → #23 → #21).

Syfte: göra den befintliga GTK4-mixern användbar utan att öka minnesförbrukningen nämnvärt. Arbeta med `Project.masterGain`, `Track.pan` och `TrackEffects.thresholdDb/ratio` som **redan finns** i projektkärnan. Inget nytt format, ljudbibliotek eller bakgrundsjobb.

## Etappens kontroller
- Huvudvolym (master gain).
- Panorering vänster–höger för varje spår.
- Tröskel och förhållande för den befintliga kompressorn, utöver redan befintlig EQ.
- GTK4-temat och svenska/engelska etiketter; tangentbordsnavigering och tillgänglighetsnamn.

## Prov före granskning
- Befintlig och ny testkörning av sparning, återläsning och mixning i stereo med olika pan-värden.
- `wenuyos verify` i Debian 13 med `WENUYOS_PERF=low` och `WENUYOS_GFX=soft`; grön Program-kontroll.
- **Ej provat ännu:** fysisk högtalare, skärmläsare, 1024×600 och 30 min på 2 GB-dator.

## Genomfört i utvecklingsgrenen
- Mastervolym 0–2 för `Project.masterGain`, med svenskt/engelskt namn.
- Panorering -1 (vänster) till +1 (höger) på varje spår, med mittmarkering.
- Kompressortröskel -48–0 dB och ratio 1–12, med tillgänglighetsnamn.
- Nytt CTest för projektsparning, återläsning och stereomix i befintlig `.wstudio` v5.
- Ändringar under pågående uppspelning kan kräva att uppspelningen startas om.
