# Musikstudio – keyboard och MIDI-inspelning
Ärende #26, stackad ovanpå #24 (#23/#21). Inget nytt programberoende, släpp kräver Daniels godkännande.

## Steg 1: live-keyboard
- Två oktaver C4–B5 som tangentbordsstyrt och musklickbart GTK4-kontrollfält.
- Temaavledda kontraster; tillgängliga tonnamn, fokus och stöd för vanliga datortangenter.
- Omedelbar ljudförhandsvisning från befintlig PortAudio/InstrumentBank, oberoende av sequencerns huvudtransport.
- Programmet kan användas utan ljudenhet: visa fel kort, inte krascha.

## Steg 2: MIDI-take
- Monoton tidsmätning för anslag/släpp, flera samtidigt hållna toner; ingen datainsamling.
- Spela in på valt spår utan att spela över gammal musik. Låt ångra återställa hela tagningen.
- Stoppa/spara/stäng säkert: förhindra att aktiv inspelning tappas. Hårt tak 50 000 noter/projekt.
- Låg CPU/RAM vid stillastående, inget nätverk.

## Steg 3: samplingar
- Första PR:et importerar INGA samplingar. Fria samplingar behöver en separat proveniens-, licens- och minnesgranskning.
- Dokumentera källa, tillåtna ändringar/redistribution, tillskrivning, ljudformat och max arbetsminne innan integrering.

## Kontroll
- Nya C++17-tester för inmatning, noter, tider och överskridna gränser.
- Program-CI (Debian 13) med `WENUYOS_PERF=low`, `WENUYOS_GFX=soft`, `wenuyos verify`.
- Manuell live-lyssning med PortAudio, extern MIDI och 30 min hårdvaruprov redovisas som EJ PROVAT tills det utförts.
- Beroende PR-stack #21 → #23 → #24 → detta PR. Ingen merge före granskning och synkning.