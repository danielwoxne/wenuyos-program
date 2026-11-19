# Android-appar

Android-appar i WenuyOS: Waydroid kör riktigt Android (LineageOS, utan Googles tjänster) i en container som delar
datorns Linux-kärna. Varje app öppnas i ett eget fönster, syns i sökfältet och startar bara när den används.

Förut en del av systemet; sedan 20 nov 2026 ett eget program (Daniel 19 nov: "Jag väljer alternativ 2", New-wenuyos
förslag #351). Systemet behåller bara den låsta delen som behöver root: `/usr/lib/wenuyos/wenuyos-android-root`
(fasta kommandon), sudo-regeln, vilolägeskroken och `wenuyos_android_split.py`.

| | |
|---|---|
| `main.py` | fönstret (förut Inställningar → Android): aktivera, visning, mus, delade mappar, minne, grafik, appbutiker, underhåll |
| `bin/wenuyos-android` | allt arbete – samma kommandon som förut (`wenuyos-android help`) |
| `lib/wenuyos_xapk.py` | appar i flera delar (.xapk, .apks, .apkm) |
| `share/applications/` | Helskärm, Fönster, Storlek, Stäng och "Öppna i Android" – skrivs till Start när programmet öppnas (`wenuyos-android setup`) |
| `tests/` | `bash tests/run.sh` – inget Android behövs |

**Nätverk:** Aktivera hämtar Waydroid (Debian, trixie-backports) och Android (~800 MB) i ett terminalfönster som visar
vad som händer – eller tar Android från ett USB-minne. Inget hämtas utan att du trycker.

**2 GB:** Android startar först när en app öppnas och pausas en minut efter sista fönstret. Fönstret läser statusen
bara medan det syns.
