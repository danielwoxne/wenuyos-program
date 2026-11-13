# Extern MIDI-inmatning – steg efter virtuella keyboardet

Ärende #26. Detta steg byggs i egen gren ovanpå [PR #27](https://github.com/danielwoxne/wenuyos-program/pull/27) och får inte tas in i main före beroendekedjan #21 → #23 → #24 → #27.

## För användaren
- Tryck **Sök MIDI-enheter**, välj en lokal MIDI-källa, och välj **Anslut**. Ingen automatisk anslutning, inga nätverkssökningar eller öppna TCP-portar.
- Spela med externa USB-MIDI-tangenter. Note On/Off går genom samma `pressKeyboard`/`releaseKeyboard` och `MidiTake` som det virtuella keyboardet. Velocity för anslag bevaras i det inspelade materialet.
- Välj **Koppla från** för att stänga ALSA-prenumerationen. Vid frånkoppling eller fel frigörs alla toner som enheten håller.
- Onscreen- och datorkeyboard fungerar som förut även utan ALSA Sequencer eller MIDI-enhet.

## Arkitektur och 2 GB
- Linux ALSA Sequencer (inte en egen MIDI-protokolltolk över USB): `snd_seq_open`, lista lokala källportar, `snd_seq_connect_from` först efter uttryckligt knapptryck.
- ALSA SDK upptäcks valfritt via `pkg-config alsa`. Ingen ny bakgrundstjänst. Ingen polling när inget är anslutet: filbeskrivare bevakas händelsestyrt i GTK/GLib.
- Låg minnesbudget och begränsad händelsebatch per GLib-dispatch (högst 128). Begränsa antalet synliga portar till 128.
- Audio-preview använder PortAudio som förut; extern MIDI fungerar för inspelning även om ljudutgång saknas.

## Prov och granskning
- Enhetstester för normalisering av Note On med velocity 0, Note Off, controller All Notes Off, ogiltiga kanal- och tonnummer.
- CI i Debian 13: `wenuyos verify`, fönsterstart på 2 GB/soft. Om ALSA-dev finns byggs den riktiga backendvägen; annars måste en fungerande stub byggas.
- **Ej bevisat av CI:** faktisk USB-MIDI-enhet/hotplug, latency, samtidiga noter via fysisk enhet, 30 min faktisk 2 GB-dator.
- Claude granskar oberoende innan merge. Daniels samtycke krävs för nytt släpp. Ingen nätverksfunktion tillkommer.