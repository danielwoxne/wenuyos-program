# Klockan (Clock Trainer)

Lär dig klockan steg för steg – analog och digital tid, på svenska och engelska. För WenuyOS (GTK 4, Python).

- **13 nivåer efter grundskolans kursplan i matematik (Lgr22)**, grupperade per stadium med kursplanens ord på startsidan:
  - **Åk 1–3** – *"Jämförelser och uppskattningar av storheter." "Mätning av längd, massa, volym och tid med vanliga nutida
    och äldre måttenheter."*: hel timme, halv, kvart, fem minuter, varje minut, dygnets 24 timmar, hur lång tid,
    sekund–minut–timme–dygn–vecka–år (jämföra, omvandla, uppskatta hur lång tid något tar, kvart, decennium, sekel),
    veckodagar, månader och datum.
  - **Åk 4–6** – *"Jämförelse, uppskattning och mätning av … tid … med standardiserade måttenheter samt enhetsbyten i samband
    med detta."*: byta enhet (h ↔ min ↔ s, dygn, veckor, timmar i decimalform), tidtabeller (över hel timme och midnatt,
    nästa buss, dagar mellan datum), sekunder och tiondelar (vem var snabbast, skillnad, min och s).
  - **Åk 7–9** – *"Härledda enheter, till exempel km/h och kr/kg."*: hastighet, sträcka och tid, km/h ↔ m/s, timmar i
    decimalform från räknaren.
  Kursplanen anger centralt innehåll per stadium, inte i vilken årskurs varje del ska läras – nivåernas ordning och
  placering är programmets **förslag på progression**. Varje omgång är tio uppgifter och ger upp till tre stjärnor;
  alla nivåer går att välja från början. Datum räknas med ett vanligt år; uppgifter som passerar slutet av februari
  säger det ("ett vanligt år, inte skottår").
- **Sorters uppgifter**: läs av klockan (välj svar), ställ klockan (dra i visarna eller använd knapparna; på 24-timmarsnivån
  också förmiddag/eftermiddag), räkna tid och flerval om enheter, kalender, tidtabeller och hastighet.
- **Utforska klockan**: dra fritt i visarna och se tiden i ord och med siffror (03:00 eller 15:00).
- Fel svar förklaras ("Den långa visaren pekar på 6 – halv", "1 h = 60 min, så 1,5 h = 90 min"), och felsvaren är de
  vanliga misstagen (visarna omkastade, "halv tre" läst som 3:30, tolv timmar fel, 1,5 h = 1 h 50 min, 34 mars,
  12,4 s snabbare än 12,38 s, tidtabellen räknad som vanliga tal).
- **Lyssna** läser upp tiden med `espeak-ng` (finns i WenuyOS); saknas den visas inte knappen.

Inget nätverk. Framstegen sparas i en liten fil i användarens datamapp (`se.wenuyos.app.klockan/framsteg.json`).
Färger och typsnitt kommer från WenuyOS-temat; urtavlan ritas bara om när något ändras (ingen animation i vila).

## Prov

```bash
bash tests/run.sh        # 86 prov: tiden i ord, vinklar, dragning, alla uppgifter räknade efter, kursplanens ord, framsteg, filerna
```

Licens: GPL-3.0-or-later.

Kursplanen: Skolverket, Läroplan för grundskolan (SKOLFS 2010:37 i senaste lydelse, Lgr22), kursplanen i matematik,
centralt innehåll om tid. Nya läroplaner börjar gälla hösten 2028 – då ses nivåerna över.
