# Matte – WenuyOS

**Status: import pågår i utkast-PR.** Allt här ska vara källkod som genomgått granskning och tester innan sammanslagning. 

- Målplattform: Debian 13, GTK4/libadwaita, C++17; WenuyOS på 2 GB RAM utan GPU.
- Tillgänglighet: svenska och engelska, lässtöd och tangentbordsnavigering.
- Elevdata sparas utanför programmappen, i `~/.local/share/se.wenuyos.app.matte/`.
- Nätverket används inte av programmet utan användarens uttryckliga val.
- Ingen byggd `.wapp`, `.zip` eller `.so` får checkas in i Git.

Se [ärende #9](https://github.com/danielwoxne/wenuyos-program/issues/9). Fullständig kod och testresultat läggs i detta PR före granskning.
