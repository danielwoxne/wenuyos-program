# Versionsanteckningar 0.4.1 (2026-11-10)

Ny ljudarkitektur med producer-workern `BlockMixer` och en separat PortAudio-utgångscallback via `AudioBuffer` (SPSC). Avsikten är att disk/fil-läsning inte ska blockera callbacken. Förfyllning sker före enhetsstart. Tre buffertprofiler: low (128 callback, 4096 ring), balanced (512, 8192), safe (1024, 16384). Underfyllda ljudanrop blir tystnad och räknas. GUI visar avbrott och buffertnivå samt låter användaren välja profil; CLI accepterar `--buffer=` och skriver diagnoser.

Projektformat fortsatt v3 (öppnar v2 också). Ingen `libwenuy`-ändring och inget nytt tredjepartsberoende. `Recording` och bildvisning är oförändrade. Alla tidigare tester samt nytt SPSC-, concurrency- och callbacktest ingår. En riktad testdubbel simulerar avbrott genom att efterfråga 8192 bilder i ett anrop; den distribueras inte.

Kvar att verifiera på en riktig installation: GTK-bygg med libwenuy, fysisk ljudutgång och mikrofon, ljudavbrott med många samtidiga spår, den faktiska användarupplevda latensen samt drift på 2 GB RAM. **Detta är inte en verifierad `.wapp`.**
