# Tredjepartslicenser / Third-party licences

- SDL2: programmet länkar dynamiskt till det separata systempaketet
  `libsdl2-2.0-0` (SDL2 2.x; zlib-licensen). Ingen SDL2-källkod eller
  SDL2-binär ingår i WenuyOS-programarkivet. Se https://libsdl.org/license.php
  och distributionens paket för exakta licensuppgifter.
- C++ standard library / libstdc++ och glibc: systemets körmiljöer är externa
  programberoenden; inga bibliotekskopior bäddas in i WAPP.
- WenuyOS SDK: en lokal paketeringsskriptkoppling används vid byggtid. Inget
  libwenuy har länkats eller distribuerats med detta spel.
- Projektets egna C++-filer, procedurgenererade spelgrafik, UI-text och
  spelets logik: MIT; se LICENSE.
- Inga kommersiella spels bilder, modeller, musik, dialoger eller kod ingår.

Den som distribuerar ett framtida paket med nya resurser behöver bokföra
licens, attribution och rättighetsinnehavare per tillgång.
