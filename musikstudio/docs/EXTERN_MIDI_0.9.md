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
## Implementerat i PR #28

- `src/MidiInput.cpp` använder ALSA Sequencer, `snd_seq_connect_from` med användarvalt `client:port`, och GLibs FD-bevakning (`g_unix_fd_add_full`) i huvudloopen. Ingen egen tråd, ingen kontinuerlig polling i viloläge.
- Portar med `READ` + `SUBS_READ` väljs, upp till 128 poster. Den egna mottagarporten har `NO_EXPORT`; programmet accepterar bara händelser med den uttryckligen valda källans client/port.
- `Note On` / `Note Off`, Velocity 0 som Note Off, `Controller 120/123` och USB-bortkoppling/frånkopplad ALSA-port lösgör externa toner. Spårets befintliga `MidiTake` spelar in verkliga tangentlängder och velocity.
- `GtkStudio.cpp`: **Sök MIDI-enheter**, välj källa, **Anslut MIDI/Koppla från MIDI**. Lokala textfält och felsvar på svenska/engelska. Digitalt piano och extern MIDI har separata tangenttillstånd; externa toner utanför C4–B5 kan fortfarande spelas in.
- `wenuystudio_midi` använder systemets libasound endast när `pkg-config alsa` finns; `wenuystudio_midi_stub` och dess regressionstest verifierar reservvägen utan ALSA. Debians `libasound2-dev` är deklarerat som byggpaket och `libasound2t64` som körpaket för denna version.

## Prov på riktig hårdvara som återstår

1. Anslut ett USB-MIDI-keyboard och testa **Sök → Anslut**. Enheten ska vara synlig på ALSA-källistan utan automatiska anslutningar.
2. Prova både C4 och en låg/hög ton utanför C4–B5. Lyssna på förhandsljudet, starta MIDI-tagning och kontrollera velocity samt tonlängd efter Stoppa och spara/ladda.
3. Håll två tangenter samtidigt, släpp en före den andra. Kontrollera att båda syns som separata noter. Prova även datorns tangentbord på samma ton.
4. Koppla loss USB-kabeln medan en tangent är nedtryckt. Inga fastnade toner får ligga kvar; koppla in enheten igen och anslut med nytt synligt val.
5. Prova felaktig/en återanvänd klientport och frånvarande `/dev/snd/seq`: användarvänligt fel, inte krasch. Gör motsvarande prov med kompilering utan ALSA-SDK (virtuellt keyboard ska fungera).
6. Mät på fysisk 2 GB-dator: start-RSS, max-RSS, CPU över 30 minuter, ingångslatens och ljudhack i 2 GB/soft. CI mäter bara uppstart, inte långtidsprestanda.
7. Testa skärm 1024×600 och 1280×720, svenska/engelska och läsbarheten i MIDI-källväljaren.

**Inte implementerat i denna etapp:** Sustain CC64, pitchbend/aftertouch, kanal-separata noter, MIDI 2.0 UMP, lyssning via flera synt-röster samtidigt och MIDI-nätverk. Ingen signering, merge eller release före projektets granskning och Daniels releasegodkännande.
