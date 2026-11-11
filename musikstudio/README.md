# Wenuy Studio – Musikstudio

C++17-musikstudio för WenuyOS. Import och vidareutveckling av 0.7/0.8 pågår i denna utkast-PR.

Det här är **källkod**, inte en godkänd eller signerad utgåva. Prov av GUI i Debian 13, 2 GB-profil och fysisk ljudhårdvara återstår.

Bygg och prov: `cmake -S . -B build && cmake --build build -j2 && ctest --test-dir build --output-on-failure`. Inga hemligheter, nätverksanrop eller byggda binärer ska föras in.
