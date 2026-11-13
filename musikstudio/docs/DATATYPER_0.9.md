# Musikstudio 0.9 – WenuyOS Datatyper för ljud (etapp B)

Stackad efter [instrument-PR #21](https://github.com/danielwoxne/wenuyos-program/pull/21); ingår i [plan #20](https://github.com/danielwoxne/wenuyos-program/issues/20). Släpps inte separat eller före Daniels godkännande. Koden kopieras inte från systemet.

## Mål
- Välj **Importera ljud** i Musikstudion. Det egna WAV-formatet fortsätter att fungera utan Datatyper.
- När SDK:t `pkg-config wenuy-datatypes` finns används `wenuy_datatype_load_audio` för till exempel FLAC, Ogg, MP3, AIFF, Amiga 8SVX. Stöd varierar med installerade formattillägg. Datatyper är valfritt på äldre system.
- Avkoda högst 64 MiB PCM tillfälligt, strömma till stereo-PCM16 WAV i `~/.local/share/se.wenuyos.app.musikstudio/imports` och ange den permanenta sökvägen i projektet. **Inte** använda osäkra, försvinnande temporära WAV-kopior.
- Ge ett begripligt svenskt och engelskt fel om SDK/format saknas. Inget nät/användarhemligheter.
- Inget nytt minneskrav i vila. Importen får använda högst en begränsad avkodningsbuffert och 512 ramar arbetsminne.
- Inget WenuyGFX-beroende; 3D-visualisator utreds i separat steg så 2 GB/soft-fönstret behåller nuvarande GTK4-ritning.

## Prov
- Bygg/kör i Debian 13 med respektive utan `wenuy-datatypes`.
- WAV-pass-through och fel vid saknad fil, avkodad stereo PCM, filer som överstiger taket, öppna projekt efter omstart.
- Bygg/fönsterstart `wenuyos verify` på 2 GB/soft; verkligt MP3/FLAC/8SVX-prov på WenuyOS med installerade tillägg.
- Riktiga högtalare/mikrofon och 8-spårsbelastning återstår.
