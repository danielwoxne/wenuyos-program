# Klockan (Clock Trainer)

Lär dig klockan steg för steg – analog och digital tid, på svenska och engelska. För WenuyOS (GTK 4, Python).

- **Sju nivåer**: hel timme, halv, kvart, fem minuter, varje minut, 24-timmarsklockan och "hur lång tid?".
  Varje nivå är tio uppgifter och ger en till tre stjärnor; nästa nivå öppnas när man klarat den förra.
- **Tre sorters uppgifter**: läs av klockan (välj svar), ställ klockan (dra i visarna eller använd knapparna)
  och räkna tid ("Vad är klockan om 25 min?").
- **Utforska klockan**: dra fritt i visarna och se tiden i ord och med siffror (03:00 eller 15:00).
- Fel svar förklaras ("Den långa visaren pekar på 6 – halv"), och felsvaren är de vanliga förväxlingarna
  (visarna omkastade, "halv tre" läst som 3:30, tolv timmar fel).
- **Lyssna** läser upp tiden om `espeak-ng` finns på datorn; annars visas inte knappen.

Inget nätverk. Framstegen sparas i en liten fil i användarens datamapp (`klockan/framsteg.json`).
Färger och typsnitt kommer från WenuyOS-temat; urtavlan ritas bara om när något ändras (ingen animation i vila).

## Prov

```bash
bash tests/run.sh        # 35 prov: tiden i ord (sv/en), vinklar, dragning, uppgifter, svarsalternativ, framsteg, filerna
```

Licens: GPL-3.0-or-later.
