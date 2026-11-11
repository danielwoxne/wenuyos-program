# Testprotokoll 0.4.1 – 2026-11-10

| Test | Utfall / begränsning |
|---|---|
| GCC/CMake Release-kärna + CLI (C++17, `-Wall -Wextra -Wpedantic -Werror`) | Godkänd |
| CTest Release | 10/10 godkända |
| CTest AddressSanitizer + UndefinedBehaviorSanitizer (`detect_leaks=1`) | 10/10 godkända |
| `buffer041_spsc_and_simulated_callback` | Godkänt: wraparound, ordning, nollfyllning, två trådar, tre profiler, simulerat ljudkort och framkallat underrun |
| 12 repetitioner av bufferttestet | Godkända |
| WAV-export `Startprojekt.wstudio` | Godkänd: PCM16 stereo 48000 Hz |
| CLI-export, högsta RSS, Linux x86-64 | 2176 KiB (en liten exportprocess, inte hela musikprogrammet) |
| CLI-uppspelning med simulerad ljudutgång, stabil profil | Genomförd, 0 underruns i det enkla testet |
| GTK4/libadwaita-kompilering | **Inte testad här:** utvecklingspaketen saknas |
| Riktig PortAudio/PipeWire/ALSA utgång och mikrofon | **Inte testad** |
| Max 2 GB RAM och ljudavbrott under full last | **Inte testade på målmaskin** |

Observera att alla enhetstester ovan använder en *testdubbel* av `libportaudio.so.2`. Den får aldrig ingå i levererad `.wapp`. En kort simulering med 0 avbrott säger inte att riktig uppspelning blir avbrottsfri. GUI och enhetslatens kan inte påstås vara verifierade.
