# Version 0.7.0 – fullständigt fristående provbygge

## Implementerat

- Körbart musikstudiofönster för X11/XWayland, byggt på C++17.
- Flera arbetsytor: arrangemang, pianorulle, mixer och FX.
- Fyra syntmodeller, wav-import, MIDI-import/export, audio rendering, användarstyrd loop.
- Mixer/effekter från 0.6, sequencer/MIDI från 0.5, WAV-strömning och projektformat v5.
- Ångra/gör om för fullständig session är **inte implementerat** i X11-kompatibilitetsgränssnittet. Core-editing är icke-förstörande.
- Mikrofoninspelning till separat temporär fil med säker no-replace-finalisering och infogning i spår.
- Autosparad återställningsfil bredvid projektet var 45:e sekund vid osparade ändringar.
- Förbättrade regressionstester och SDK-kompatibel `.wapp`-paketering.

## Kvalitetsgrind

Kärna/CLI och X11-fönster har byggts för x86-64 i Debianlik utvecklingsmiljö. 0.5-, 0.6- och 0.7-kärntest ingår. Test mot riktig WenuyOS/Wayland-kompositor, verklig ljudinspelning och den långsiktiga GTK4-versionen återstår.
