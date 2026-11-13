# Musikstudio 0.9 – instrumentbibliotek (etapp A)

[Ärende #20](https://github.com/danielwoxne/wenuyos-program/issues/20). Enbart en patch mot senaste programförrådets `main`.

## Acceptans
- Exakt **16** inbyggda, små, **syntetiserade klangpresets** med svenska/engelska namn, tillgängliga per spår i GTK4.
- Gamla instrument-ID **0–3** är oförändrade; `.wstudio` **v5** kan öppnas av äldre Musikstudio när enbart 0–3 används. Projekt med ID 4–15 kräver en ny version, men själva filformatet ändras inte.
- Nytt ljud skapas av C++17-oscillatorer i befintlig mixer. Ingen samplerfil, nätåtkomst, instrument-plugin eller ny bakgrundstjänst.
- Kontroller för 2 GB: begränsade övertoner, Nyquist-filter, max 128 spår, inga samplebuffertar.
- Samma instrumentval vid WAV-render, PortAudio-uppspelning och projektöppning.
- CTest provar 16 ljud, WAV-output, gamla ID, felaktiga ID och spar/ladda.
- Systemets **Datatyper** integreras i nästa separat granskade steg för fler ljudformat; **WenuyGFX** endast vid reellt 3D-visualiseringsbehov och uppmätt overhead.

## Ej del av första PR
- Realistiska inspelade pianon, gitarrer, SF2/SFZ, licensberoende ljudbanker.
- Tillägg av effekter i ljudtråd, nya projektfält, 3D-fönster, publicering.

## Tonformning, etapp 3 – mer levande klanger utan samplingar
- Piano (ID 4), elpiano (5), plockad gitarr (7), elbas (8) och marimba (15) använder snabbare övertonsdämpning under tonsvansen. Det ger skarpare anslag och mjukare efterklang utan externt ljudmaterial.
- Tidigare ljud-ID 0–3 och samtliga 16 ID-nummer bevaras. `.wstudio` är fortfarande version 5; äldre projekt behöver inte konverteras.
- Ingen extra minnesallokering eller bakgrundstråd. Enkel aritmetik styr dämpningen, i stället för fler `sin`/`exp` per sampel.
- Enhetstester täcker syntetiserade transienta klanger och gamla ID. Lyssning och belastning på fysisk 2 GB-dator återstår.
