# Licenser och distributionskontroll – 0.3.2

| Komponent | Leverans | Licens / ansvar |
|---|---|---|
| Wenuy Studio: C++, GTK-gränssnitt, CLI, dokumentation, tester och egen syntgenererad demo | Källkod i detta paket | GPL-3.0-or-later (`LICENSE`, SPDX-kommentarer) |
| `third_party/libwenuy/` | SDK-källa från WenuyOS 2026-11-10 | Apache-2.0 enligt medföljande `LICENSE`, behåll notices vid distribution |
| PortAudio v19 | **Inte** bundlat; laddas dynamiskt via `libportaudio.so.2` | MIT-liknande PortAudio-licens. Källa: https://portaudio.com/license.html. Om det senare paketeras, kopiera dess copyright/licenstext |
| GTK4 och libadwaita | Runtime/byggberoende, inte bundlat | LGPL-2.1-or-later enligt respektive upstream; behåll upstream-villkor vid distribution |
| glibc, libstdc++, ALSA/PipeWire-beroenden via OS | Systemberoenden, ej packade här | Respektive paketvillkor; kontrollera distributionsgränsen |
| fake PortAudio-testbibliotek | Egen källkod för CTest, **inte att distribuera som produktionsdrivrutin** | GPL-3.0-or-later |

Några kommersiella ljud, tredjepartssamplingar eller proprietära plugins ingår inte. Exempel-WAV är syntgenererad av Wenuy Studios första ljudmotor.

## Checklista innan binär distribution

1. Bygg och verifiera på WenuyOS, samla `ldd` och tredjepartsversioner.
2. Kontrollera att runtime (`libportaudio2`, GTK4, libadwaita) finns eller ange rätt installerbara beroenden i `.wapp`.
3. Om binär distribution görs: behåll GPL och den tillhörande tillgängliga källkoden, även Apache-2.0-text för `libwenuy`.
4. Lägg aldrig in testbiblioteket `libportaudio.so.2` från byggträdet som användarens ljuddrivrutin.
5. Granska ny plugin- och samplebanklicens separat innan de inkluderas.

## Förändring i 0.3.3

`Recovery` och `MediaCheck` är egen C++17-kod under GPL-3.0-or-later. Inga nya externa bibliotek, bild-, ljud- eller fonträttigheter tillkommer. Exempelljud i paketet är ärvt från tidigare projektversion – kontrollera ursprung och distributionsrätt separat innan vidarepublicering.

## Ändringar i 0.4.0

BlockMixer, mixerreglage, GTask-baserad vågformsanalys, projektformat v3 och de nya regressionstesterna är utvecklad egen kod under GPL-3.0-or-later. Inga nya tredjepartsbibliotek, samples eller nya grafikresurser ingår. PortAudio används fortfarande via en systeminstallerad dynamisk runtime och testdubbeln ska inte distribueras till slutanvändare. Verifiera originalets SDK-notices och faktiska beroendevillkor mot den WenuyOS-installation där programmet paketeras.

## Ändringar i 0.4.1

`AudioBuffer`, callback-integrationen, buffertprofilerna, UI-valet, fake PortAudio-backendens callbacktest och dokumentation är ny egen källkod med SPDX `GPL-3.0-or-later`. PortAudio v19 laddas fortsatt via systemets `libportaudio.so.2` och distribueras **inte** i källkodspaketet. Bundlat `libwenuy` har separat Apache-2.0-text. Inga nya samples, instrument, ljudbanker, tredjepartskod eller typsnitt tillförs. Innan riktig `.wapp` distribueras: granska tillämpliga systemberoendeversioner och deras licenstexter på WenuyOS.
